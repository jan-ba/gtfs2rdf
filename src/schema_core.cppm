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
#include <functional>

export module schema:core;
import rdf_components;
import field_transforms;


using namespace rdf;

namespace schema {

struct Datagap {
    size_t num_fields = 0;
    size_t num_transforms = 0;
    std::array<int, field_transforms::MaxArgs> column_indices;
    std::array<field_transforms::Transform, field_transforms::MaxTransforms> transforms;
    bool contains_transf2n = false;
    int transf2n_index = -1;
};

export class Instruction {
  private:
    std::vector<std::string> parts_;  // static parts between datagaps
    std::vector<Datagap> datagaps_;  // dynamic parts
    std::array<const std::string*, field_transforms::MaxArgs> arg_buf_;
    size_t base_len_ = 0;
    std::string out_;
    const std::string empty_ = "";
    size_t counter_ = 0;
    bool is_valid_ = true;  // set to false if instruction is invalid due to missing columns
    const field_transforms::TransformRegistry& registry_;
    const std::string raw_instruction_;
    bool contains_transf2n_ = false;
    std::vector<std::string> transf_buf_;  // buffer for transforms whose outputs span across several rows
    size_t transf2n_placeholder_index_ = 0;  // TODO: describe

  public:
    Instruction(const std::string& raw_instruction, const std::unordered_map<std::string, 
                int>& column_map, const field_transforms::TransformRegistry& registry) 
        : registry_(registry), raw_instruction_(raw_instruction)
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

            std::string field = raw_instruction.substr(pos + 1, end - pos - 1);
            field_transforms::ParsedPlaceholder pp = registry_.parse_placeholder_with_functors(field);

            if (pp.field_names.size() > field_transforms::MaxArgs) {
                throw std::runtime_error("❌  Error: too many function arguments in instruction: " 
                                          + raw_instruction + " (max " + std::to_string(field_transforms::MaxArgs) + ")");
            } 

            if (pp.transforms.size() > field_transforms::MaxTransforms) {
                throw std::runtime_error("❌  Error: too many chained transforms in instruction: " 
                                          + raw_instruction + " (max " + std::to_string(field_transforms::MaxTransforms) + ")");
            }

            Datagap dg;
            dg.num_fields = pp.field_names.size();
            dg.num_transforms = pp.transforms.size();
            
            for (size_t j = 0; j < pp.transforms.size(); ++j) {
                dg.transforms[j] = pp.transforms[j];
                if (pp.transforms[j].kind == field_transforms::TransformKind::Multi) {
                    if (contains_transf2n_ == true) {
                        throw std::runtime_error("❌  Error: only 1 Transform2N transform allowed in total in instruction: "
                                                 + raw_instruction);
                    }
                    contains_transf2n_ = true;
                    dg.contains_transf2n = true;
                    dg.transf2n_index = j;
                }
            }

            for (size_t j = 0; j < dg.num_fields; ++j) {
                const auto& column_name = pp.field_names[j];
                if (!column_map.contains(column_name)) {
                    throw std::runtime_error("❌  Error: unknown column in instruction: " + column_name);
                } else if (column_map.at(column_name) == -1) {
                    std::cerr << "⚠️  Warning: column '" << column_name << "' not found in header for triple: '" 
                              << raw_instruction << "' . Skipping this triple.\n";
                    is_valid_ = false;
                    return;
                }
                dg.column_indices[j] = column_map.at(column_name);
            }
            datagaps_.push_back(std::move(dg));
            start = end + 1; 
        }
        parts_.push_back(raw_instruction.substr(start));
        parts_.back().append("\n");
        base_len_ += parts_.back().size();
        
        // estimate output size
        size_t approx_dg_len = 0;
        for (const auto& dg : datagaps_) {
            approx_dg_len += 20 * dg.num_fields;
        }

        out_.reserve(base_len_ + approx_dg_len);
      }

      const std::string& render(const std::vector<std::string>& row) {
          out_.clear();
          out_.append(parts_[0]);

          for (size_t k = 0; k < datagaps_.size(); ++k) {
              const Datagap& dg = datagaps_[k];
              for (size_t j = 0; j < dg.num_fields; ++j) {
                  arg_buf_[j] = &row[dg.column_indices[j]];
                  if (arg_buf_[j]->empty()) {
                      // missing value -> return empty string
                      return empty_;
                  }
              }

              // apply transforms
              if (dg.num_transforms > 0) {
                  std::string c;
                  field_transforms::ArgSpan spanN {arg_buf_.data(), dg.num_fields};

                  // handle Transform2N if present
                  if (dg.contains_transf2n) {
                      transf_buf_.clear();
                      transf2n_placeholder_index_ = out_.size();
                      std::string d = c;
                      const std::string* d_ptr = &d;
                      field_transforms::ArgSpan span1 {&d_ptr, 1};

                      for (size_t i = 0; i < dg.transf2n_index; i++) {
                          if (i == 0) {
                              dg.transforms[i].single(spanN, c);
                          } else {
                              dg.transforms[i].single(span1, c);
                          }
                          d = c;
                      }
                      if (dg.transf2n_index == 0) {
                          dg.transforms[dg.transf2n_index].multi(spanN, transf_buf_);
                      } else { dg.transforms[dg.transf2n_index].multi(span1, transf_buf_); }

                      for (size_t i = dg.transf2n_index + 1; i < dg.num_transforms; i++) {
                          for (size_t j = 0; j < transf_buf_.size(); ++j) {
                              d = transf_buf_[j];
                              const std::string* d_ptr = &d;
                              field_transforms::ArgSpan span1 {&d_ptr, 1};
                              dg.transforms[i].single(span1, transf_buf_[j]);
                          }
                      }

                  // no Transform2N
                  } else {
                      dg.transforms[0].single(spanN, c);

                      if (dg.num_transforms > 1) {
                          // multiple transforms
                          std::string d = c;
                          const std::string* d_ptr = &d;
                          field_transforms::ArgSpan span1 {&d_ptr, 1};
                          for (size_t j = 1; j < dg.num_transforms; ++j) {
                              dg.transforms[j].single(span1, c);
                              d = c;
                          }
                      }

                      // TODO: what if c is empty? Is the resulting triple invalid then?
                      out_.append(c);
                  }
 
              } else {  // no transforms
                  out_.append(*arg_buf_[0]);
              }

              out_.append(parts_[k + 1]);
          }
        
          if (contains_transf2n_) {
              std::string prefix = out_.substr(0, transf2n_placeholder_index_);
              std::string suffix = out_.substr(transf2n_placeholder_index_);
              out_.clear();
              for (const auto& val : transf_buf_) {
                  out_.append(prefix);
                  out_.append(val);
                  out_.append(suffix);
              }
              counter_ += transf_buf_.size();
          } else {
              counter_++;
          }
          return out_;
      }


    int getCount() const { return counter_; }
    bool isValid() const { return is_valid_; }
    const std::string& getRawInstruction() const { return raw_instruction_; }
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
    const field_transforms::TransformRegistry& registry_;

    // computed from header
    std::unordered_map<std::string, int> column_map_;  // column name -> index in file, -1 if not found
    std::vector<Instruction> instructions_;  // computed instructions 

  public:
    Schema(const std::string name, const std::vector<std::string> possible_columns,
          const std::unordered_map<std::string, std::string> prefixes,
          const std::vector<Triple> triples, const field_transforms::TransformRegistry& registry,
          const bool outputTurtle = true, const bool usePrefixes = true, 
          const bool explicitRdfType = true)
        : name_(std::move(name)), possible_columns_(std::move(possible_columns)),
          prefixes_(std::move(prefixes)), registry_(registry),
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
                                                                          + " for " + name_);
        }
      }
      // build instructions_
      for (const auto& raw_inst : raw_instructions_) {
        Instruction instr(raw_inst, column_map_, registry_);
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