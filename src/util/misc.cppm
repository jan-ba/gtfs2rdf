// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the GTFS2RDF project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.

module;
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

export module util:misc;

export namespace util::misc {

template <class MapType>
typename MapType::mapped_type &get_or_insert(MapType &map, std::string_view key) {
	auto it = map.find(key);
	if (it != map.end()) {
		return it->second;
	} else {
		auto res = map.emplace(std::string(key), typename MapType::mapped_type{});
		return res.first->second;
	}
}

// RAII timer that accumulates elapsed nanoseconds into passed acc
struct ScopedTimerNS {
	uint64_t &acc;
	std::chrono::steady_clock::time_point t0;

	explicit ScopedTimerNS(uint64_t &a)
	    : acc(a)
	    , t0(std::chrono::steady_clock::now()) {}

	~ScopedTimerNS() {
		acc += (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
		           std::chrono::steady_clock::now() - t0)
		           .count();
	}
};

// _________________________________________________________________________________________________
// overloaded stream operators for STL containers (for convenient pretty printing)

template <typename T> std::ostream &operator<<(std::ostream &os, const std::vector<T> &vec) {
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

template <typename K, typename V>
std::ostream &operator<<(std::ostream &os, const std::map<K, V> &map) {
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

template <typename K, typename V, typename Hash, typename Equal>
std::ostream &operator<<(std::ostream &os, const std::unordered_map<K, V, Hash, Equal> &map) {
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

template <typename K, typename V>
std::ostream &operator<<(std::ostream &os, const std::unordered_map<K, V> &map) {
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

template <typename T> std::ostream &operator<<(std::ostream &os, const std::unordered_set<T> &set) {
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

} // namespace util::misc