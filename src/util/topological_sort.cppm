// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the GTFS2RDF project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.

module;
#include <vector>
#include <unordered_map>
#include <string>
#include <stdexcept>

export module util:topological_sort;

export namespace util {

// Topological Sort using Kahn's Algorithm (https://en.wikipedia.org/wiki/Topological_sorting)
// Upon researching briefly, I couldn't find a plug-and-play TopoSort implementation without adding 
// a heavy dependency, so I implemented this simple version here. Due to its use of an adjacency 
// matrix for simplicity, performance will likely cause issues for large graphs, but for the use 
// cases in GTFS2RDF (dozens of nodes at most), this should be fine.
class TopologicalSort {
  private:
    // adjacency matrix
    std::vector<std::vector<bool>> A_;

    std::vector<std::pair<size_t, size_t>> edges_;
    
    // map from node name to index in adjacency matrix
    std::unordered_map<size_t, size_t> node_index_;
    std::vector<size_t> index_to_name_;

    std::vector<size_t> L_;
    std::vector<size_t> S_;

    bool isRowFalse(size_t row) {
        for (bool val : A_[row]) {
            if (val) return false;
        }
        return true;
    }

    bool isColFalse(size_t col) {
        for (const auto& row : A_) {
            if (row[col]) return false;
        }
        return true;
    }
  public:
    // enter estimated number of nodes to avoid reallocations for efficiency
    TopologicalSort(size_t num_nodes = 10) {
        edges_.reserve(num_nodes * 2);  // assume very sparse graph
        node_index_.reserve(num_nodes);
        index_to_name_.reserve(num_nodes);
        L_.reserve(num_nodes);
        S_.reserve(num_nodes);
    }

    void addEdge(size_t from, size_t to, bool allow_self_loops = false) {
        auto [it, res] = node_index_.emplace(from, node_index_.size());
        if (res) {
            index_to_name_.push_back(from);
        }
        auto [it2, res2] = node_index_.emplace(to, node_index_.size());
        if (res2) {
            index_to_name_.push_back(to);
        }

        if (from == to) {
            if (!allow_self_loops) {
                throw std::runtime_error("❌ TopologicalSort error: self-loop detected for node " + std::to_string(from));
            }
        } else {
            // don't add self-loops as edges, that would be pointless
            edges_.emplace_back(it->second, it2->second);
        }
    }

    // this can be used for nodes which might not have any edges but should still appear in the sorted output
    void addNode(size_t node) {
        auto [it, res] = node_index_.emplace(node, node_index_.size());
        if (res) {
            index_to_name_.push_back(node);
        }
    }

    std::vector<size_t> sort() {
        // initialise adjacency matrix
        A_ = std::vector<std::vector<bool>>(
            node_index_.size(), std::vector<bool>(node_index_.size(), false));
        for (const auto& [from_id, to_id] : edges_) {
            A_[from_id][to_id] = true;
        }

        L_.clear();
        S_.clear();

        // initialise set of nodes with no incoming edges
        for (size_t i = 0; i < A_.size(); ++i) {
            if (isColFalse(i)) {
                S_.push_back(i);
            }
        }

        while (!S_.empty()) {
            size_t n = S_.back();
            S_.pop_back();
            L_.push_back(n);

            // for each node m with an edge e from n to m
            for (size_t m = 0; m < A_.size(); ++m) {
                if (A_[n][m]) {
                    // remove edge e from the graph
                    A_[n][m] = false;
                    // if m has no other incoming edges then insert m into S
                    if (isColFalse(m)) {
                        S_.push_back(m);
                    }
                }
            }
        }

        // check for cycles
        for (size_t i = 0; i < A_.size(); ++i) {
            if (!isRowFalse(i)) {
                throw std::runtime_error("❌ TopologicalSort error: graph has at least one cycle");
            }
        }

        // build result
        std::vector<size_t> result;
        result.reserve(L_.size());
        for (size_t index : L_) {
            result.push_back(index_to_name_[index]);
        }
        return result;
    }
};

} // namespace