// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the GTFS2RDF project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.

module;

#include "util/diagnostics.h"

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <iostream> // is this needed after debugging prints are removed?
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

namespace schema {

struct Datagap {
	size_t num_args = 0;
	size_t num_transforms = 0;

	// arguments for this placeholder (columns/litearls/storage/consts etc.)
	std::array<ArgSource, field_transforms::MaxArgs> arg_sources;

	// transforms as functors
	std::array<field_transforms::Transform, field_transforms::MaxTransforms> transforms;
	bool contains_transf2many = false;
	size_t transf2many_index = 0;
	StorageWriteSpec storage; // full storage spec

	RenderKind render_kind = RenderKind::Raw; // how to render this placeholder
};

struct InstructionTemplate {
	std::string raw;
	std::vector<std::string> parts;       // static parts between datagaps
	std::vector<PlaceholderSpec> phs;     // dynamic parts, one per each placeholder '{...}'
	std::vector<RenderKind> render_kinds; // per placeholder in order
	bool suppress_output = false;         // if true, do not write to file (only store internally)
};

export class Instruction {
  private:
	std::vector<std::string> parts_; // static parts between placeholders
	std::vector<Datagap> datagaps_;  // bound placeholders

	std::array<std::string_view, field_transforms::MaxArgs> arg_buf_;

	size_t base_len_ = 0;
	std::string out_;
	static inline const std::string empty_ = "";
	static inline constexpr std::string_view empty_sv_ = "";

	uint64_t counter_ = 0;
	bool is_valid_ = true;

	runtime::RuntimeContainer& rt_;
	const std::string raw_instruction_;

	std::string cur_;         // temporary storage for transform outputs
	std::string_view cur_sv_; // view into 'cur_'
	std::string next_;        // temporary swap storage

	bool contains_transf2many_ = false;
	RenderKind transf2many_render_kind_ = RenderKind::Raw;
	std::vector<std::string> transf_buf_; // buffer for Transform2Many outputs
	std::vector<std::string_view> transf_buf_sv_;
	size_t transf2many_placeholder_index_ = 0;

	bool suppress_output_ = false;

	// helper: resolve an ArgSource to a string_view for this row
	std::string_view resolve_arg(const ArgSource& a, std::span<const std::string> row) {
		switch (a.kind) {
		case ArgSourceKind::ColumnIndex: {
			if (a.column_index < 0) {
				return empty_sv_;
			} else {
				return row[a.column_index];
			}
		}
		case ArgSourceKind::Literal: {
			return a.literal;
		}
		case ArgSourceKind::StorageVar: {
			const auto& v = rt_.getStorage().get_variable(a.ctx, a.name);
			return v;
		}
		}
		return empty_sv_;
	}

  public:
	Instruction(const InstructionTemplate& tmpl,
	            const std::unordered_map<std::string, int>& column_map,
	            runtime::RuntimeContainer& rt,
	            const std::string& ctx_name)
	    : rt_(rt)
	    , raw_instruction_(tmpl.raw) {
		suppress_output_ = tmpl.suppress_output;
		parts_ = tmpl.parts;
		base_len_ = 0;
		for (auto& p : parts_)
			base_len_ += p.size();

		datagaps_.reserve(tmpl.phs.size());

		for (size_t ph_i = 0; ph_i < tmpl.phs.size(); ++ph_i) {
			const auto& ph = tmpl.phs[ph_i];
			Datagap dg;

			// args
			if (ph.args.size() > static_cast<size_t>(field_transforms::MaxArgs)) {
				throw diagnostics::Error("Schema error: too many placeholder args (Max value is " +
				                         std::to_string(field_transforms::MaxArgs) + ")");
			}
			dg.num_args = ph.args.size();

			for (size_t i = 0; i < ph.args.size(); ++i) {
				const auto& a = ph.args[i];
				ArgSource src;

				if (a.kind == ArgKind::Column) {
					if (!column_map.contains(a.name)) {
						throw diagnostics::Error("Schema error: unknown column '" + a.name + "'");
					}
					int idx = column_map.at(a.name);
					if (idx == -1) {
						// header missing required column -> skip this instruction
						is_valid_ = false;
						rt_.getWarningCollector().addLeaf("Column '" + a.name +
						                                      "' not found in header. Skipping",
						                                  diagnostics::WarningLevel::Info);
						return;
					}
					src.kind = ArgSourceKind::ColumnIndex;
					src.column_index = idx;
				} else if (a.kind == ArgKind::Literal) {
					src.kind = ArgSourceKind::Literal;
					src.literal = a.name;
				} else { // StorageVar
					src.kind = ArgSourceKind::StorageVar;
					src.name = a.name;
					src.ctx = a.ctx;
				}

				dg.arg_sources[i] = std::move(src);
			}

			// transforms
			if (ph.transforms.size() > static_cast<size_t>(field_transforms::MaxTransforms)) {
				throw diagnostics::Error(
				    "Schema error: too many chained transforms (Max value is " +
				    std::to_string(field_transforms::MaxTransforms) + ")");
			}
			dg.num_transforms = ph.transforms.size();

			for (size_t j = 0; j < ph.transforms.size(); ++j) {
				dg.transforms[j] = ph.transforms[j].transform;
				if (dg.transforms[j].kind == field_transforms::TransformKind::Multi) {
					if (contains_transf2many_) {
						throw diagnostics::Error(
						    "Schema error: only 1 Transform2Many allowed in total per instruction");
					}
					contains_transf2many_ = true;
					dg.contains_transf2many = true;
					dg.transf2many_index = j;
				}
			}

			// storage
			dg.storage = ph.storage;
			if (dg.storage.kind != StorageKind::None) {
				if (dg.storage.target_ctx.empty()) {
					dg.storage.target_ctx = ctx_name; // default: current schema context
				} else if (dg.storage.target_ctx != ctx_name) {
					throw diagnostics::Error(
					    "Schema error: storage context '" + dg.storage.target_ctx +
					    "' does not match current schema context '" + ctx_name + "'");
				}
			}

			// some sanity checks
			if (dg.storage.kind == StorageKind::Variable && dg.contains_transf2many) {
				throw diagnostics::Error(
				    "Schema error: cannot store Transform2Many output into a variable");
			}

			dg.render_kind =
			    (ph_i < tmpl.render_kinds.size()) ? tmpl.render_kinds[ph_i] : RenderKind::Raw;

			datagaps_.push_back(std::move(dg));
		}

		// final newline
		parts_.back().append("\n");
		base_len_ += 1;

		out_.reserve(base_len_ + 256);
	}

	std::string_view render(std::span<const std::string> row) {
		out_.clear();
		out_.append(parts_[0]);
		cur_.clear();
		next_.clear();
		cur_sv_ = cur_;
		field_transforms::ArgSpan span1{&cur_sv_, 1};

		for (size_t k = 0; k < datagaps_.size(); ++k) {
			Datagap& dg = datagaps_[k];

			cur_.clear();
			next_.clear();

			// resolve args to pointers
			for (size_t j = 0; j < dg.num_args; ++j) {
				arg_buf_[j] = resolve_arg(dg.arg_sources[j], row);
			}

			// compute placeholder output (and possibly Transform2Many buffer)
			if (dg.num_transforms > 0) {
				field_transforms::ArgSpan spanN{arg_buf_.data(), dg.num_args};
				if (dg.contains_transf2many) {
					transf2many_render_kind_ = dg.render_kind;
					transf_buf_.clear();
					transf2many_placeholder_index_ = out_.size();

					// apply transforms before the 2N
					for (size_t i = 0; i < dg.transf2many_index; i++) {
						cur_sv_ = cur_;
						if (i == 0) {
							dg.transforms[i].single(spanN, next_);
						} else {
							dg.transforms[i].single(span1, next_);
						}
						cur_.swap(next_);
						next_.clear();
					}

					cur_sv_ = cur_;

					// run the Transform2Many
					if (dg.transf2many_index == 0) {
						dg.transforms[dg.transf2many_index].multi(spanN, transf_buf_);
					} else {
						dg.transforms[dg.transf2many_index].multi(span1, transf_buf_);
					}

					// run remaining transforms elementwise on the produced vector
					for (size_t i = dg.transf2many_index + 1; i < dg.num_transforms; i++) {
						for (size_t buf_i = 0; buf_i < transf_buf_.size(); buf_i++) {
							cur_sv_ = transf_buf_[buf_i];
							dg.transforms[i].single(span1, next_);
							transf_buf_[buf_i].swap(next_);
							next_.clear();
						}
					}
				} else { // no Transform2Many
					dg.transforms[0].single(spanN, cur_);

					if (dg.num_transforms > 1) {
						cur_sv_ = cur_;

						for (size_t i = 1; i < dg.num_transforms; ++i) {
							cur_sv_ = cur_;
							dg.transforms[i].single(span1, next_);
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
				if (dg.storage.is_keyed()) {
					cur_sv_ = arg_buf_[dg.storage.key_arity];
				} else {
					cur_sv_ = arg_buf_[0];
				}
			}

			// early exit: either transforms filter (empty output) or empty column or empty
			// computed Transform2Many result which need not be rendered
			if (!dg.contains_transf2many && cur_sv_.empty())
				return empty_;

			// --- side effects: storage write ---
			if (dg.storage.kind != StorageKind::None) {
				auto& st = rt_.getStorage();

				if (dg.storage.mode == StoreMode::FilterStoreRaw) {
					// filter predicate: non-empty transform output

					if (!cur_sv_.empty()) {
						if (dg.storage.kind == StorageKind::MultiMap) {
							st.store_value(dg.storage.target_ctx,
							               dg.storage.target_name,
							               ArgSpan{arg_buf_.data(), dg.storage.key_arity},
							               arg_buf_[dg.storage.key_arity]);
						} else if (dg.storage.kind == StorageKind::TupleMap) {
							st.store_tuple(dg.storage.target_ctx,
							               dg.storage.target_name,
							               ArgSpan{arg_buf_.data(), dg.storage.key_arity},
							               ArgSpan{arg_buf_.data() + dg.storage.key_arity,
							                       dg.storage.value_arity});
						} else if (dg.storage.kind == StorageKind::Variable) {
							st.store_variable(
							    dg.storage.target_ctx, dg.storage.target_name, arg_buf_[0]);
						}
					}
				} else if (dg.storage.mode == StoreMode::StoreRaw) {
					// store RHS tuple directly, unconditionally

					if (dg.storage.kind == StorageKind::MultiMap) {
						st.store_value(dg.storage.target_ctx,
						               dg.storage.target_name,
						               ArgSpan{arg_buf_.data(), dg.storage.key_arity},
						               arg_buf_[dg.storage.key_arity]);
					} else if (dg.storage.kind == StorageKind::TupleMap) {
						st.store_tuple(dg.storage.target_ctx,
						               dg.storage.target_name,
						               ArgSpan{arg_buf_.data(), dg.storage.key_arity},
						               ArgSpan{arg_buf_.data() + dg.storage.key_arity,
						                       dg.storage.value_arity});
					}
				} else {
					// StoreComputed
					if (dg.contains_transf2many) {
						if (!transf_buf_.empty()) {
							transf_buf_sv_.resize(transf_buf_.size());
							for (size_t i = 0; i < transf_buf_.size(); i++) {
								transf_buf_sv_[i] = transf_buf_[i];
							}
							st.store_tuple(dg.storage.target_ctx,
							               dg.storage.target_name,
							               ArgSpan{arg_buf_.data(), dg.storage.key_arity},
							               ArgSpan{transf_buf_sv_.data(), transf_buf_sv_.size()});
						}
					} else {
						switch (dg.storage.kind) {
						case StorageKind::Variable:
							st.store_variable(
							    dg.storage.target_ctx, dg.storage.target_name, cur_sv_);
							break;
						case StorageKind::MultiMap: {
							st.store_value(dg.storage.target_ctx,
							               dg.storage.target_name,
							               ArgSpan{arg_buf_.data(), dg.storage.key_arity},
							               cur_sv_);
							break;
						}
						}
					}
				}
			}

			// --- output rendering ---
			// suppress output for the whole instruction
			if (!suppress_output_) {
				if (!dg.contains_transf2many) {
					switch (dg.render_kind) { // how to escape the placeholder
					case RenderKind::IriRef:
						percent_encode_iriref(out_, cur_sv_);
						break;
					case RenderKind::PrefixedLocal:
						percent_encode_prefixed_local(out_, cur_sv_);
						break;
					case RenderKind::Literal:
						percent_encode_literal(out_, cur_sv_);
						break;
					case RenderKind::LangTag:
						out_.append(cur_sv_); // language tags do not need escaping
						break;
					case RenderKind::Raw:
					default:
						out_.append(cur_sv_);
						break;
					}
				}
				out_.append(parts_[k + 1]);
			}
		}

		// If suppress_output_ => side effect only, skip writing entirely
		if (suppress_output_) {
			// counter_++;  // no triples written
			return empty_;
		}

		// replicate for Transform2Many
		if (contains_transf2many_) {
			std::string prefix = out_.substr(0, transf2many_placeholder_index_);
			std::string suffix = out_.substr(transf2many_placeholder_index_);
			out_.clear();
			for (const auto& val : transf_buf_) {
				if (val.empty())
					continue;
				out_.append(prefix);
				switch (transf2many_render_kind_) { // how to escape the placeholder
				case RenderKind::IriRef:
					percent_encode_iriref(out_, val);
					break;
				case RenderKind::PrefixedLocal:
					percent_encode_prefixed_local(out_, val);
					break;
				case RenderKind::Literal:
					percent_encode_literal(out_, val);
					break;
				case RenderKind::LangTag:
					out_.append(val); // language tags do not need escaping
					break;
				case RenderKind::Raw:
				default:
					out_.append(val);
					break;
				}
				out_.append(suffix);
				counter_++;
			}
		} else {
			counter_++;
		}
		return out_;
	}

	uint64_t getCount() const {
		return counter_;
	}
	bool isValid() const {
		return is_valid_;
	}
	const std::string& getRawInstruction() const {
		return raw_instruction_;
	}
	const std::vector<Datagap>& getDatagaps() const {
		return datagaps_;
	}
	std::vector<Datagap>& getModifiableDatagaps() {
		return datagaps_;
	}
};

export class Schema {
  private:
	const std::string name_; // name of file with file type, e.g. "stops.txt"
	const std::vector<std::string> possible_columns_ =
	    {}; // all columns that could be contained by <name_>  TODO: actually needed?
	std::unordered_map<std::string, std::string> prefixes_;
	std::vector<std::string> raw_instructions_;
	std::vector<std::vector<RenderKind>> raw_render_kinds_; // per instruction, per placeholder

	size_t num_storage_only_instructions_ = 0;
	runtime::RuntimeContainer& rt_;
	const field_transforms::TransformRegistry& registry_;
	std::unordered_set<std::string> dependencies_; // other schemas that this schema depends on
	std::vector<InstructionTemplate> templates_;
	bool compiled_ = false;
	bool allow_storage_writes_ = true;

	// computed from header
	std::unordered_map<std::string, int>
	    column_map_;                        // column name -> index in file, -1 if not found
	std::vector<Instruction> instructions_; // computed instructions

	std::vector<std::string> header_;
	std::unordered_set<std::string> referenced_columns_;

  public:
	Schema(const Schema&) = delete;
	Schema& operator=(const Schema&) = delete;
	Schema(Schema&&) noexcept = default;
	Schema& operator=(Schema&&) noexcept = delete;

	Schema(const std::string name,
	       const std::vector<std::string> possible_columns,
	       const std::unordered_map<std::string, std::string> prefixes,
	       const std::vector<Triple>& triples,
	       runtime::RuntimeContainer& rt)
	    : name_(std::move(name))
	    , possible_columns_(std::move(possible_columns))
	    , prefixes_(std::move(prefixes))
	    , rt_(rt)
	    , registry_(rt.getTransformRegistry()) {
		if (!valid_ctx_name(name_)) {
			throw diagnostics::Error("Schema error: invalid schema name '" + name_ +
			                         "' (must end with .txt)");
		}

		for (const auto& col : this->possible_columns_) {
			column_map_[col] = -1; // initialize all to -1 (not found)
		}

		// build raw_instructions_ from triples
		for (size_t i = 0; i < triples.size(); ++i) {
			try {
				auto t = triples[i].toTemplate(prefixes_, rt_);
				raw_instructions_.push_back(t.raw);
				raw_render_kinds_.push_back(t.render_kinds);
				rt_.getWarningCollector().addNode(
				    "while building instruction from triple number " + std::to_string(i + 1), 4);
			} catch (const diagnostics::Error& e) {
				diagnostics::wrap_and_rethrow(
				    e, "while building instruction from triple number " + std::to_string(i + 1));
			}
		}
	}

	// allow side-effect only instructions to be added as well and add them to the front
	Schema(const std::string name,
	       const std::vector<std::string> possible_columns,
	       const std::unordered_map<std::string, std::string> prefixes,
	       const std::vector<Triple>& triples,
	       const std::vector<std::string>& storage_only_instructions,
	       runtime::RuntimeContainer& rt)
	    : Schema(name, possible_columns, prefixes, triples, rt) {
		// add side effect instructions in front (so that triples could depend on them)
		num_storage_only_instructions_ = storage_only_instructions.size();
		raw_instructions_.insert(raw_instructions_.begin(),
		                         storage_only_instructions.begin(),
		                         storage_only_instructions.end());
		raw_render_kinds_.insert(raw_render_kinds_.begin(), num_storage_only_instructions_, {});
	}

	// compile raw_instructions_ into templates_ and compute dependencies_ from other schemas
	void compile() {
		if (compiled_)
			return;

		templates_.clear();
		dependencies_.clear();

		templates_.reserve(raw_instructions_.size());

		for (size_t inst_i = 0; inst_i < raw_instructions_.size(); ++inst_i) {
			try {
				const auto& raw_inst = raw_instructions_[inst_i];
				const auto& kinds = raw_render_kinds_[inst_i];
				size_t kind_i = 0;

				InstructionTemplate t;
				t.raw = raw_inst;

				size_t start = 0;
				size_t pos = 0;

				while ((pos = raw_inst.find('{', start)) != std::string::npos) {
					size_t end = raw_inst.find('}', pos);
					if (end == std::string::npos) {
						throw diagnostics::Error(
						    "Syntax error: malformed instruction (missing '}')");
					}

					t.parts.push_back(raw_inst.substr(start, pos - start));

					std::string placeholder = raw_inst.substr(pos + 1, end - pos - 1);

					try {
						PlaceholderSpec spec = parse_placeholder(placeholder, registry_);

						// dependencies from args
						for (const auto& a : spec.args) {
							if (a.kind == ArgKind::StorageVar) {
								if (!a.ctx.empty()) {
									// if self-reference, check that the variable was already
									// written in a previous instruction
									if (a.ctx == name_) {
										bool found = false;
										for (const auto& instr : templates_) {
											for (const auto& ph : instr.phs) {
												if (ph.storage.target_name == a.name) {
													found = true;
													break;
												}
											}
											if (found)
												break;
										}
										if (!found) {
											throw diagnostics::Error("Schema error: self-reference "
											                         "to storage variable '" +
											                         a.name + "' in context '" +
											                         a.ctx +
											                         "' before it was written");
										}
									} else
										dependencies_.insert(a.ctx);
								}
							} else if (a.kind == ArgKind::Column) {
								referenced_columns_.insert(a.name);
							}
						}
						// dependencies from transform ctx hints
						for (const auto& tc : spec.transforms) {
							if (!tc.ctx_hint.empty())
								dependencies_.insert(tc.ctx_hint);
						}

						t.phs.push_back(std::move(spec));

						// preserve render kind per placeholder coming from Triple::toTemplate
						if (kind_i < kinds.size()) {
							t.render_kinds.push_back(kinds[kind_i]);
							kind_i++;
						} else {
							t.render_kinds.push_back(RenderKind::Raw); // default
						}

						start = end + 1;

						rt_.getWarningCollector().addNode(
						    "while parsing placeholder '" + placeholder + "'", 4);
					} catch (const diagnostics::Error& e) {
						diagnostics::wrap_and_rethrow(
						    e, "while parsing placeholder '" + placeholder + "'");
					}
				}

				t.parts.push_back(raw_inst.substr(start));
				templates_.push_back(std::move(t));
				rt_.getWarningCollector().addNode(
				    "while parsing instruction '" + raw_instructions_[inst_i] + "'", 3);
			} catch (const diagnostics::Error& e) {
				diagnostics::wrap_and_rethrow(
				    e, "while parsing instruction '" + raw_instructions_[inst_i] + "'");
			}
		}

		for (size_t i = 0; i < num_storage_only_instructions_; ++i) {
			templates_[i].suppress_output = true;
		}

		compiled_ = true;
	}

	// set column map from header and build instructions_ once header from file has been read
	void setHeader(const std::vector<std::string>& header) {
		header_ = header;
		instructions_.clear();
		if (!compiled_)
			throw diagnostics::Error(
			    "Internal error: schema must be compiled before setting header");
		// compute column_map_ from header
		for (size_t file_idx = 0; file_idx < header.size(); ++file_idx) {
			if (column_map_.contains(header[file_idx])) {
				column_map_[header[file_idx]] = static_cast<int>(file_idx);
			} else {
				rt_.getWarningCollector().addLeaf("Schema does not use header column'" +
				                                      header[file_idx] + "'",
				                                  diagnostics::WarningLevel::Info);
			}
		}
		// build instructions_
		// skip storage-only instructions if storage writes are forbidden which would be triggered
		// if no other schema actually depends on storage from this one
		size_t start_idx = allow_storage_writes_ ? 0 : num_storage_only_instructions_;
		for (size_t i = start_idx; i < templates_.size(); ++i) {
			const auto& tmp = templates_[i];
			try {
				Instruction instr(tmp, column_map_, rt_, name_);
				if (!instr.isValid()) {
					continue;
				} // skip invalid instructions
				if (!allow_storage_writes_) {
					for (auto& dg : instr.getModifiableDatagaps()) {
						dg.storage.kind = StorageKind::None;
					}
				}
				instructions_.push_back(std::move(instr));

				rt_.getWarningCollector().addNode(
				    "while building instruction from template '" + tmp.raw + "'", 3);
			} catch (const diagnostics::Error& e) {
				diagnostics::wrap_and_rethrow(
				    e, "while building instruction from template '" + tmp.raw + "'");
			}
		}
	}

	// Getters
	const std::string& getName() const {
		return name_;
	}
	const std::vector<std::string>& getPossibleColumns() const {
		return possible_columns_;
	}
	const std::unordered_map<std::string, std::string>& getPrefixes() const {
		return prefixes_;
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
merge_prefixes(const std::vector<Schema>& schemas,
               diagnostics::WarningCollector& wc,
               bool strict_conflicts = true) {
	std::unordered_map<std::string, std::string> out;

	for (const auto& sc : schemas) {
		const auto& pfx = sc.getPrefixes();
		for (const auto& [k, v] : pfx) {
			if (auto it = out.find(k); it == out.end()) {
				out.emplace(k, v);
			} else if (it->second != v) {
				if (strict_conflicts) {
					throw diagnostics::Error("Schema error: prefix conflict for '" + k + "': '" +
					                         it->second + "' vs '" + v + "'");
				} else {
					wc.addLeaf("Prefix conflict for '" + k + "': '" + it->second + "' vs '" + v +
					               "'. Using first.",
					           diagnostics::WarningLevel::Warning);
				}
			}
		}
	}
	return out;
}

} // namespace schema