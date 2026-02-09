module;

#include <algorithm>
#include <array>
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
struct StringHash {
	using is_transparent =
	    void; // NOLINT(readability-identifier-naming): required for heterogeneous lookup

	size_t operator()(std::string_view svw) const noexcept {
		return std::hash<std::string_view>{}(svw);
	}
	size_t operator()(const std::string& str) const noexcept {
		return (*this)(std::string_view{str});
	}
	// size_t operator()(const char* str) const noexcept {
	//   return (*this)(std::string_view{str}); // assumes null-terminated
	// }
};

// parses a date string in 'YYYYMMDD' format into a sys_days object
// GIGO: no validation is performed
// NOLINTBEGIN : function is compact and very clear in its naming and purpose
std::chrono::sys_days parseYYYYMMDD(std::string_view svw) {
	const char* p = svw.data();

	int y = (p[0] - '0') * 1000 + (p[1] - '0') * 100 + (p[2] - '0') * 10 + (p[3] - '0');
	unsigned char m = (p[4] - '0') * 10 + (p[5] - '0');
	unsigned char d = (p[6] - '0') * 10 + (p[7] - '0');

	return std::chrono::sys_days{std::chrono::year{y} / std::chrono::month{m} /
	                             std::chrono::day{d}};
}
// NOLINTEND

// replace all occurences of 'before' with 'after' in 'svw'
std::string replaceAll(std::string_view svw, std::string_view before, std::string_view after) {
	std::string out;
	size_t start = 0;
	while (true) {
		size_t pos = svw.find(before, start);
		if (pos == std::string_view::npos) {
			out.append(svw.substr(start));
			break;
		}
		out.append(svw.substr(start, pos - start));
		out.append(after);
		start = pos + before.size();
	}
	return out;
}

// splits a string by a given delimiter character
std::vector<std::string_view> split(std::string_view svw, char delimiter) {
	std::vector<std::string_view> tokens;
	size_t start = 0;

	for (size_t i = 0; i < svw.size();
	     ++i) { // NOLINT(readability-identifier-length): very clear usage
		if (svw[i] == delimiter) {
			tokens.push_back(svw.substr(start, i - start));
			start = i + 1;
		}
	}

	// add the last token (even if it's empty)
	tokens.push_back(svw.substr(start));

	return tokens;
}

// removes whitespace outside of quoted substrings
std::string removeWSOutsideQuotes(std::string_view svw) {
	std::string out;
	out.reserve(svw.size());
	bool in_q = false;
	for (size_t i = 0; i < svw.size(); ++i) {
		char c = svw[i]; // NOLINT(readability-identifier-length): very clear usage
		if (c == '"' && (i == 0 || svw[i - 1] != '\\')) {
			in_q = !in_q;
		}
		if (!in_q && std::isspace((unsigned char)c)) {
			continue;
		}
		out.push_back(c);
	}
	return out;
}

// finds the position of a delimiter character at the top level (not inside quotes or parentheses)
size_t findAtTopLevel(std::string_view svw, char delimiter, size_t from = 0) {
	bool in_q = false;
	int paren = 0;
	for (size_t i = from; i < svw.size(); ++i) {
		char c = svw[i]; // NOLINT(readability-identifier-length): very clear usage
		if (c == '"' && (i == 0 || svw[i - 1] != '\\')) {
			in_q = !in_q;
		}
		if (in_q) {
			continue;
		}
		if (c == '(') {
			++paren;
		} else if (c == ')') {
			--paren;
		} else if (paren == 0 && c == delimiter) {
			return i;
		}
	}
	return std::string_view::npos;
}

// splits a string by a given delimiter character only at the top level
std::vector<std::string_view> splitAtTopLevel(std::string_view svw, char delimiter) {
	std::vector<std::string_view> out;
	size_t start = 0;
	while (start <= svw.size()) {
		size_t pos = findAtTopLevel(svw, delimiter, start);
		if (pos == std::string_view::npos) {
			auto sub_svw = svw.substr(start);
			if (!sub_svw.empty()) {
				out.push_back(sub_svw);
			}
			break;
		}
		auto sub_svw = svw.substr(start, pos - start);
		if (!sub_svw.empty()) {
			out.push_back(sub_svw);
		}
		start = pos + 1;
	}
	return out;
}

// splits a string once at the top level by a given delimiter character into a pair
std::pair<std::string_view, std::string_view> splitOnceAtTopLevel(std::string_view svw,
                                                                  char delimiter) {
	size_t pos = findAtTopLevel(svw, delimiter, 0);
	if (pos == std::string_view::npos) {
		return {svw, std::string_view{}};
	}
	return {svw.substr(0, pos), svw.substr(pos + 1)};
}

// splits a string once at a given index into a pair at pos such that
// the element at the given index is not included in either part
std::pair<std::string_view, std::string_view> splitOnceAtIndex(std::string_view svw, size_t pos) {
	if (pos >= svw.size()) {
		return {svw, std::string_view{}};
	}
	return {svw.substr(0, pos), svw.substr(pos + 1)};
}

// removes surrounding quotes and unescapes minimal escape sequences, meaning \" and \\ will
// be unescaped
std::string unquote(std::string_view svw) {
	if (svw.size() >= 2 && svw.front() == '"' && svw.back() == '"') {
		std::string out;
		out.reserve(svw.size() - 2);
		for (size_t i = 1; i + 1 < svw.size(); ++i) {
			char c = svw[i]; // NOLINT(readability-identifier-length): very clear usage
			if (c == '\\' && i + 1 < svw.size() - 1) {
				char n = svw[i + 1]; // NOLINT(readability-identifier-length): very clear usage
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
	return std::string(svw);
}

// encloses a string in char1 from left and char2 from right
std::string enclose(std::string_view svw, char char1, char char2) {
	std::string out;
	out.reserve(2 + svw.size());
	out.push_back(char1);
	out.append(svw);
	out.push_back(char2);
	return out;
}

// splits a string at the first occurrence of a delimiter character
std::pair<std::string_view, std::string_view> splitAt(std::string_view svw, char delimiter) {
	size_t pos = svw.find(delimiter);
	if (pos == std::string_view::npos) {
		return {svw, {}};
	}
	return {svw.substr(0, pos), svw.substr(pos + 1)};
}

enum class UnitType : u_int8_t { COUNT, SIZE, TIME };

// formats a large integer value into a human-readable string with units
// - counts: "", k, M, B, T, P, E (base 1000)
// - sizes : B, kB, MB, GB, TB, PB, EB (base 1000)
// - time  : ns, µs, ms, s, min, h, d
std::string formatValueWithPaddedUnits(uint64_t val, UnitType unit_type) {
	struct Unit {
		const char* abb;
		long double lim;
	};

	constexpr int MAX_UNIT_I = 6; // last valid index (E / EB)

	static constexpr std::array<Unit, MAX_UNIT_I + 1> COUNT_UNITS = {{
	    {"", 1.0L},
	    {"k", 1e3L},
	    {"M", 1e6L},
	    {"B", 1e9L},
	    {"T", 1e12L},
	    {"P", 1e15L},
	    {"E", 1e18L},
	}};

	static constexpr std::array<Unit, MAX_UNIT_I + 1> SIZE_UNITS = {{
	    {"B", 1.0L},
	    {"kB", 1e3L},
	    {"MB", 1e6L},
	    {"GB", 1e9L},
	    {"TB", 1e12L},
	    {"PB", 1e15L},
	    {"EB", 1e18L},
	}};

	static constexpr std::array<Unit, MAX_UNIT_I + 1> TIME_UNITS = {{
	    {"ns", 1.0L},
	    {"µs", 1e3L},
	    {"ms", 1e6L},
	    {"s", 1e9L},
	    {"min", 60e9L},
	    {"h", 3600e9L},
	    {"d", 86400e9L},
	}};

	const std::array<Unit, MAX_UNIT_I + 1>* units;
	switch (unit_type) {
		case UnitType::COUNT:
			units = &COUNT_UNITS;
			break;
		case UnitType::SIZE:
			units = &SIZE_UNITS;
			break;
		case UnitType::TIME:
			units = &TIME_UNITS;
			break;
	}

	// pick largest unit where val >= lim
	int unit_i = 0;
	for (int u_i = MAX_UNIT_I; u_i >= 1; --u_i) {
		if ((long double)val >= (*units)[u_i].lim) {
			unit_i = u_i;
			break;
		}
	}

	long double scaled = (long double)val / ((*units)[unit_i].lim);
	long double rounded = std::round(scaled * 10.0L) / 10.0L; // NOLINT(readability-magic-numbers)

	// if rounding pushed it to 1000.0, bump unit (e.g. 999.95k -> 1.0M)
	if (unit_i < MAX_UNIT_I && rounded >= 1000.0L) { // NOLINT(readability-magic-numbers)
		unit_i += 1;
		scaled = (long double)val / ((*units)[unit_i].lim);
		rounded = std::round(scaled * 10.0L) / 10.0L; // NOLINT(readability-magic-numbers)
	}

	std::ostringstream oss;
	if (unit_i == 0) {
		// plain integer for < 1000 (counts) or < 1000 B (sizes)
		oss << val << (*units)[unit_i].abb; // note: counts adds "", sizes adds "B"
	} else {
		oss << std::fixed << std::setprecision(1) << (double)rounded << (*units)[unit_i].abb;
	}

	return oss.str();
}

bool isValidGtfsChar(char c) { // NOLINT(readability-identifier-length)
	return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
};

bool isValidCTXName(std::string_view ctx) {
	// splits by '.' and checks that last part is "txt" and all chars are valid
	auto [name_part, ext_part] = splitAt(ctx, '.');
	if (ext_part != "txt") {
		return false;
	}
	return std::ranges::all_of(name_part, isValidGtfsChar);
}

} // namespace util::strings