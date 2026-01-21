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
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <map>

export module util:misc;

export namespace util {

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

template<typename T>
std::ostream& operator<<(std::ostream& os, const std::unordered_set<T>& set) {
    os << "{";
    for (auto it = set.begin(); it != set.end(); ++it) {
        os << *it;
        if (std::next(it) != set.end()) {
            os << ", ";
        }
    }
    os << "}";
    return os;
}

} // namespace