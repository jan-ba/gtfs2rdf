// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the GTFS2RDF project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.


module;
#include <vector>
#include <iostream>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <string>
#include <string_view>

export module util;

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

    void addEdge(size_t from, size_t to) {
        if (from == to) {
            throw std::runtime_error("❌ TopologicalSort error: self-loop detected for node '" 
                                        + std::to_string(from) + "'");
        }
        auto [it, res] = node_index_.emplace(from, node_index_.size());
        if (res) {
            index_to_name_.push_back(from);
        }
        auto [it2, res2] = node_index_.emplace(to, node_index_.size());
        if (res2) {
            index_to_name_.push_back(to);
        }
        edges_.emplace_back(it->second, it2->second);
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

// _________________________________________________________________________________________________
// std::string utility functions
// _________________________________________________________________________________________________

// splits a string by a given delimiter character
std::vector<std::string> split(const std::string& str, const char delimiter) {
    std::vector<std::string> tokens;
    std::string current;

    for (char c : str) {
        if (c == delimiter) {
            tokens.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }

    // add the last token (even if it's empty)
    tokens.push_back(current);

    return tokens;
}

// removes whitespace inplace from a string
void remove_whitespace(std::string& str) {
    str.erase(std::remove_if(str.begin(), str.end(), 
                             [](unsigned char c) { return std::isspace(c); }), 
                             str.end());
}

// returns a concatenation string of a vector of strings
std::string concat(const std::vector<std::string>& vec, const std::string& delimiter = "") {
    std::string result;
    for (size_t i = 0; i < vec.size(); ++i) {
        result += vec[i];
        if (i < vec.size() - 1) {
            result += delimiter;
        }
    }
    return result;
}

// returns all occurences of substrings that are enclosed between 'start_delim' and 'end_delim'
// throws an error if delimiters are unbalanced
// invariant: nested delimiters are not supported
std::vector<std::string> extract_enclosed_substrings(const std::string& str,
    const std::string& start_delim, const std::string& end_delim) 
{
    std::vector<std::string> results;

    // if there is an end delimiter but no start delimiter at all -> unbalanced
    if (str.find(end_delim) != std::string::npos &&
        str.find(start_delim) == std::string::npos) {
        throw std::runtime_error("❌  Error: unbalanced delimiters in string: " + str);
    }

    size_t start_search = 0;
    size_t last_consumed = 0;

    while (true) {
        size_t pos = str.find(start_delim, start_search);
        if (pos == std::string::npos) break;

        size_t end = str.find(end_delim, pos + start_delim.size());
        if (end == std::string::npos) {
            throw std::runtime_error("❌  Error: unbalanced delimiters in string: " + str);
        }

        results.push_back(str.substr(pos + start_delim.size(),
                                     end - (pos + start_delim.size())));

        last_consumed = end + end_delim.size();
        start_search = last_consumed;
    }

    // any stray end delimiter after the last consumed block -> unbalanced
    if (str.find(end_delim, last_consumed) != std::string::npos) {
        throw std::runtime_error("❌  Error: unbalanced delimiters in string: " + str);
    }

    return results;
}

bool is_gtfs_file_char (char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.' || c == '-';
};


template<typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& vec) {
    os << "[";
    for (size_t i = 0; i < vec.size(); ++i) {
        os << vec[i];
        if (i < vec.size() - 1) {
            os << ", ";
        }
    }
    os << "]";
    return os;
}

template<typename K, typename V>
std::ostream& operator<<(std::ostream& os, const std::map<K,V>& map) {
    os << "{";
    for (auto it = map.begin(); it != map.end(); ++it) {
        os << it->first << ": " << it->second;
        if (std::next(it) != map.end()) {
            os << ", ";
        }
    }
    os << "}";
    return os;
}

template<typename K, typename V>
std::ostream& operator<<(std::ostream& os, const std::unordered_map<K,V>& map) {
    os << "{";
    for (auto it = map.begin(); it != map.end(); ++it) {
        os << it->first << ": " << it->second;
        if (std::next(it) != map.end()) {
            os << ", ";
        }
    }
    os << "}";
    return os;
} 

} // namespace