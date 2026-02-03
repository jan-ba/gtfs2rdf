// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the GTFS2RDF project.
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
export enum class ArgKind {
	Column,     // e.g. stop_id
	StorageVar, // e.g. FEED_LANG@feed_info.txt
	Literal     // e.g. "hardcoded"
};

// describes one argument in a placeholder,
// i.e. "{ arg1, arg2, ... | transform1 | transform2 > STORAGE }"
export struct ArgSpec {
	ArgKind kind;
	std::string name; // column name or variable name; for Literal: the literal text
	std::string ctx;  // only for StorageVar (and later maybe other storage reads)l
};

export enum class StoreMode {
	None,           // no storage write (normal FILE placeholder)
	StoreComputed,  // store transform output (no (...) on value side)
	FilterStoreRaw, // (...) present -> transforms act as filter, store raw tuple
	StoreRaw        // store raw args without transforms
};

export enum class StorageKind {
	None,     // FILE output
	Variable, // > CONST
	MultiMap, // key -> vector<string> with quick lookup
	TupleMap  // key -> vector<vector<string...>>
};

export struct StorageWriteSpec {
	StorageKind kind = StorageKind::None;
	StoreMode mode = StoreMode::None;

	std::string target_name; // e.g. FEED_LANG, removed_dates
	std::string
	    target_ctx; // optional explicit ctx from > TARGET@ctx; else filled later with schema ctx

	// partitioning of args for keyed storage:
	// args = [keys..., values..., extra...]
	uint8_t key_arity = 0;
	uint8_t value_arity = 0;
	uint8_t extra_arity = 0;

	// convenience
	bool is_keyed() const {
		return kind == StorageKind::MultiMap || kind == StorageKind::TupleMap;
	}
};

export struct TransformCallSpec {
	field_transforms::Transform transform; // your existing functor wrapper (Single/Multi)
	std::string ctx_hint; // @ctx on the transform name (dependency + later transform access)
};

export struct PlaceholderSpec {
	std::vector<ArgSpec> args;                 // includes columns, literals, storage const reads
	std::vector<TransformCallSpec> transforms; // functor + optional context
	StorageWriteSpec storage;                  // what to do with output / whether to store
};

export enum class ArgSourceKind { ColumnIndex, StorageVar, Literal };

export struct ArgSource {
	ArgSourceKind kind;
	size_t column_index = -1; // ColumnIndex: if valid, index in CSV row
	std::string literal;
	std::string name; // StorageVar: variable name
	std::string ctx;  // StorageVar: context name
};

export struct BoundPlaceholder {
	std::vector<ArgSource> args;
	std::vector<TransformCallSpec> transforms;
	StorageWriteSpec storage;
};

ArgSpec parse_arg(std::string_view tok) {
	if (tok.empty()) {
		throw diagnostics::Error("Syntax error: Empty argument field");
	}

	// literal
	if (tok.front() == '"') {
		return ArgSpec{ArgKind::Literal, util::unquote(tok), ""};
	}

	// storage variable read: NAME@ctx
	auto [lhs, rhs] = util::split_at(tok, '@');
	if (!rhs.empty()) {
		if (!valid_ctx_name(rhs)) {
			throw diagnostics::Error("Syntax error: Invalid context name after '@' in arg: " +
			                         std::string(tok));
		}
		return ArgSpec{ArgKind::StorageVar, std::string(lhs), std::string(rhs)};
	}

	// column
	return ArgSpec{ArgKind::Column, std::string(tok), ""};
}

TransformCallSpec parse_transform(std::string_view tok,
                                  const field_transforms::TransformRegistry &reg) {
	if (tok.empty())
		throw diagnostics::Error("Syntax error: Empty transform field");

	auto [name, ctx] = util::split_at(tok, '@');
	TransformCallSpec out;
	out.transform = reg.getTransform(std::string(name));
	if (!ctx.empty()) {
		if (!valid_ctx_name(ctx)) {
			throw diagnostics::Error("Syntax error: Invalid context name after '@' in transform: " +
			                         std::string(tok));
		}
		out.ctx_hint = std::string(ctx);
	}
	return out;
}

static void parse_target(std::string_view tok, StorageWriteSpec &stg) {
	auto [name, ctx] = util::split_at(tok, '@');
	if (name.empty())
		throw diagnostics::Error("Syntax error: Empty storage target after '>'");
	stg.target_name = std::string(name);
	if (!ctx.empty()) {
		if (!valid_ctx_name(ctx)) {
			throw diagnostics::Error(
			    "Syntax error: Invalid context name after '@' in storage target: " +
			    std::string(tok));
		}
		stg.target_ctx = std::string(ctx);
	}
}

// ---------- main parser ----------

export PlaceholderSpec parse_placeholder(std::string_view raw,
                                         const field_transforms::TransformRegistry &reg) {
	// normalise whitespace but keep spaces in quoted literals
	std::string cleaned = util::remove_ws_outside_quotes(raw);
	std::string_view s(cleaned);

	PlaceholderSpec spec;

	// split off storage target: "... > target"
	// only at most one '>' supported at top level
	auto [before_gt, after_gt] = util::split_once_top_level(s, '>');
	if (!after_gt.empty()) {
		// storage is enabled, kind/mode decided below
		parse_target(after_gt, spec.storage);
	}

	// split remainder into fields part and transforms part: "fields | t1 | t2"
	auto [fields_part, trans_part] = util::split_once_top_level(before_gt, '|');

	// transforms
	if (!trans_part.empty()) {
		auto tks = util::split_top_level(trans_part, '|');
		for (auto t : tks) {
			spec.transforms.push_back(parse_transform(t, reg));
		}
	}

	// detect keyed storage (':') at top level: "key1,key2 : (val1,val2),extra1,extra2"
	auto [left_of_colon, right_of_colon] = util::split_once_top_level(fields_part, ':');
	const bool keyed = !right_of_colon.empty();

	if (keyed) {
		if (spec.storage.target_name.empty()) {
			throw diagnostics::Error(
			    "Syntax error: Keyed placeholder with ':' requires storage target after '>')");
		}

		// keys
		auto key_toks = util::split_top_level(left_of_colon, ',');
		if (key_toks.empty())
			throw diagnostics::Error("Syntax error: Missing key fields before ':'");
		for (auto tk : key_toks)
			spec.args.push_back(parse_arg(tk));
		spec.storage.key_arity = static_cast<uint8_t>(key_toks.size());

		// right side: either "(values),extras" (filter mode) OR "value_inputs" (compute mode)
		if (!right_of_colon.empty() && right_of_colon.front() == '(') {
			// FILTER MODE
			spec.storage.mode = StoreMode::FilterStoreRaw;

			// find matching ')'
			bool in_q = false;
			int par = 0;
			size_t close = std::string_view::npos;
			for (size_t i = 0; i < right_of_colon.size(); ++i) {
				char c = right_of_colon[i];
				if (c == '"' && (i == 0 || right_of_colon[i - 1] != '\\'))
					in_q = !in_q;
				if (in_q)
					continue;
				if (c == '(')
					++par;
				else if (c == ')') {
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

			auto val_toks = util::split_top_level(inside, ',');
			if (val_toks.empty())
				throw diagnostics::Error("Syntax error: Empty value tuple '(...)' after ':'");
			for (auto tk : val_toks)
				spec.args.push_back(parse_arg(tk));
			spec.storage.value_arity = static_cast<uint8_t>(val_toks.size());

			// extras
			if (!after.empty()) {
				if (after.front() != ',') {
					throw diagnostics::Error(
					    "Syntax error: Expected ',' after ')' for extras in placeholder");
				}
				auto extras = after.substr(1);
				if (!extras.empty()) {
					auto ex_toks = util::split_top_level(extras, ',');
					for (auto tk : ex_toks)
						spec.args.push_back(parse_arg(tk));
					spec.storage.extra_arity = static_cast<uint8_t>(ex_toks.size());
				}
			}

			// kind: multimap if 1 value, else tuplemap
			spec.storage.kind =
			    (spec.storage.value_arity == 1) ? StorageKind::MultiMap : StorageKind::TupleMap;

			// disallow Transform2Many in filter-mode
			for (auto &tc : spec.transforms) {
				if (tc.transform.kind == field_transforms::TransformKind::Multi) {
					throw diagnostics::Error("Syntax error: Transform2Many not allowed with "
					                         "filter-mode '(...)' in placeholder");
				}
			}

		} else {
			// RHS without (...) : either
			//  - StoreRaw (no transforms): store RHS tuple directly (MultiMap if 1 value else
			//  TupleMap)
			//  - StoreComputed (has transforms): store transform output (TupleMap if Transform2Many
			//  involved)

			auto rhs_toks = util::split_top_level(right_of_colon, ',');
			if (rhs_toks.empty())
				throw diagnostics::Error("Syntax error: Missing value inputs after ':'");
			for (auto tk : rhs_toks)
				spec.args.push_back(parse_arg(tk));

			spec.storage.value_arity = static_cast<uint8_t>(rhs_toks.size());
			spec.storage.extra_arity = 0;

			bool has_multi = false;
			for (const auto &tc : spec.transforms) {
				if (tc.transform.kind == field_transforms::TransformKind::Multi) {
					has_multi = true;
					break;
				}
			}

			if (spec.transforms.empty()) {
				// raw store, no filtering
				spec.storage.mode = StoreMode::StoreRaw;
				spec.storage.kind =
				    (spec.storage.value_arity == 1) ? StorageKind::MultiMap : StorageKind::TupleMap;
			} else {
				// computed store
				spec.storage.mode = StoreMode::StoreComputed;
				// Transform2Many output is a vector => must be TupleMap
				spec.storage.kind = has_multi ? StorageKind::TupleMap : StorageKind::MultiMap;
			}
		}

	} else {
		// non-keyed (variables or file output)
		auto arg_toks = util::split_top_level(fields_part, ',');
		for (auto tk : arg_toks)
			spec.args.push_back(parse_arg(tk));

		if (!spec.storage.target_name.empty()) {
			// variable store
			spec.storage.kind = StorageKind::Variable;
			spec.storage.mode = StoreMode::StoreComputed;
		} else {
			spec.storage.kind = StorageKind::None;
			spec.storage.mode = StoreMode::None;
		}
	}

	// --- ensure correctness of the parsed spec ---

	if (spec.args.empty()) {
		throw diagnostics::Error("Syntax error: Placeholder must have at least 1 argument");
	}

	// count Transform2Many occurrences
	size_t num_multi = 0;
	for (const auto &tc : spec.transforms) {
		if (tc.transform.kind == field_transforms::TransformKind::Multi)
			++num_multi;
	}
	if (num_multi > 1) {
		throw diagnostics::Error("Syntax error: Only one Transform2Many allowed per placeholder");
	}

	const bool has_transforms = !spec.transforms.empty();
	const bool is_keyed = spec.storage.is_keyed();

	// non-keyed placeholders: multi-arg is only allowed if transforms exist
	if (!is_keyed) {
		if (!has_transforms && spec.args.size() != 1) {
			throw diagnostics::Error(
			    "Syntax error: Non-keyed placeholder with multiple args requires transforms");
		}
	}

	// FilterStoreRaw should actually have transforms (otherwise there's not a filter)
	if (spec.storage.mode == StoreMode::FilterStoreRaw && !has_transforms) {
		throw diagnostics::Error(
		    "Syntax error: FilterStoreRaw '(...)' requires at least one filter transform");
	}

	// Transform2Many must never target Variable or MultiMap
	if (num_multi == 1) {
		if (spec.storage.kind == StorageKind::Variable) {
			throw diagnostics::Error("Syntax error: Transform2Many cannot store into Variable");
		}
		if (spec.storage.kind == StorageKind::MultiMap) {
			throw diagnostics::Error(
			    "Syntax error: Transform2Many cannot store into MultiMap (use TupleMap)");
		}
	}

	return spec;
}

} // namespace schema