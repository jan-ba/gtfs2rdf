// SPDX-FileCopyrightText: 2026 Jan Babin
// SPDX-License-Identifier: AGPL-3.0-only
//
// Part of gtfs2rdf. See the LICENSE file for details.

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
typename MapType::mapped_type& getOrInsert(MapType& map, std::string_view key) {
	auto itr = map.find(key);
	if (itr == map.end()) {
		auto res = map.emplace(std::string(key), typename MapType::mapped_type{});
		return res.first->second;
	}
	return itr->second;
}

// RAII timer that accumulates elapsed nanoseconds into passed acc
class ScopedTimerNS {
  public:
	explicit ScopedTimerNS(uint64_t& acc)
	    : acc_(&acc)
	    , t_start_(std::chrono::steady_clock::now()) {
	}

	explicit ScopedTimerNS(uint64_t* acc)
	    : acc_(acc)
	    , t_start_(std::chrono::steady_clock::now()) {
	}

	~ScopedTimerNS() {
		*acc_ += (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
		             std::chrono::steady_clock::now() - t_start_)
		             .count();
	}

  private:
	uint64_t* acc_;
	std::chrono::steady_clock::time_point t_start_;
};

// _________________________________________________________________________________________________
// overloaded stream and string operators for STL containers (for convenient pretty printing)

template <typename T> std::ostream& operator<<(std::ostream& out, const std::vector<T>& vec) {
	out << "[";
	for (size_t i = 0; i < vec.size(); ++i) {
		out << vec[i];
		if (i < vec.size() - 1) {
			out << ", ";
		}
	}
	out << "]";
	return out;
}

template <typename T> std::string operator+(const std::string& str, const std::vector<T>& vec) {
	std::ostringstream oss;
	oss << vec;
	return str + oss.str();
}

template <typename K, typename V>
std::ostream& operator<<(std::ostream& out, const std::map<K, V>& map) {
	out << "{";
	for (auto itr = map.begin(); itr != map.end(); ++itr) {
		out << itr->first << ": " << itr->second;
		if (std::next(itr) != map.end()) {
			out << ", ";
		}
	}
	out << "}";
	return out;
}

template <typename K, typename V, typename Hash, typename Equal>
std::ostream& operator<<(std::ostream& out, const std::unordered_map<K, V, Hash, Equal>& map) {
	out << "{";
	for (auto itr = map.begin(); itr != map.end(); ++itr) {
		out << itr->first << ": " << itr->second;
		if (std::next(itr) != map.end()) {
			out << ", ";
		}
	}
	out << "}";
	return out;
}

template <typename K, typename V>
std::ostream& operator<<(std::ostream& out, const std::unordered_map<K, V>& map) {
	out << "{";
	for (auto itr = map.begin(); itr != map.end(); ++itr) {
		out << itr->first << ": " << itr->second;
		if (std::next(itr) != map.end()) {
			out << ", ";
		}
	}
	out << "}";
	return out;
}

template <typename T>
std::ostream& operator<<(std::ostream& out, const std::unordered_set<T>& set) {
	out << "{";
	for (auto itr = set.begin(); itr != set.end(); ++itr) {
		out << *itr;
		if (std::next(itr) != set.end()) {
			out << ", ";
		}
	}
	out << "}";
	return out;
}

} // namespace util::misc