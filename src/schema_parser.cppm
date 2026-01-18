// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the GTFS2RDF project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.


module;

#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <cctype>
#include <cstdint>

export module schema_parser;
import field_transforms;

namespace schema {

export enum class ArgKind {
  Column,     // e.g. stop_id
  StorageConst, // e.g. FEED_LANG@feed_info.txt
  Literal     // e.g. "hardcoded"
};

// describes one argument in a placeholder, 
// i.e. "{ arg1, arg2, ... | transform1 | transform2 > STORAGE }"
export struct ArgSpec {
  ArgKind kind;
  std::string name;   // column name or constant name; for Literal: the literal text
  std::string ctx;    // only for StorageConst (and later maybe other storage reads)
};

export enum class StoreMode {
  None,           // no storage write (normal FILE placeholder)
  StoreComputed,  // store transform output (no (...) on value side)
  FilterStoreRaw  // (...) present -> transforms act as filter, store raw tuple
};

export enum class StorageKind {
  None,      // FILE output
  Constant,  // > CONST
  MultiMap,  // key -> vector<string> with quick lookup
  TupleMap   // key -> vector<vector<string...>>
};

export struct StorageWriteSpec {
  StorageKind kind = StorageKind::None;
  StoreMode mode = StoreMode::None;

  std::string target_name;  // e.g. FEED_LANG, removed_dates
  std::string target_ctx;   // optional explicit ctx from > TARGET@ctx; else filled later with schema ctx

  // partitioning of args for keyed storage:
  // args = [keys..., values..., extra...]
  uint8_t key_arity = 0;
  uint8_t value_arity = 0;
  uint8_t extra_arity = 0;

  // convenience
  bool is_keyed() const { return kind == StorageKind::MultiMap || kind == StorageKind::TupleMap; }
};

export struct TransformCallSpec {
  field_transforms::Transform transform;     // your existing functor wrapper (Single/Multi)
  std::string ctx_hint;    // @ctx on the transform name (dependency + later transform access)
};

export struct PlaceholderSpec {
  std::vector<ArgSpec> args;                 // includes columns, literals, storage const reads
  std::vector<TransformCallSpec> transforms; // functor + optional context
  StorageWriteSpec storage;                  // what to do with output / whether to store
};

export enum class ArgSourceKind { ColumnIndex, StorageConst, Literal };

export struct ArgSource {
  ArgSourceKind kind;
  int column_index = -1;          // ColumnIndex
  const std::string* literal = nullptr; // Literal (points into InstructionTemplate-owned pool)
  std::string name;               // StorageConst: constant name
  std::string ctx;                // StorageConst: context name
};

export struct BoundPlaceholder {
  std::vector<ArgSource> args;
  std::vector<TransformCallSpec> transforms;
  StorageWriteSpec storage;
};




} // namespace schema