// SPDX-FileCopyrightText: 2026 Jan Babin
// SPDX-License-Identifier: AGPL-3.0-only
//
// Part of gtfs2rdf. See the LICENSE file for details.

module;

#include "util/diagnostics.h"

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

export module schema:core;
import rdf_components;
import field_transforms;
import runtime;
import util;
import schema_parser;

using namespace rdf;
using namespace util::strings;
using namespace field_transforms;

// this module implements core logic for several major classes of gtfs2rdf:
// - Instruction: represents a compiled instruction template ready to be rendered for each row of a 
//                file; specifically, implements render(row&) (i.e. the hot loop of the converter)
// - Schema: represents a mapping from a GTFS file to RDF, consisting of multiple instructions and 
//           metadata

namespace schema {

// a Datagap represents a placeholder but with all final metadata on how to render it
struct Datagap {
	size_t num_args = 0;
	size_t num_transforms = 0;

	// arguments for this placeholder (columns/literals/storage vars etc.)
	std::array<ArgSource, field_transforms::MAX_ARGS> arg_sources;

	// transforms as std::function objects
	std::array<field_transforms::Transform, field_transforms::MAX_TRANSFORMS> transforms;
	bool contains_transf2many = false;
	size_t transf2many_index = 0;
	StorageWriteSpec storage; // full storage spec in case of storage writes

	RenderKind render_kind = RenderKind::RAW; // how to render this placeholder
};

// pre-stage of final Instruction; placeholders not yet bound to columns / finalised
struct InstructionTemplate {
	std::string raw;
	std::vector<std::string> parts;       // static parts between datagaps
	std::vector<PlaceholderSpec> plhs;    // dynamic parts, one per each placeholder '{...}'
	std::vector<RenderKind> render_kinds; // per placeholder in order
	bool suppress_output = false;         // if true, do not write to file (only store internally)
};

// contains all metadata and logic for rendering an instruction for a given row of a GTFS file
export class Instruction {
  private:
	std::vector<std::string> parts_; // static parts between placeholders
	std::vector<Datagap> datagaps_;  // bound placeholders
	std::array<std::string_view, field_transforms::MAX_ARGS> arg_buf_;

	size_t base_len_ = 0;  // used to aggregate an estimate for the required output buffer size
	std::string out_buffer_;      // output buffer for rendering

	static constexpr std::string EMPTY_;
	static constexpr std::string_view EMPTY_SV_{EMPTY_};

	uint64_t counter_ = 0;
	bool is_valid_ = true;  // whether to include this instruction in rendering

	runtime::RuntimeContainer& rtc_;
	const std::string RAW_INSTRUCTION_;

	// swap buffers for transform application to avoid unnecessary copying
	std::string cur_;         // temporary storage for transform outputs
	std::string_view cur_sv_; // view into 'cur_'
	std::string next_;        // temporary swap storage

	bool contains_transf2many_ = false;
	RenderKind transf2many_render_kind_ = RenderKind::RAW;
	std::vector<std::string> transf2many_buf_;    // buffer for Transform2Many outputs
	std::vector<std::string_view> transf_buf_sv_; // views into the above buffer
	size_t transf2many_placeholder_index_ = 0;

	const bool SUPPRESS_OUTPUT_ = false;

	// helper: resolve an ArgSource to a string_view for this row
	// ArgSource can be a csv column (by id), a literal or a storage variable
	// we leave them where they are stored and only resolve to string_views
	std::string_view resolveARG_(const ArgSource& arg_src, std::span<const std::string> row) {
		switch (arg_src.kind) {
			case ArgSourceKind::COLUMN_INDEX:
				{
					if (arg_src.column_index < 0) {
						return EMPTY_SV_;
					}
					return row[arg_src.column_index];
				}
			case ArgSourceKind::LITERAL:
				{
					return arg_src.literal;
				}
			case ArgSourceKind::STORAGE_VAR:
				{
					const auto& var = rtc_.getStorage().getVariable(arg_src.ctx, arg_src.name);
					return var;
				}
		}
		return EMPTY_SV_;
	}

  public:
    // builds an Instruction given
	// - InstructionTemplate with placeholders not yet bound to columns / finalised
	// - column name to index mapping for resolving column args
	// resolves placeholder args to column indices and literals, checks for validity and gathers metadata for rendering
	Instruction(const InstructionTemplate& tmpl,
	            const std::unordered_map<std::string, int>& column_map,
	            runtime::RuntimeContainer& rtc,
	            const std::string& ctx_name)
	    : rtc_(rtc)
	    , RAW_INSTRUCTION_(tmpl.raw)
	    , SUPPRESS_OUTPUT_(tmpl.suppress_output) {
		parts_ = tmpl.parts;
		base_len_ = 0;
		for (auto& part : parts_) {
			base_len_ += part.size();
		}

		datagaps_.reserve(tmpl.plhs.size());

		// for each placeholder, resolve args and gather metadata for rendering and storage
		for (size_t ph_i = 0; ph_i < tmpl.plhs.size(); ++ph_i) {
			const auto& plh = tmpl.plhs[ph_i];
			Datagap dgp;

			// sanity check: number of args must not exceed max supported by field_transforms
			if (plh.args.size() > static_cast<size_t>(field_transforms::MAX_ARGS)) {
				throw diagnostics::Error("Schema error: too many placeholder args (Max value is " +
				                         std::to_string(field_transforms::MAX_ARGS) + ")");
			}
			dgp.num_args = plh.args.size();

			// resolve each arg to its source (column index, literal value or storage variable)
			for (size_t i = 0; i < plh.args.size(); ++i) {
				const auto& arg = plh.args[i];
				ArgSource src;

				if (arg.kind == ArgKind::COLUMN) {
					if (!column_map.contains(arg.name)) {
						throw diagnostics::Error("Schema error: unknown column '" + arg.name + "'");
					}
					int idx = column_map.at(arg.name);
					if (idx == -1) {
						// header missing required column -> skip this instruction
						is_valid_ = false;
						rtc_.getWarningCollector().addLeaf(
						    "Column '" + arg.name + "' not found in file header. Skipping",
						    diagnostics::WarningLevel::DEBUG);
						return;
					}
					src.kind = ArgSourceKind::COLUMN_INDEX;
					src.column_index = idx;

				} else if (arg.kind == ArgKind::LITERAL) {
					src.kind = ArgSourceKind::LITERAL;
					src.literal = arg.name;

				} else { // STORAGE_VAR
					src.kind = ArgSourceKind::STORAGE_VAR;
					src.name = arg.name;
					src.ctx = arg.ctx;
				}

				dgp.arg_sources[i] = std::move(src);
			}

			// sanity check: number of transforms must not exceed max supported by field_transforms
			if (plh.transforms.size() > static_cast<size_t>(field_transforms::MAX_TRANSFORMS)) {
				throw diagnostics::Error(
				    "Schema error: too many chained transforms (Max value is " +
				    std::to_string(field_transforms::MAX_TRANSFORMS) + ")");
			}
			dgp.num_transforms = plh.transforms.size();

			// just copy transforms from template to datagap
			for (size_t j = 0; j < plh.transforms.size(); ++j) {
				dgp.transforms[j] = plh.transforms[j].transform;
				if (dgp.transforms[j].kind == field_transforms::TransformKind::MANY) {
					if (contains_transf2many_) {
						throw diagnostics::Error(
						    "Schema error: only 1 Transform2Many allowed in total per instruction");
					}
					contains_transf2many_ = true;
					dgp.contains_transf2many = true;
					dgp.transf2many_index = j;
				}
			}

			// storage write directives are transferred, involving sanity checks
			dgp.storage = plh.storage;
			if (dgp.storage.kind != StorageKind::NONE) {
				if (dgp.storage.target_ctx.empty()) {
					dgp.storage.target_ctx = ctx_name; // default: current schema context
				} else if (dgp.storage.target_ctx != ctx_name) {
					throw diagnostics::Error(
					    "Schema error: storage context '" + dgp.storage.target_ctx +
					    "' does not match current schema context '" + ctx_name + "'");
				}
			}

			// sanity check: Transform2Many can never be used with a VARIABLE
			if (dgp.storage.kind == StorageKind::VARIABLE && dgp.contains_transf2many) {
				throw diagnostics::Error(
				    "Schema error: cannot store Transform2Many output into a variable");
			}

			dgp.render_kind =
			    (ph_i < tmpl.render_kinds.size()) ? tmpl.render_kinds[ph_i] : RenderKind::RAW;

			datagaps_.push_back(std::move(dgp));
		}

		// final newline
		parts_.back().append("\n");
		base_len_ += 1;

		// space pre-allocation based on approximation
		out_buffer_.reserve(base_len_ + 256); // NOLINT(readability-magic-numbers)
	}

	// render / hot loop of the converter: given a row of a GTFS file, produce the output string
	// for this instruction, applying transforms and performing storage writes as needed
	// NOTE: output string does not necessarily correspond to one triple due to Transform2Many
	std::string_view render(std::span<const std::string> row) {
		out_buffer_.clear();
		out_buffer_.append(parts_[0]);  // append static part before first placeholder / datagap
		cur_.clear();
		next_.clear();
		cur_sv_ = cur_;

		// rationale: cur_ will change with each transform, but span_1 will always be a view
		// of cur_ and can be passed to transforms without needing to be recreated
		field_transforms::ArgSpan span_1{&cur_sv_, 1};

		// process each datagap 
		// resolve args, apply transforms, perform storage writes and render output for each
		for (size_t k = 0; k < datagaps_.size(); ++k) {
			Datagap& dgp = datagaps_[k];

			cur_.clear();
			next_.clear();

			// resolve args to pointers sitting in the arg buffer for this datagap
			for (size_t j = 0; j < dgp.num_args; ++j) {
				arg_buf_[j] = resolveARG_(dgp.arg_sources[j], row);
			}

			// apply transform applications in sequence, feeding output of one as input to the next
			if (dgp.num_transforms > 0) {
				field_transforms::ArgSpan span_n{arg_buf_.data(), dgp.num_args};

				// if there is a Transform2Many, apply preceding transforms first, then the single 
				// Transform2Many and then the remaining transforms elementwise on the produced vector
				if (dgp.contains_transf2many) {
					transf2many_render_kind_ = dgp.render_kind;
					transf2many_buf_.clear();
					transf2many_placeholder_index_ = out_buffer_.size();

					// apply transforms before the 2N
					for (size_t i = 0; i < dgp.transf2many_index; i++) {
						cur_sv_ = cur_;
						if (i == 0) {
							dgp.transforms[i].single(span_n, next_);
						} else {
							dgp.transforms[i].single(span_1, next_);
						}
						cur_.swap(next_);
						next_.clear();
					}

					cur_sv_ = cur_;

					// run the Transform2Many
					if (dgp.transf2many_index == 0) {
						dgp.transforms[dgp.transf2many_index].many(span_n, transf2many_buf_);
					} else {
						dgp.transforms[dgp.transf2many_index].many(span_1, transf2many_buf_);
					}

					// run remaining transforms elementwise on the produced vector
					for (size_t i = dgp.transf2many_index + 1; i < dgp.num_transforms; i++) {
						for (auto& buf_i : transf2many_buf_) {
							cur_sv_ = buf_i;
							dgp.transforms[i].single(span_1, next_);
							buf_i.swap(next_);
							next_.clear();
						}
					}

				} else { // no Transform2Many
					dgp.transforms[0].single(span_n, cur_);

					if (dgp.num_transforms > 1) {
						cur_sv_ = cur_;

						for (size_t i = 1; i < dgp.num_transforms; ++i) {
							cur_sv_ = cur_;
							dgp.transforms[i].single(span_1, next_);
							cur_.swap(next_);
							next_.clear();
						}
					}
				}
				cur_sv_ = cur_;

			} else {
				// no transforms:
				// - non-keyed: placeholder output is arg0
				// - keyed: placeholder output is the first VALUE field (right side), not key0
				if (dgp.storage.isKeyed()) {
					cur_sv_ = arg_buf_[dgp.storage.key_arity];
				} else {
					cur_sv_ = arg_buf_[0];
				}
			}

			// early exit: either transforms filter (empty output) or there is a missing input
			// (column empty etc.) and there is no Transform2Many: needs not be rendered
			if (!dgp.contains_transf2many && cur_sv_.empty()) {
				return EMPTY_;
			}

			// process storage writes if applicable
			if (dgp.storage.kind != StorageKind::NONE) {
				auto& stor = rtc_.getStorage();

				if (dgp.storage.mode == StoreMode::FILTER_STORE_RAW) {
					// filter predicate: non-empty transform output

					if (!cur_sv_.empty()) {
						if (dgp.storage.kind == StorageKind::MULTI_MAP) {
							stor.storeValue(dgp.storage.target_ctx,
							                dgp.storage.target_name,
							                ArgSpan{arg_buf_.data(), dgp.storage.key_arity},
							                arg_buf_[dgp.storage.key_arity]);
						} else if (dgp.storage.kind == StorageKind::TUPLE_MAP) {
							stor.storeTuple(dgp.storage.target_ctx,
							                dgp.storage.target_name,
							                ArgSpan{arg_buf_.data(), dgp.storage.key_arity},
							                ArgSpan{arg_buf_.data() + dgp.storage.key_arity,
							                        dgp.storage.value_arity});
						} else if (dgp.storage.kind == StorageKind::VARIABLE) {
							stor.storeVariable(
							    dgp.storage.target_ctx, dgp.storage.target_name, arg_buf_[0]);
						}
					}
				} else if (dgp.storage.mode == StoreMode::STORE_RAW) {
					// store RHS tuple directly, unconditionally

					if (dgp.storage.kind == StorageKind::MULTI_MAP) {
						stor.storeValue(dgp.storage.target_ctx,
						                dgp.storage.target_name,
						                ArgSpan{arg_buf_.data(), dgp.storage.key_arity},
						                arg_buf_[dgp.storage.key_arity]);
					} else if (dgp.storage.kind == StorageKind::TUPLE_MAP) {
						stor.storeTuple(dgp.storage.target_ctx,
						                dgp.storage.target_name,
						                ArgSpan{arg_buf_.data(), dgp.storage.key_arity},
						                ArgSpan{arg_buf_.data() + dgp.storage.key_arity,
						                        dgp.storage.value_arity});
					}
				} else {
					// StoreComputed
					if (dgp.contains_transf2many) {
						if (!transf2many_buf_.empty()) {
							transf_buf_sv_.resize(transf2many_buf_.size());
							for (size_t i = 0; i < transf2many_buf_.size(); i++) {
								transf_buf_sv_[i] = transf2many_buf_[i];
							}
							stor.storeTuple(dgp.storage.target_ctx,
							                dgp.storage.target_name,
							                ArgSpan{arg_buf_.data(), dgp.storage.key_arity},
							                ArgSpan{transf_buf_sv_.data(), transf_buf_sv_.size()});
						}
					} // store computed, no Transform2Many -> single value in cur_
					else {
						switch (dgp.storage.kind) {
							case StorageKind::VARIABLE:
								stor.storeVariable(
								    dgp.storage.target_ctx, dgp.storage.target_name, cur_sv_);
								break;
							case StorageKind::MULTI_MAP:
								{
									stor.storeValue(dgp.storage.target_ctx,
									                dgp.storage.target_name,
									                ArgSpan{arg_buf_.data(), dgp.storage.key_arity},
									                cur_sv_);
									break;
								}
							default:
								throw diagnostics::Error(
								    "Schema error: invalid storage kind for computed storage");
						}
					}
				}
			}

			// compile output for this datagap if not SUPPRESS_OUTPUT_
			// escape datagap according to render kind and append static part after datagap
			if (!SUPPRESS_OUTPUT_) {
				if (!dgp.contains_transf2many) {
					switch (dgp.render_kind) {
						case RenderKind::IRI_REF:
							percentEncodeIRIREF(out_buffer_, cur_sv_);
							break;
						case RenderKind::PREFIXED_LOCAL:
							percentEncodePrefixedLocal(out_buffer_, cur_sv_);
							break;
						case RenderKind::LITERAL:
							percentEncodeLiteral(out_buffer_, cur_sv_);
							break;
						case RenderKind::LANG_TAG:
							out_buffer_.append(
							    cur_sv_); // [TODO]: language tags should be validated <future work>
							break;
						case RenderKind::RAW:
						default:
							out_buffer_.append(cur_sv_);
							break;
					}
				}
				out_buffer_.append(parts_[k + 1]);  // static bit after datagap
			}
		}  // !we have left the datagap loop now!

		// If SUPPRESS_OUTPUT_ => side effect only, skip writing entirely
		if (SUPPRESS_OUTPUT_) {
			return EMPTY_;
		}

		// replicate output compilation in case of Transform2Many
		if (contains_transf2many_) {
			// [TODO]: this is not very efficient
			std::string prefix = out_buffer_.substr(0, transf2many_placeholder_index_);
			std::string suffix = out_buffer_.substr(transf2many_placeholder_index_);
			out_buffer_.clear();
			for (const auto& val : transf2many_buf_) {
				if (val.empty()) {
					continue;
				}
				out_buffer_.append(prefix);
				switch (transf2many_render_kind_) { // how to escape the placeholder
					case RenderKind::IRI_REF:
						percentEncodeIRIREF(out_buffer_, val);
						break;
					case RenderKind::PREFIXED_LOCAL:
						percentEncodePrefixedLocal(out_buffer_, val);
						break;
					case RenderKind::LITERAL:
						percentEncodeLiteral(out_buffer_, val);
						break;
					case RenderKind::LANG_TAG:
						out_buffer_.append(val); // [TODO]: language tags should be validated <future work>
						break;
					case RenderKind::RAW:
					default:
						out_buffer_.append(val);
						break;
				}
				out_buffer_.append(suffix);
				counter_++;
			}
		} else {
			counter_++;
		}
		return out_buffer_;
	}

	[[nodiscard]] uint64_t getCount() const {
		return counter_;
	}
	[[nodiscard]] bool isValid() const {
		return is_valid_;
	}
	[[nodiscard]] const std::string& getRawInstruction() const {
		return RAW_INSTRUCTION_;
	}
	[[nodiscard]] const std::vector<Datagap>& getDatagaps() const {
		return datagaps_;
	}
	[[nodiscard]] std::vector<Datagap>& getModifiableDatagaps() {
		return datagaps_;
	}
};

// represents a schema for a GTFS file, i.e. a mapping
// manages the lifecycle of instructions from raw instruction strings to final compiled instructions
export class Schema {
  private:
	const std::string NAME_; // name of file with file type, e.g. "stops.txt"
	const std::unordered_map<std::string, std::string> PREFIXES_;
	std::vector<std::string> raw_instructions_;
	std::vector<std::vector<RenderKind>> raw_render_kinds_; // per instruction, per placeholder

	size_t num_storage_only_instructions_ = 0;
	runtime::RuntimeContainer& rtc_;
	const field_transforms::TransformRegistry& REGISTRY_;
	std::unordered_set<std::string> dependencies_; // other schemas that this schema depends on
	std::vector<InstructionTemplate> templates_;
	bool compiled_ = false;
	bool allow_storage_writes_ = true;

	// computed from header
	std::unordered_map<std::string, int>
	    column_map_;                        // column name -> index in file, -1 if not found
	std::vector<Instruction> instructions_; // computed instructions fit for rendering

	std::vector<std::string> header_;
	std::unordered_set<std::string> referenced_columns_;

  public:
	Schema(const Schema&) = delete;
	Schema& operator=(const Schema&) = delete;
	Schema(Schema&&) noexcept = default;
	Schema& operator=(Schema&&) noexcept = delete;

	Schema(std::string name,
	       const std::vector<std::string>& POSSIBLE_COLUMNS,
	       std::unordered_map<std::string, std::string> prefixes,
	       const std::vector<Triple>& TRIPLES,
	       runtime::RuntimeContainer& rtc)
	    : NAME_(std::move(name))
	    , PREFIXES_(std::move(prefixes))
	    , rtc_(rtc)
	    , REGISTRY_(rtc.getTransformRegistry()) {
		if (!isValidCTXName(NAME_)) {
			throw diagnostics::Error("Schema error: invalid schema name '" + NAME_ +
			                         "' (must end with .txt)");
		}

		for (const auto& col : POSSIBLE_COLUMNS) {
			column_map_[col] = -1; // initialise all to -1 (not found)
		}

		// build raw_instructions_ from triples
		for (size_t i = 0; i < TRIPLES.size(); ++i) {
			try {
				auto templ = TRIPLES[i].toTemplate(PREFIXES_, rtc_);
				raw_instructions_.push_back(templ.raw);
				raw_render_kinds_.push_back(templ.render_kinds);
				rtc_.getWarningCollector().addNode(
				    "while building instruction from triple number " + std::to_string(i + 1), 5);
			} catch (const std::exception& excpt) {
				diagnostics::wrapAndRethrow("while building instruction from triple number " +
				                            std::to_string(i + 1));
			}
		}
	}

	// allow side-effect only instructions to be added as well and add them to the front
	// so that they are processed before the triples such that they could be used by triples
	Schema(std::string name,
	       const std::vector<std::string>& POSSIBLE_COLUMNS,
	       std::unordered_map<std::string, std::string> prefixes,
	       const std::vector<Triple>& TRIPLES,
	       const std::vector<std::string>& NO_WRITE_INSTRUCTIONS,
	       runtime::RuntimeContainer& rtc)
	    : Schema(std::move(name), POSSIBLE_COLUMNS, std::move(prefixes), TRIPLES, rtc) {
		// add side effect instructions in front (so that triples could depend on them)
		num_storage_only_instructions_ = NO_WRITE_INSTRUCTIONS.size();
		raw_instructions_.insert(
		    raw_instructions_.begin(), NO_WRITE_INSTRUCTIONS.begin(), NO_WRITE_INSTRUCTIONS.end());
		raw_render_kinds_.insert(raw_render_kinds_.begin(), num_storage_only_instructions_, {});
	}

	// compile raw_instructions_ into templates_ and compute dependencies_ from other schemas
	void compile() {
		if (compiled_) {
			return;
		}

		templates_.clear();
		dependencies_.clear();

		templates_.reserve(raw_instructions_.size());

		for (size_t inst_i = 0; inst_i < raw_instructions_.size(); ++inst_i) {
			try {
				const auto& raw_inst = raw_instructions_[inst_i];
				const auto& kinds = raw_render_kinds_[inst_i];
				size_t kind_i = 0;

				InstructionTemplate templ;
				templ.raw = raw_inst;

				size_t start = 0;
				size_t pos = 0;

				// find each placeholder (e.g. {arg | transf | store@ctx}) and parse it into a
				// PlaceholderSpec, while also gathering static parts between placeholders
				while ((pos = raw_inst.find('{', start)) != std::string::npos) {
					size_t end = raw_inst.find('}', pos);
					if (end == std::string::npos) {
						throw diagnostics::Error(
						    "Syntax error: malformed instruction (missing '}')");
					}

					templ.parts.push_back(raw_inst.substr(start, pos - start));

					std::string placeholder = raw_inst.substr(pos + 1, end - pos - 1);

					try {
						PlaceholderSpec spec = parsePlaceholder(placeholder, REGISTRY_);

						// identify dependencies from other files using args after parsing
						for (const auto& arg : spec.args) {
							if (arg.kind == ArgKind::STORAGE_VAR) {
								if (!arg.ctx.empty()) {
									// if self-reference, check that the variable was already
									// written in a previous instruction
									if (arg.ctx == NAME_) {
										bool found = false;
										for (const auto& instr : templates_) {
											for (const auto& plh : instr.plhs) {
												if (plh.storage.target_name == arg.name) {
													found = true;
													break;
												}
											}
											if (found) {
												break;
											}
										}
										if (!found) {
											throw diagnostics::Error("Schema error: self-reference "
											                         "to storage variable '" +
											                         arg.name + "' in context '" +
											                         arg.ctx +
											                         "' before it was written");
										}
									} else {
										dependencies_.insert(arg.ctx);
									}
								}  // [TODO]: ctx empty, perhaps this should throw?
							} else if (arg.kind == ArgKind::COLUMN) {
								referenced_columns_.insert(arg.name);
							}
						}
						// dependencies from transform ctx hints
						for (const auto& trf : spec.transforms) {
							if (!trf.ctx_hint.empty()) {
								dependencies_.insert(trf.ctx_hint);
							}
						}

						templ.plhs.push_back(std::move(spec));

						// preserve render kind per placeholder coming from Triple::toTemplate
						if (kind_i < kinds.size()) {
							templ.render_kinds.push_back(kinds[kind_i]);
							kind_i++;
						} else {
							templ.render_kinds.push_back(RenderKind::RAW); // default
						}

						start = end + 1;

						rtc_.getWarningCollector().addNode(
						    "while parsing placeholder '" + placeholder + "'", 6);
					} catch (const std::exception& excpt) {
						diagnostics::wrapAndRethrow("while parsing placeholder '" + placeholder +
						                            "'");
					}
				}

				templ.parts.push_back(raw_inst.substr(start));
				templates_.push_back(std::move(templ));
				rtc_.getWarningCollector().addNode(
				    "while parsing instruction '" + raw_instructions_[inst_i] + "'", 5);
			} catch (const std::exception& excpt) {
				diagnostics::wrapAndRethrow("while parsing instruction '" +
				                            raw_instructions_[inst_i] + "'");
			}
		}

		// marks storage-only instructions to suppress output
		// note that this requires that storage-only instructions are added in front of the 
		// instruction list as should be enforced via the two-ctor approach of Schema
		for (size_t i = 0; i < num_storage_only_instructions_; ++i) {
			templates_[i].suppress_output = true;
		}

		compiled_ = true;
	}

	// set column map from header and build instructions_ once header from file has been read
	void setHeader(const std::vector<std::string>& header) {
		header_ = header;
		instructions_.clear();
		if (!compiled_) {
			throw diagnostics::Error(
			    "Internal error: schema must be compiled before setting header");
		}

		// compute column_map_ from header, i.e. column_name -> column_index in file, -1 if not found
		for (size_t file_idx = 0; file_idx < header.size(); ++file_idx) {
			if (column_map_.contains(header[file_idx])) {
				column_map_[header[file_idx]] = static_cast<int>(file_idx);
			} else {
				rtc_.getWarningCollector().addLeaf("Schema does not define column '" +
				                                       header[file_idx] + "' used in file header",
				                                   diagnostics::WarningLevel::WARNING);
			}
		}

		// build instructions_
		// skip storage-only instructions if storage writes are forbidden which would be triggered
		// if no other schema or schema itself depends on storage from this one
		size_t start_idx = allow_storage_writes_ ? 0 : num_storage_only_instructions_;
		for (size_t i = start_idx; i < templates_.size(); ++i) {
			const auto& tmp = templates_[i];
			try {
				Instruction instr(tmp, column_map_, rtc_, NAME_);

				rtc_.getWarningCollector().addNode(
				    "while building instruction from template '" + tmp.raw + "'", 5);

				if (!instr.isValid()) {
					continue;
				} 
				if (!allow_storage_writes_) {
					for (auto& dgp : instr.getModifiableDatagaps()) {
						dgp.storage.kind = StorageKind::NONE;
					}
				}
				instructions_.push_back(std::move(instr));

			} catch (const std::exception& excpt) {
				diagnostics::wrapAndRethrow("while building instruction from template '" + tmp.raw +
				                            "'");
			}
		}
	}

	// Getters
	const std::string& getName() const {
		return NAME_;
	}
	const std::unordered_map<std::string, std::string>& getPrefixes() const {
		return PREFIXES_;
	}
	const std::unordered_map<std::string, int>& getColumnMap() const {
		return column_map_;
	}
	std::vector<Instruction>& getInstructions() {
		return instructions_;
	}
	const std::unordered_set<std::string>& getDependencies() const {
		return dependencies_;
	}

	// returns the header with unused columns enclosed in parentheses
	std::vector<std::string> formatHeaderWithUnused() const {
		std::vector<std::string> out;
		for (const auto& col : header_) {
			if (referenced_columns_.contains(col)) {
				out.push_back(col);
			} else {
				out.push_back(enclose(col, '(', ')'));
			}
		}
		return out;
	}

	// Setters
	void forbidStorageWrites() {
		allow_storage_writes_ = false;
	}
};

// merge prefixes from multiple schemas into one map, checking for conflicts
export std::unordered_map<std::string, std::string>
mergePrefixes(const std::vector<Schema>& schemas,
              diagnostics::WarningCollector& wcol,
              bool strict_conflicts = true) {
	std::unordered_map<std::string, std::string> out;

	for (const auto& sch : schemas) {
		const auto& pfx = sch.getPrefixes();
		for (const auto& [k, v] : pfx) {
			if (auto itr = out.find(k); itr == out.end()) {
				out.emplace(k, v);
			} else if (itr->second != v) {
				if (strict_conflicts) {
					throw diagnostics::Error("Schema error: prefix conflict for '" + k + "': '" +
					                         itr->second + "' vs '" + v + "'");
				}
				wcol.addLeaf("Prefix conflict for '" + k + "': '" + itr->second + "' vs '" + v +
				                 "'. Using first.",
				             diagnostics::WarningLevel::WARNING);
			}
		}
	}
	return out;
}

} // namespace schema