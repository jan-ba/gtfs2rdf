// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the gtfs2rdf project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.

module;

#include "util/diagnostics.h"

#include <cctype>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

export module schema_parser;
import field_transforms;
import util;

using namespace util;

namespace schema {

// type of argument in a placeholder {arg1, arg2, ... | transform1 | transform2 > STORAGE}
export enum class ArgKind : uint8_t {
	COLUMN,      // e.g. stop_id
	STORAGE_VAR, // e.g. FEED_LANG@feed_info.txt
	LITERAL      // e.g. "hardcoded"
};

// describes one argument in a placeholder,
// i.e. "{ arg1, arg2, ... | transform1 | transform2 > STORAGE }"
export struct ArgSpec {
	ArgKind kind;
	std::string name; // column name or variable name; for Literal: the literal text
	std::string ctx;  // only for STORAGE_VAR (and later maybe other storage reads)l
};

export enum class StoreMode : uint8_t {
	NONE,             // no storage write (normal FILE placeholder)
	STORE_COMPUTED,   // store transform output (no (...) on value side)
	FILTER_STORE_RAW, // (...) present -> transforms act as filter, store raw tuple
	STORE_RAW         // store raw args without transforms
};

export enum class StorageKind : uint8_t {
	NONE,      // FILE output
	VARIABLE,  // > CONST
	MULTI_MAP, // key -> vector<string> with quick lookup
	TUPLE_MAP  // key -> vector<vector<string...>>
};

export struct StorageWriteSpec {
	StorageKind kind = StorageKind::NONE;
	StoreMode mode = StoreMode::NONE;

	std::string target_name; // e.g. FEED_LANG, removed_dates
	std::string
	    target_ctx; // optional explicit ctx from > TARGET@ctx; else filled later with schema ctx

	// partitioning of args for keyed storage:
	// args = [keys..., values..., extra...]
	uint8_t key_arity = 0;
	uint8_t value_arity = 0;
	uint8_t extra_arity = 0;

	// convenience
	[[nodiscard]] bool isKeyed() const {
		return kind == StorageKind::MULTI_MAP || kind == StorageKind::TUPLE_MAP;
	}
};

export struct TransformCallSpec {
	field_transforms::Transform transform; // your existing functor wrapper (ONE/MANY)
	std::string ctx_hint; // @ctx on the transform name (dependency + later transform access)
};

export struct PlaceholderSpec {
	std::vector<ArgSpec> args;                 // includes columns, literals, storage const reads
	std::vector<TransformCallSpec> transforms; // functor + optional context
	StorageWriteSpec storage;                  // what to do with output / whether to store
};

export enum class ArgSourceKind : u_int8_t { COLUMN_INDEX, STORAGE_VAR, LITERAL };

export struct ArgSource {
	ArgSourceKind kind;
	size_t column_index = -1; // COLUMN_INDEX: if valid, index in CSV row
	std::string literal;
	std::string name; // STORAGE_VAR: variable name
	std::string ctx;  // STORAGE_VAR: context name
};

export struct BoundPlaceholder {
	std::vector<ArgSource> args;
	std::vector<TransformCallSpec> transforms;
	StorageWriteSpec storage;
};

ArgSpec parseArg(std::string_view svw) {
	if (svw.empty()) {
		throw diagnostics::Error("Syntax error: Empty argument field");
	}

	// literal
	if (svw.front() == '"') {
		return ArgSpec{ArgKind::LITERAL, util::unquote(svw), ""};
	}

	// storage variable read: NAME@ctx
	auto [lhs, rhs] = util::splitAt(svw, '@');
	if (!rhs.empty()) {
		if (!isValidCTXName(rhs)) {
			throw diagnostics::Error("Syntax error: Invalid context name after '@' in arg: " +
			                         std::string(svw));
		}
		return ArgSpec{ArgKind::STORAGE_VAR, std::string(lhs), std::string(rhs)};
	}

	// column
	return ArgSpec{ArgKind::COLUMN, std::string(svw), ""};
}

TransformCallSpec parseTransform(std::string_view svw,
                                 const field_transforms::TransformRegistry& reg) {
	if (svw.empty()) {
		throw diagnostics::Error("Syntax error: Empty transform field");
	}
	auto [name, ctx] = util::splitAt(svw, '@');
	TransformCallSpec out;
	out.transform = reg.getTransform(std::string(name));
	if (!ctx.empty()) {
		if (!isValidCTXName(ctx)) {
			throw diagnostics::Error("Syntax error: Invalid context name after '@' in transform: " +
			                         std::string(svw));
		}
		out.ctx_hint = std::string(ctx);
	}
	return out;
}

void parseTarget(std::string_view svw, StorageWriteSpec& stg) {
	auto [name, ctx] = util::splitAt(svw, '@');
	if (name.empty()) {
		throw diagnostics::Error("Syntax error: Empty storage target after '>'");
	}
	stg.target_name = std::string(name);
	if (!ctx.empty()) {
		if (!isValidCTXName(ctx)) {
			throw diagnostics::Error(
			    "Syntax error: Invalid context name after '@' in storage target: " +
			    std::string(svw));
		}
		stg.target_ctx = std::string(ctx);
	}
}

// ---------- main parser ----------

export PlaceholderSpec parsePlaceholder(std::string_view raw,
                                        const field_transforms::TransformRegistry& reg) {
	// normalise whitespace but keep spaces in quoted literals
	std::string cleaned = util::removeWSOutsideQuotes(raw);
	std::string_view cleaned_svw(cleaned);

	PlaceholderSpec spec;

	// split off storage target: "... > target"
	// only at most one '>' supported at top level
	auto [before_gt, after_gt] = util::splitOnceAtTopLevel(cleaned_svw, '>');
	if (!after_gt.empty()) {
		// storage is enabled, kind/mode decided below
		parseTarget(after_gt, spec.storage);
	}

	// split remainder into fields part and transforms part: "fields | t1 | t2"
	auto [fields_part, trans_part] = util::splitOnceAtTopLevel(before_gt, '|');

	// transforms
	if (!trans_part.empty()) {
		auto transforms = util::splitAtTopLevel(trans_part, '|');
		for (auto transform : transforms) {
			spec.transforms.push_back(parseTransform(transform, reg));
		}
	}

	// detect keyed storage (':') at top level: "key1,key2 : (val1,val2),extra1,extra2"
	auto [left_of_colon, right_of_colon] = util::splitOnceAtTopLevel(fields_part, ':');

	if (!right_of_colon.empty()) {
		if (spec.storage.target_name.empty()) {
			throw diagnostics::Error(
			    "Syntax error: Keyed placeholder with ':' requires storage target after '>')");
		}

		// keys
		auto key_toks = util::splitAtTopLevel(left_of_colon, ',');
		if (key_toks.empty()) {
			throw diagnostics::Error("Syntax error: Missing key fields before ':'");
		}
		for (auto key_tok : key_toks) {
			spec.args.push_back(parseArg(key_tok));
		}
		spec.storage.key_arity = static_cast<uint8_t>(key_toks.size());

		// right side: either "(values),extras" (filter mode) OR "value_inputs" (compute mode)
		if (!right_of_colon.empty() && right_of_colon.front() == '(') {
			// FILTER MODE
			spec.storage.mode = StoreMode::FILTER_STORE_RAW;

			// find matching ')'
			bool in_q = false;
			int par = 0;
			size_t close = std::string_view::npos;
			for (size_t i = 0; i < right_of_colon.size(); ++i) {
				char c = right_of_colon[i]; // NOLINT(readability-identifier-length)
				if (c == '"' && (i == 0 || right_of_colon[i - 1] != '\\')) {
					in_q = !in_q;
				}
				if (in_q) {
					continue;
				}
				if (c == '(') {
					++par;
				} else if (c == ')') {
					--par;
					if (par == 0) {
						close = i;
						break;
					}
				}
			}
			if (close == std::string_view::npos) {
				throw diagnostics::Error("Syntax error: Unbalanced parentheses in placeholder");
			}

			auto inside = right_of_colon.substr(1, close - 1);
			auto after = right_of_colon.substr(close + 1); // may start with ',' or empty

			auto val_toks = util::splitAtTopLevel(inside, ',');
			if (val_toks.empty()) {
				throw diagnostics::Error("Syntax error: Empty value tuple '(...)' after ':'");
			}
			for (auto val_tok : val_toks) {
				spec.args.push_back(parseArg(val_tok));
			}
			spec.storage.value_arity = static_cast<uint8_t>(val_toks.size());

			// extras
			if (!after.empty()) {
				if (after.front() != ',') {
					throw diagnostics::Error(
					    "Syntax error: Expected ',' after ')' for extras in placeholder");
				}
				auto extras = after.substr(1);
				if (!extras.empty()) {
					auto ex_toks = util::splitAtTopLevel(extras, ',');
					for (auto ex_tok : ex_toks) {
						spec.args.push_back(parseArg(ex_tok));
					}
					spec.storage.extra_arity = static_cast<uint8_t>(ex_toks.size());
				}
			}

			// kind: multimap if 1 value, else tuplemap
			spec.storage.kind =
			    (spec.storage.value_arity == 1) ? StorageKind::MULTI_MAP : StorageKind::TUPLE_MAP;

			// disallow Transform2Many in filter-mode
			for (auto& trf : spec.transforms) {
				if (trf.transform.kind == field_transforms::TransformKind::MANY) {
					throw diagnostics::Error("Syntax error: Transform2Many not allowed with "
					                         "filter-mode '(...)' in placeholder");
				}
			}

		} else {
			// RHS without (...) : either
			//  - STORE_RAW (no transforms): store RHS tuple directly (MultiMap if 1 value else
			//  TupleMap)
			//  - STORE_COMPUTED (has transforms): store transform output (TupleMap if
			//  Transform2Many involved)

			auto rhs_toks = util::splitAtTopLevel(right_of_colon, ',');
			if (rhs_toks.empty()) {
				throw diagnostics::Error("Syntax error: Missing value inputs after ':'");
			}
			for (auto rhs_tok : rhs_toks) {
				spec.args.push_back(parseArg(rhs_tok));
			}

			spec.storage.value_arity = static_cast<uint8_t>(rhs_toks.size());
			spec.storage.extra_arity = 0;

			bool has_multi = false;
			for (const auto& trf : spec.transforms) {
				if (trf.transform.kind == field_transforms::TransformKind::MANY) {
					has_multi = true;
					break;
				}
			}

			if (spec.transforms.empty()) {
				// raw store, no filtering
				spec.storage.mode = StoreMode::STORE_RAW;
				spec.storage.kind = (spec.storage.value_arity == 1) ? StorageKind::MULTI_MAP
				                                                    : StorageKind::TUPLE_MAP;
			} else {
				// computed store
				spec.storage.mode = StoreMode::STORE_COMPUTED;
				// Transform2Many output is a vector => must be TupleMap
				spec.storage.kind = has_multi ? StorageKind::TUPLE_MAP : StorageKind::MULTI_MAP;
			}
		}

	} else {
		// non-keyed (variables or file output)
		auto arg_toks = util::splitAtTopLevel(fields_part, ',');
		for (auto arg_tok : arg_toks) {
			spec.args.push_back(parseArg(arg_tok));
		}

		if (!spec.storage.target_name.empty()) {
			// variable store
			spec.storage.kind = StorageKind::VARIABLE;
			spec.storage.mode = StoreMode::STORE_COMPUTED;
		} else {
			spec.storage.kind = StorageKind::NONE;
			spec.storage.mode = StoreMode::NONE;
		}
	}

	// --- ensure correctness of the parsed spec ---

	if (spec.args.empty()) {
		throw diagnostics::Error("Syntax error: Placeholder must have at least 1 argument");
	}

	// count Transform2Many occurrences
	size_t num_multi = 0;
	for (const auto& trf : spec.transforms) {
		if (trf.transform.kind == field_transforms::TransformKind::MANY) {
			++num_multi;
		}
	}
	if (num_multi > 1) {
		throw diagnostics::Error("Syntax error: Only one Transform2Many allowed per placeholder");
	}

	const bool HAS_TRANSFORMS = !spec.transforms.empty();

	// non-keyed placeholders: multi-arg is only allowed if transforms exist
	if (!spec.storage.isKeyed()) {
		if (!HAS_TRANSFORMS && spec.args.size() != 1) {
			throw diagnostics::Error(
			    "Syntax error: Non-keyed placeholder with multiple args requires transforms");
		}
	}

	// FilterStoreRaw should actually have transforms (otherwise there's not a filter)
	if (spec.storage.mode == StoreMode::FILTER_STORE_RAW && !HAS_TRANSFORMS) {
		throw diagnostics::Error(
		    "Syntax error: FilterStoreRaw '(...)' requires at least one filter transform");
	}

	// Transform2Many must never target Variable or MultiMap
	if (num_multi == 1) {
		if (spec.storage.kind == StorageKind::VARIABLE) {
			throw diagnostics::Error("Syntax error: Transform2Many cannot store into Variable");
		}
		if (spec.storage.kind == StorageKind::MULTI_MAP) {
			throw diagnostics::Error(
			    "Syntax error: Transform2Many cannot store into MultiMap (use TupleMap)");
		}
	}

	return spec;
}

} // namespace schema