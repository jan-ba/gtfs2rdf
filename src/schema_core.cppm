// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the GTFS2RDF project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.


module;

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <stdexcept>
#include <iostream>

export module schema.core;
import rdf_components;

using namespace rdf;

namespace schema {

export class Instruction {
  private:
    std::vector<std::string> parts_;
    std::vector<int> column_indices_;
    size_t base_len_ = 0;
    std::string out_;
    const std::string empty_ = "";
    int counter_ = 0;
    bool is_valid_ = true;  // set to false if instruction is invalid due to missing columns

  public:
    Instruction(const std::string& raw_instruction, const std::unordered_map<std::string, 
                int>& column_map) 
      {
        // parse raw_instruction into parts and column_indices
        size_t pos = 0;
        size_t start = 0;
        while ((pos = raw_instruction.find('{', start)) != std::string::npos) {
            size_t end = raw_instruction.find('}', pos);
            if (end == std::string::npos) {
                throw std::runtime_error("❌  Error: malformed instruction: " + raw_instruction);
            }
            parts_.push_back(raw_instruction.substr(start, pos - start));
            base_len_ += parts_.back().size();
            std::string column_name = raw_instruction.substr(pos + 1, end - pos - 1);
            if (!column_map.contains(column_name)) {
                throw std::runtime_error("❌  Error: unknown column in instruction: " + column_name);
            } else if (column_map.at(column_name) == -1) {
                std::cerr << "⚠️  Warning: column '" << column_name << "' not found in header for triple: '" 
                          << raw_instruction << "' . Skipping this triple.\n";
                is_valid_ = false;
                return;
            }
            column_indices_.push_back(column_map.at(column_name));
            start = end + 1;
        }
        parts_.push_back(raw_instruction.substr(start));
        parts_.back().append("\n");
        base_len_ += parts_.back().size();
        // at a later stage: better approximation by incorporating whether prefix on / off 
        // or depending on type of column
        out_.reserve(base_len_ + 20 * column_indices_.size());
      }

    const std::string& render(const std::vector<std::string>& row) {
        out_.clear();
        out_.append(parts_[0]);
        for (std::size_t k = 0; k < column_indices_.size(); ++k) {
            const std::string& c = row[column_indices_[k]];
            if (c.empty()) {
                // missing value -> return empty string
                return empty_;
            }
            out_.append(c);
            out_.append(parts_[k + 1]);
        }
        counter_++;
        return out_;
    }

    int getCount() const { return counter_; }
    bool isValid() const { return is_valid_; }
};

export class Schema {
  private:
    const std::string name_ = "";                // name of file with file type, e.g. "stops.txt"
    const std::vector<std::string> possible_columns_ = {}; // all columns that could be contained by <name_>.txt  TODO: actually needed?
    std::unordered_map<std::string, std::string> prefixes_;
    std::vector<std::string> raw_instructions_;
    const bool outputTurtle_ = true;        // ttl vs. ntriples
    const bool usePrefixes_ = true;         // @prefix Header
    const bool explicitRdfType_ = true;     // if true, no 'a', ',' , ';' syntactic ttl sugar

    // computed from header
    std::unordered_map<std::string, int> column_map_;  // column name -> index in file, -1 if not found
    std::vector<Instruction> instructions_;  // computed instructions 

  public:
    // ~Schema() {}
    // Schema() {}
    Schema(const std::string name, const std::vector<std::string> possible_columns,
          const std::unordered_map<std::string, std::string> prefixes,
          const std::vector<Triple> triples,
          const bool outputTurtle = true, const bool usePrefixes = true, 
          const bool explicitRdfType = true)
        : name_(std::move(name)), possible_columns_(std::move(possible_columns)),
          prefixes_(std::move(prefixes)),
          outputTurtle_(outputTurtle), usePrefixes_(usePrefixes),
          explicitRdfType_(explicitRdfType) {
      for (const auto& col : this->possible_columns_) {
        column_map_[col] = -1; // initialize all to -1 (not found)
      }
      
      bool turtAndpref = outputTurtle && usePrefixes;  // only ttl allows prefixes
        if (!outputTurtle && usePrefixes) {
          std::cerr << "⚠️  Warning: cannot use prefixes in ntriples output. Ignoring prefixes.\n";
        }
      // build raw_instructions_ from triples
      for (const auto& triple : triples) {
        raw_instructions_.push_back(triple.toString(prefixes_, outputTurtle_, turtAndpref, explicitRdfType_));
      }
    }


    void setHeader(const std::vector<std::string>& header) {
      // compute column_map_ from header
      for (size_t file_idx = 0; file_idx < header.size(); ++file_idx) {
        if (column_map_.contains(header[file_idx])) {
            column_map_[header[file_idx]] = static_cast<int>(file_idx);
        } else {
            throw std::runtime_error("❌  Error: unknown column " + header[file_idx] 
                                                                          + "for " + name_);
        }
      }
      // build instructions_
      for (const auto& raw_inst : raw_instructions_) {
        Instruction instr(raw_inst, column_map_);
        if (!instr.isValid()) {
            continue; // skip invalid instructions (due to missing columns)
        }
        instructions_.push_back(instr);
      }
    }

    // Getters
    const std::string& getName() const { return name_; }
    const std::vector<std::string>& getPossibleColumns() const { return possible_columns_; }
    bool isTurtle() const { return outputTurtle_; }
    bool isPrefixes() const { return usePrefixes_; }
    bool isExplicitRdfType() const { return explicitRdfType_; }
    const std::unordered_map<std::string, std::string>& getPrefixes() const { return prefixes_; }
    const std::unordered_map<std::string, int>& getColumnMap() const { return column_map_; }
    const std::vector<Instruction>& getInstructions() const { return instructions_; }

    // Setters
    void setPrefixes(std::unordered_map<std::string, std::string> prefixes) {
      prefixes_ = prefixes;
    }
};

export std::unordered_map<std::string, std::string> merge_prefixes(const std::vector<Schema>& schemas, 
                                                            bool strict_conflicts = true) {
  std::unordered_map<std::string, std::string> out;

  for (const auto& sc : schemas) {
    const auto& pfx = sc.getPrefixes();
    for (const auto& [k, v] : pfx) {
      if (auto it = out.find(k); it == out.end()) {
        out.emplace(k, v);
      } else if (it->second != v) {
        if (strict_conflicts) {
          throw std::runtime_error(
            "Prefix conflict for '" + k + "': '" + it->second +
            "' vs '" + v + "'");
        } else {
            
        }
      }
    }
  }
  return out;
}

} // namespace