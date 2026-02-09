// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the gtfs2rdf project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.

/*
================================================================================
gtfs2rdf — Schema Template (copy into <your_schema>.cppm, then adapt)
================================================================================

This template is meant as a quick starting point to define a new GTFS schema in
order to support another GTFS file with this converter.

For built-in/library transforms, see: transform_lib.cppm (imported via `import t_lib;`).

------------------------------------------------------------------------------
1) Placeholders: `{ ... }` (args + transform chain + optional storage)
------------------------------------------------------------------------------
In instruction strings, `{...}` is a placeholder evaluated per GTFS row.

General form:
  { arg1,arg2,... | transform1 | transform2 | ... > STORAGE_TARGET }

Whitespace outside of quoted literals is ignored:
  "{ stop_id , \"x\" | some_trf }"  ==  "{stop_id,\"x\"|some_trf}"

Limits (see field_transforms, can be adjusted if needed):
  - MAX_ARGS       (currently 10)
  - MAX_TRANSFORMS (currently 3)

Empty output acts as a filter:
  - If a placeholder yields an empty string, the WHOLE instruction
    is skipped (no triple emitted, and no further placeholders are processed).

Transform2Many duplicates the whole instruction:
  - The instruction is emitted once per non-empty element produced by a Transform2Many.
  - Limitation: within ONE instruction string, at most ONE Transform2Many may appear
    (across all placeholders in that instruction).

------------------------------------------------------------------------------
2) Transforms and chaining semantics
------------------------------------------------------------------------------
A placeholder may have 0..MAX_TRANSFORMS chained transforms.

Dataflow rule:
  - The FIRST transform sees ALL placeholder arguments.
  - Every subsequent transform sees ONLY the predecessor output:
      transform2 gets ARGS[0] == output_of_transform1, etc.

Example:
  "{ a,b | join_ab | normalise }"
    join_ab(ARGS=[a,b])      -> "a-b"
    normalise(ARGS=["a-b"])  -> "A-B"

If you want later steps to access original inputs again, do it inside one transform.
This will usually also be more efficient.

--- Transform2Many inside a chain ---
Transform2Many “fans out” the pipeline into multiple values:

  - Transforms BEFORE the MANY behave normally (first sees N, later sees 1).
  - The MANY transform itself sees:
      * all N args if it is the first transform, otherwise just the predecessor output (size 1).
  - It produces vector<string>.
  - Any transforms AFTER the MANY are applied element-wise:
      each element becomes ARGS[0] to the next Transform2One.

Example:
  { SUBJ, {"gtfs","tag"}, {"{tags|split_semicolon|trim|normalise}"} }

For a row with:
  tags = " a ;b; C "

Execution:
  split_semicolon(ARGS=[tags]) -> [" a ", "b", " C "]
  trim (element-wise)          -> ["a","b","C"]
  normalise (element-wise)     -> ["A","B","C"]

Resulting emitted triples (same subject + predicate, only object differs):
  <SUBJ> gtfs:tag "A" .
  <SUBJ> gtfs:tag "B" .
  <SUBJ> gtfs:tag "C" .

------------------------------------------------------------------------------
3) Defining transforms (preferred: macros)
------------------------------------------------------------------------------
Define schema-local transforms inside build<Schema>() using transform_macros.h:

  TRANSFORM2ONE(name, ARGS, OUT_VAL, STORAGE) {
    ...
  }
  TRANSFORM_END

  TRANSFORM2MANY(name, ARGS, OUT_VAL, STORAGE) {
    ...
  }
  TRANSFORM_END

IMPORTANT:
  - ARGS is NOT bounds-checked. Always check i < ARGS.size() before ARGS[i].
  - If the args are wrong, use the error macro to print a meaningful message:
      TRANSFORM_ERROR("message")
  - Returning with OUT_VAL empty means “filter out / skip” (see §1).

STORAGE is available to transforms (for reading variables/maps written by storage placeholders).

------------------------------------------------------------------------------
4) Argument kinds (what you can write before the first `|`)
------------------------------------------------------------------------------
(A) Column from the current GTFS row:
    { stop_id }
    { stop_lon,stop_lat | some_trf }

(B) Literal:
    { "hardcoded" }
  IMPORTANT: inside a C++ string, quotes must be escaped:
    "{ \"hardcoded\" }"
  Spaces inside the literal are kept.

(C) Storage variable read:
    { FEED_LANG@feed_info.txt }
  Syntax: <var_name>@<context_name_ending_in_.txt>
  Reads a VARIABLE from storage (maps/tuples can be accessed from transforms via STORAGE).

------------------------------------------------------------------------------
5) Writing to storage (use `>` inside the placeholder)
------------------------------------------------------------------------------
Storage writes happen as a side effect during rendering (before output escaping/writing).

In general, storage uses:
  - VARIABLE: one string value
  - MULTI_MAP: key -> vector<string>
  - TUPLE_MAP: key -> vector<tuple<string...>>

A storage target is written as:
  > NAME@<ctx_ending_with_txt>
`@<ctx_ending_with_txt>` must match the current schema context.

(A) Variable store (non-keyed, no ':'):
    { value_expr | transforms... > FEED_LANG@feed_info.txt }
  - Stores the computed output into a VARIABLE.
  - Read later from any schema via: { FEED_LANG@feed_info.txt }

(B) Keyed store (requires ':'):
    { key_fields : rhs_fields | transforms... > MAP_NAME@my_schema.txt }

  Args presented to the FIRST transform are:
    [ key_fields..., rhs_fields... ]  (plus possible extras in filter mode)

  Modes:
  (B1) STORE_RAW (keyed, NO transforms):
      { shape_id : shape_pt_sequence,shape_pt_lon,shape_pt_lat > shapes@shapes.txt }
    - Stores RHS tuple directly (unconditionally).
    - MULTI_MAP if RHS has 1 value, else TUPLE_MAP.

  (B2) STORE_COMPUTED (keyed, HAS transforms):
      { service_id : date,exception_type | is_disabled_date | convertDate2xs_unchecked
        > disabled_dates@calendar_dates.txt }
    - Stores the final computed output:
        * MULTI_MAP if final output is single string
        * TUPLE_MAP if a Transform2Many is involved (stores the produced vector as a tuple)

  (B3) FILTER_STORE_RAW (parentheses on RHS):
      { key : (raw_value1,raw_value2), extra_input | filter_trf | ... >
some_map@<ctx_ending_with_txt> }
    - Transforms act as predicate:
        non-empty computed output  => store RAW tuple from inside `( ... )`
    - Transform2Many is not allowed in this mode.

Reminder: empty output can also skip triple writing entirely (see §1). Storage-only
instructions are the usual way to do “pure storage setup” (see §6).

------------------------------------------------------------------------------
6) NO_WRITE_INSTRUCTIONS run first
------------------------------------------------------------------------------
If you pass NO_WRITE_INSTRUCTIONS to Schema(...), they are executed BEFORE all
triple-generation instructions for that schema.

Use this for initializing variables/maps used later in triples (in this or other schemas).
(B1) is a perfect example for when a storage-only makes sense: We're interested in having
the shapes available later, but we don't want to emit any triple directly from shapes.txt.

------------------------------------------------------------------------------
7) Using storage in transforms (get / contains)
------------------------------------------------------------------------------
You typically:
  1) write to storage via NO_WRITE_INSTRUCTIONS (or placeholders with `>`), and then
  2) read/check that storage from inside transforms via the `STORAGE` object.

Always use explicit contexts:
  - When reading a VARIABLE in a placeholder, always write: VAR_NAME@<ctx_ending_with_txt>
  - When calling a transform that reads from storage, always annotate the transform call:
        "{ ... | my_transform@<ctx_ending_with_txt> }"
    This makes the dependency clear and avoids ordering surprises.

VARIABLES (single string):
  - storeVariable(ctx, name, value)   (usually via a placeholder `>`, but available in code too)
  - getVariable(ctx, name)            (missing => empty string)

MULTI_MAP (key -> vector<string>):
  - storeValue(ctx, name, key_parts..., value)
  - getValues(ctx, name, key_parts...)            (missing => empty vector)
  - containsValue(ctx, name, key_parts..., value) (missing value => false)

TUPLE_MAP (key -> vector<tuple<string...>>):
  - storeTuple(ctx, name, key_parts..., tuple_parts...)
  - getTuples(ctx, name, key_parts...)            (missing => empty vector)
  - containsTuple(ctx, name, key, tuple_parts)    (missing tuple => false)

Keep it simple:
  - Use MULTI_MAP when you want “key -> many strings”
  - Use TUPLE_MAP when you want “key -> many tuples”

================================================================================
*/

module;

#include "transform_macros.h"

#include <string>
#include <unordered_map>
#include <vector>

export module schema:schema_template; // CHANGE schema_template -> your schema module name

import :core;
import rdf_components;
import field_transforms;
import t_lib; // library transforms (see transform_lib.cppm)
import runtime;

using namespace rdf;

namespace schema {

// CHANGE buildSchemaTemplateSchema -> build<YourSchemaName>Schema
export Schema buildSchemaTemplateSchema(runtime::RuntimeContainer& rtc) {
	// -------------------------------------------------------------------------
	// (1) Schema-local transforms (minimal but demonstrative)
	// -------------------------------------------------------------------------

	// Transform2One: (a,b) -> "a-b"
	TRANSFORM2ONE(exJoinAB, ARGS, OUT_VAL, STORAGE) {
		if (ARGS.size() != 2) {
			TRANSFORM_ERROR("expected 2 args (a,b)");
		}
		if (ARGS[0].empty() || ARGS[1].empty())
			return;
		OUT_VAL = std::string(ARGS[0]) + "-" + std::string(ARGS[1]);
	}
	TRANSFORM_END

	// Transform2One: ASCII upper-case; in a chain this sees exactly one pipeline value.
	TRANSFORM2ONE(exToUpperAscii, ARGS, OUT_VAL, STORAGE) {
		if (ARGS.size() != 1) {
			TRANSFORM_ERROR("expected 1 arg (pipeline value)");
		}
		OUT_VAL = std::string(ARGS[0]);
		for (char& c : OUT_VAL) {
			if ('a' <= c && c <= 'z')
				c = static_cast<char>(c - 'a' + 'A');
		}
	}
	TRANSFORM_END

	// Transform2Many: split "a;b;c" -> ["a","b","c"]
	TRANSFORM2MANY(exSplitSemicolon, ARGS, OUT_VAL, STORAGE) {
		if (ARGS.size() != 1) {
			TRANSFORM_ERROR("expected 1 arg");
		}
		std::string cur;
		for (char ch : ARGS[0]) {
			if (ch == ';') {
				if (!cur.empty())
					OUT_VAL.push_back(cur);
				cur.clear();
			} else {
				cur.push_back(ch);
			}
		}
		if (!cur.empty())
			OUT_VAL.push_back(cur);
	}
	TRANSFORM_END

	// Storage read (MULTI_MAP) + Transform2Many: aliases[id] -> many strings -> many triples
	TRANSFORM2MANY(exLookupAliases, ARGS, OUT_VAL, STORAGE) {
		if (ARGS.size() != 1) {
			TRANSFORM_ERROR("expected 1 arg (id)");
		}
		if (ARGS[0].empty())
			return;
		OUT_VAL = STORAGE.getValues("schema_template.txt", "aliases", {ARGS[0]});
	}
	TRANSFORM_END

	// -------------------------------------------------------------------------
	// (2) Columns that may appear in <your_schema>.txt
	// -------------------------------------------------------------------------
	const std::vector<std::string> POSSIBLE_COLUMNS = {
	    "id",
	    "name",
	    "a",
	    "b",
	    "tags",
	    "feed_lang",
	};

	// -------------------------------------------------------------------------
	// (3) Prefixes
	// -------------------------------------------------------------------------
	const std::unordered_map<std::string, std::string> PREFIXES = {
	    {"ex", "https://example.org/resource/"},
	    {"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
	    {"xs", "http://www.w3.org/2001/XMLSchema#"},
	    {"gtfs", "https://w3id.org/gtfs2rdf#"},
	};

	// -------------------------------------------------------------------------
	// (4) Triples (normal file output)
	// -------------------------------------------------------------------------
	const IRI SUBJ = IRI("ex", "{id}");

	const std::vector<Triple> TRIPLES = {
	    {SUBJ, {"rdf", "type"}, {IRI("gtfs", "YourClass")}},

	    // Transform2One chain:
	    {SUBJ, {"gtfs", "joinedUpper"}, {"{a,b|exJoinAB|exToUpperAscii}"}},

	    // Transform2Many + element-wise continuation:
	    // triple remains the same; only the placeholder value changes, hence multiple triples.
	    {SUBJ, {"gtfs", "tag"}, {"{tags|exSplitSemicolon|exToUpperAscii}"}},

	    // Storage-backed fan-out (MULTI_MAP -> many values -> many triples):
	    {SUBJ, {"gtfs", "alias"}, {"{id|exLookupAliases@schema_template.txt}"}},
	};

	// -------------------------------------------------------------------------
	// (5) Storage-only instructions (executed BEFORE TRIPLES for this schema)
	// -------------------------------------------------------------------------
	// These are best used to build reusable “indexes” for OTHER schemas.
	// Other schemas can consume them via @schema_template.txt (as a placeholder arg)
	// or via STORAGE.get... inside transforms.

	const std::vector<std::string> NO_WRITE_INSTRUCTIONS = {
	    // Global metadata (VARIABLE): useful across the whole dataset.
	    // Consumer (any schema): {FEED_LANG@schema_template.txt}
	    "{ feed_lang | exToUpperAscii > FEED_LANG@schema_template.txt }",

	    // Lookup index (MULTI_MAP): id -> [name, ...] for later joins.
	    // Consumer (any schema): "{id|exLookupAliases@schema_template.txt}"
	    "{ id : name > aliases@schema_template.txt }",
	};

	return Schema(
	    "schema_template.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, NO_WRITE_INSTRUCTIONS, rtc);
}

} // namespace schema

// storage writes sind ebenfalls zeilenweise bloß eben vor den triple writes.
// engine takes care of escaping depending on type (literal, IRI etc.)
// users may also want to make use of functions for string operations in util/strings.cppm