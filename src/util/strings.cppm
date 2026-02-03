module;

#include "diagnostics.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module util:strings;

export namespace util::strings {

// custom hash for string_view and string to be used in unordered_map
// taken from https://www.cppstories.com/2021/heterogeneous-access-cpp20/
struct string_hash {
	using is_transparent = void;

	size_t operator()(std::string_view sv) const noexcept {
		return std::hash<std::string_view>{}(sv);
	}
	size_t operator()(const std::string &s) const noexcept {
		return (*this)(std::string_view{s});
	}
	// size_t operator()(const char* s) const noexcept {
	//   return (*this)(std::string_view{s}); // assumes null-terminated
	// }
};

// parses a date string in 'YYYYMMDD' format into a sys_days object
std::chrono::sys_days parseYYYYMMDD(std::string_view sv) {
	if (sv.size() != 8) {
		throw diagnostics::Error("Transform error in 'parseYYYYMMDD': invalid date string format, "
		                         "expected 'YYYYMMDD', got '" +
		                         std::string(sv) + "'");
	}
	for (char c : sv) {
		if (c < '0' || c > '9') {
			throw diagnostics::Error("Transform error in 'parseYYYYMMDD': invalid date string "
			                         "format, expected 'YYYYMMDD', got '" +
			                         std::string(sv) + "'");
		}
	}

	const char *p = sv.data();

	int y = (p[0] - '0') * 1000 + (p[1] - '0') * 100 + (p[2] - '0') * 10 + (p[3] - '0');
	unsigned char m = (p[4] - '0') * 10 + (p[5] - '0');
	unsigned char d = (p[6] - '0') * 10 + (p[7] - '0');

	return std::chrono::sys_days{std::chrono::year{y} / std::chrono::month{m} /
	                             std::chrono::day{d}};
}

// splits a string by a given delimiter character
std::vector<std::string> split(std::string_view sv, const char delimiter) {
	std::vector<std::string> tokens;
	std::string current;

	for (char c : sv) {
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
void remove_ws(std::string &str) {
	str.erase(
	    std::remove_if(str.begin(), str.end(), [](unsigned char c) { return std::isspace(c); }),
	    str.end());
}

// removes whitespace outside of quoted substrings
std::string remove_ws_outside_quotes(std::string_view sv) {
	std::string out;
	out.reserve(sv.size());
	bool in_q = false;
	for (size_t i = 0; i < sv.size(); ++i) {
		char c = sv[i];
		if (c == '"' && (i == 0 || sv[i - 1] != '\\'))
			in_q = !in_q;
		if (!in_q && std::isspace((unsigned char)c))
			continue;
		out.push_back(c);
	}
	return out;
}

// finds the position of a delimiter character at the top level (not inside quotes or parentheses)
size_t find_top_level(std::string_view sv, char delimiter, size_t from = 0) {
	bool in_q = false;
	int paren = 0;
	for (size_t i = from; i < sv.size(); ++i) {
		char c = sv[i];
		if (c == '"' && (i == 0 || sv[i - 1] != '\\'))
			in_q = !in_q;
		if (in_q)
			continue;
		if (c == '(')
			++paren;
		else if (c == ')')
			--paren;
		else if (paren == 0 && c == delimiter)
			return i;
	}
	return std::string_view::npos;
}

// splits a string by a given delimiter character only at the top level
std::vector<std::string_view> split_top_level(std::string_view sv, char delimiter) {
	std::vector<std::string_view> out;
	size_t start = 0;
	while (start <= sv.size()) {
		size_t pos = find_top_level(sv, delimiter, start);
		if (pos == std::string_view::npos) {
			auto tok = sv.substr(start);
			if (!tok.empty())
				out.push_back(tok);
			break;
		}
		auto tok = sv.substr(start, pos - start);
		if (!tok.empty())
			out.push_back(tok);
		start = pos + 1;
	}
	return out;
}

// splits a string once at the top level by a given delimiter character into a pair
std::pair<std::string_view, std::string_view> split_once_top_level(std::string_view sv,
                                                                   char delimiter) {
	size_t pos = find_top_level(sv, delimiter, 0);
	if (pos == std::string_view::npos)
		return {sv, std::string_view{}};
	return {sv.substr(0, pos), sv.substr(pos + 1)};
}

// removes surrounding quotes and unescapes minimal escape sequences, meaning \" and \\ will
// be unescaped
std::string unquote(std::string_view tok) {
	if (tok.size() >= 2 && tok.front() == '"' && tok.back() == '"') {
		std::string out;
		out.reserve(tok.size() - 2);
		for (size_t i = 1; i + 1 < tok.size(); ++i) {
			char c = tok[i];
			if (c == '\\' && i + 1 < tok.size() - 1) {
				char n = tok[i + 1];
				// minimal escapes
				if (n == '"' || n == '\\') {
					out.push_back(n);
					++i;
					continue;
				}
			}
			out.push_back(c);
		}
		return out;
	}
	return std::string(tok);
}

// encloses a string in char1 from left and char2 from right
std::string enclose(std::string_view sv, char char1, char char2) {
	std::string out;
	out.reserve(2 + sv.size());
	out.push_back(char1);
	out.append(sv);
	out.push_back(char2);
	return out;
}

enum class UnitType { Counts, Sizes, Time };

// formats a large integer value into a human-readable string with units
// - counts: "", k, M, B, T, P, E (base 1000)
// - sizes : B, kB, MB, GB, TB, PB, EB (base 1000)
// - time  : ns, µs, ms, s, min, h, d
std::string fmt_suffix_padded(uint64_t v, UnitType unit_type) {
	struct Unit {
		const char *s;
		long double div;
	};

	static constexpr Unit counts_units[] = {
	    {"", 1.0L},
	    {"k", 1e3L},
	    {"M", 1e6L},
	    {"B", 1e9L},
	    {"T", 1e12L},
	    {"P", 1e15L},
	    {"E", 1e18L},
	};

	static constexpr Unit size_units[] = {
	    {"B", 1.0L},
	    {"kB", 1e3L},
	    {"MB", 1e6L},
	    {"GB", 1e9L},
	    {"TB", 1e12L},
	    {"PB", 1e15L},
	    {"EB", 1e18L},
	};

	static constexpr Unit time_units[] = {
	    {"ns", 1.0L},
	    {"µs", 1e3L},
	    {"ms", 1e6L},
	    {"s", 1e9L},
	    {"min", 60e9L},
	    {"h", 3600e9L},
	    {"d", 86400e9L},
	};

	constexpr int max_i = 6; // last valid index (E / EB)
	const Unit *units = nullptr;
	switch (unit_type) {
	case UnitType::Counts:
		units = counts_units;
		break;
	case UnitType::Sizes:
		units = size_units;
		break;
	case UnitType::Time:
		units = time_units;
		break;
	}

	// pick largest unit where v >= div
	int ui = 0;
	for (int i = max_i; i >= 1; --i) {
		if ((long double)v >= units[i].div) {
			ui = i;
			break;
		}
	}

	long double scaled = (long double)v / units[ui].div;
	long double rounded = std::round(scaled * 10.0L) / 10.0L; // 1 decimal

	// if rounding pushed it to 1000.0, bump unit (e.g. 999.95k -> 1.0M)
	if (ui < max_i && rounded >= 1000.0L) {
		ui += 1;
		scaled = (long double)v / units[ui].div;
		rounded = std::round(scaled * 10.0L) / 10.0L;
	}

	std::ostringstream oss;
	if (ui == 0) {
		// plain integer for < 1000 (counts) or < 1000 B (sizes)
		oss << v << units[ui].s; // note: counts adds "", sizes adds "B"
	} else {
		oss << std::fixed << std::setprecision(1) << (double)rounded << units[ui].s;
	}

	return oss.str();
}

// splits a string at the first occurrence of a delimiter character
std::pair<std::string_view, std::string_view> split_at(std::string_view sv, char delimiter) {
	size_t pos = sv.find(delimiter);
	if (pos == std::string_view::npos)
		return {sv, {}};
	return {sv.substr(0, pos), sv.substr(pos + 1)};
}

bool is_gtfs_file_char(char c) {
	return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
};

bool valid_ctx_name(std::string_view ctx) {
	// splits by '.' and checks that last part is "txt" and all chars are valid
	auto [name_part, ext_part] = split_at(ctx, '.');
	if (ext_part != "txt")
		return false;
	for (char c : name_part) {
		if (!is_gtfs_file_char(c))
			return false;
	}
	return true;
}

} // namespace util::strings