// SPDX-FileCopyrightText: 2026 Jan Babin
// SPDX-License-Identifier: AGPL-3.0-only
//
// Part of gtfs2rdf. See the LICENSE file for details.

module;

#include "util/diagnostics.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <stdexcept>
#include <string>

export module t_lib;
import field_transforms;

using namespace field_transforms;

// library of field transforms that could be useful for multiple schemas
// IMPORTANT: if you add a new transform here, make sure to also register it at the end of the file!
namespace t_lib {

// [TODO]: for each function in this library, add its counterpart that can be used inside a
// user transform (e.g. bool isValidInt(std::string_view sv)) <future work>

// _____________________________________________________________________________________________
// Functions for type correctness checks
// if no value is given no output will be generated (empty string)
// if the value is invalid, a custom error 'Error' will be thrown

// checks if the input is a valid xsd:integer and returns it unchanged if valid, otherwise throws
// valid integers consist of an optional leading '-' followed by one or more digits, and must not
// have leading zeros (except for the number '0' itself)
export void isValidInt(Args args, Out1& out) {
	if (args.size() != 1) {
		throw diagnostics::Error(
		    "Transform error in 'isValidInt': expected exactly 1 argument, got " +
		    std::to_string(args.size()));
	}
	std::string_view svw = args[0];
	if (svw.empty())
		return;

	bool negative = svw[0] == '-';
	size_t start = negative ? 1 : 0;
	if (start == svw.size()) {
		throw diagnostics::Error("Transform error in 'isValidInt': expected integer value, got '" +
		                         std::string(svw) + "'");
	}
	if (svw[start] == '0' && svw.size() > start + 1) {
		throw diagnostics::Error("Transform error in 'isValidInt': expected integer value, got '" +
		                         std::string(svw) + "'");
	}
	for (size_t i = start; i < svw.size(); ++i) { // NOLINT(readability-identifier-length)
		if (!std::isdigit(static_cast<unsigned char>(svw[i]))) {
			throw diagnostics::Error(
			    "Transform error in 'isValidInt': expected integer value, got '" +
			    std::string(svw) + "'");
		}
	}
	out = svw;
}

// checks if the input is a valid xsd:decimal and returns it unchanged if valid, otherwise throws
// valid decimals consist of an optional leading '-' followed by digits, with at most one decimal
// point, and must not have leading zeros (except for the number '0' itself or '0.x'). Decimal point
// must be followed by at least one digit if present.
export void isValidDecimal(Args args, Out1& out) {
	if (args.size() != 1) {
		throw diagnostics::Error(
		    "Transform error in 'isValidDecimal': expected exactly 1 argument, got " +
		    std::to_string(args.size()));
	}
	std::string_view svw = args[0];
	if (svw.empty()) {
		return; // leave empty
	}

	if (svw.back() == '.') {
		throw diagnostics::Error(
		    "Transform error in 'isValidDecimal': expected decimal value, got '" +
		    std::string(svw) + "'");
	}

	bool negative = svw[0] == '-';
	size_t start = negative ? 1 : 0;
	if (start == svw.size()) {
		throw diagnostics::Error(
		    "Transform error in 'isValidDecimal': expected decimal value, got '" +
		    std::string(svw) + "'");
	}
	if (svw[start] == '0' && svw.size() > start + 1 && svw[start + 1] != '.') {
		throw diagnostics::Error(
		    "Transform error in 'isValidDecimal': expected decimal value, got '" +
		    std::string(svw) + "'");
	}
	bool decimal_point_seen = false;
	bool digit_seen = false;

	for (size_t i = start; i < svw.size(); ++i) { // NOLINT(readability-identifier-length)
		char c = svw[i];
		if (c == '.') {
			if (decimal_point_seen) {
				throw diagnostics::Error(
				    "Transform error in 'isValidDecimal': expected decimal value, got '" +
				    std::string(svw) + "'");
			}
			decimal_point_seen = true;
		} else if (std::isdigit(static_cast<unsigned char>(c))) {
			digit_seen = true;
		} else {
			throw diagnostics::Error(
			    "Transform error in 'isValidDecimal': expected decimal value, got '" +
			    std::string(svw) + "'");
		}
	}

	if (decimal_point_seen && !digit_seen) {
		throw diagnostics::Error(
		    "Transform error in 'isValidDecimal': expected decimal value, got '" +
		    std::string(svw) + "'");
	}
	out = svw;
}

// checks whether a numerical value is unsigned
export void isUnsigned(Args args, Out1& out) {
	if (args.size() != 1) {
		throw diagnostics::Error(
		    "Transform error in 'isUnsigned': expected exactly 1 argument, got " +
		    std::to_string(args.size()));
	}
	std::string_view svw = args[0];
	if (svw.empty()) {
		return; // leave empty
	}
	if (svw[0] == '-') {
		throw diagnostics::Error("Transform error in 'isUnsigned': expected unsigned value, got '" +
		                         std::string(svw) + "'");
	}
	out = svw;
}

// converts an input string to a boolean value ("true" or "false")
// evaluates to false if string matches any entry FALSE_STRINGS (case-insensitive),
// otherwise evaluates to true
export void toBool(Args args, Out1& out) {
	static constexpr std::string FALSE_STRINGS[] = {
	    "0", "false", "no", "off", "none", "null", "nan"};
	if (args.size() != 1) {
		throw diagnostics::Error("Transform error in 'toBool': expected exactly 1 argument, got " +
		                         std::to_string(args.size()));
	}
	std::string_view svw = args[0];
	if (svw.empty()) {
		return; // leave empty
	}
	std::string lower(svw.size(), '\0');
	std::transform(
	    svw.begin(), svw.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
	for (const auto& false_string : FALSE_STRINGS) {
		if (lower == false_string) {
			out = "false";
			return;
		}
	}
	out = "true";
}

// check whether a numerical value falls within a specified range [min, max]
// ARGS: args[0] = value to check, args[1] = min, args[2] = max
export void isInRange(Args args, Out1& out) {
	if (args.size() != 3) {
		throw diagnostics::Error(
		    "Transform error in 'isInRange': expected exactly 3 arguments (value, min, max), got " +
		    std::to_string(args.size()));
	}
	if (args[0].empty()) {
		return; // leave empty
	}
	double d_val, d_min, d_max;
	auto [ptr_val, ec_val] =
	    std::from_chars(args[0].data(), args[0].data() + args[0].size(), d_val);
	auto [ptr_min, ec_min] =
	    std::from_chars(args[1].data(), args[1].data() + args[1].size(), d_min);
	auto [ptr_max, ec_max] =
	    std::from_chars(args[2].data(), args[2].data() + args[2].size(), d_max);
	if (ec_val != std::errc()) {
		throw diagnostics::Error("Transform error in 'isInRange': expected numeric value, got '" +
		                         std::string(args[0]) + "'");
	}
	if (ec_min != std::errc()) {
		throw diagnostics::Error(
		    "Transform error in 'isInRange': expected numeric min value, got '" +
		    std::string(args[1]) + "'");
	}
	if (ec_max != std::errc()) {
		throw diagnostics::Error(
		    "Transform error in 'isInRange': expected numeric max value, got '" +
		    std::string(args[2]) + "'");
	}
	if (d_val < d_min || d_val > d_max) {
		throw diagnostics::Error("Transform error in 'isInRange': value " + std::to_string(d_val) +
		                         " out of range [" + std::to_string(d_min) + ", " +
		                         std::to_string(d_max) + "]");
	}
	out = args[0];
}

// [TODO]: add more type checks as needed, e.g. for dates, times, datetimes, etc. <future work>

// _________________________________________________________________________________________________
// Functions for format conversions

// convert Gtfs date "YYYYMMDD" to xsd:date "YYYY-MM-DD"
export void convertDate2xs_unchecked(Args args, Out1& out) {
	if (args.size() != 1) {
		throw diagnostics::Error(
		    "Transform error in 'convertDate2xs_unchecked': expected exactly 1 argument, got " +
		    std::to_string(args.size()));
	}
	std::string_view svw = args[0];
	if (svw.empty()) {
		return; // leave empty
	}
	if (svw.size() != 8) {
		throw diagnostics::Error("Transform error in 'convertDate2xs_unchecked': expected date in "
		                         "format YYYYMMDD, got '" +
		                         std::string(svw) + "'");
	}
	// NOLINTBEGIN : substr calls with constant indices to parse fixed-format input
	out.append(svw.substr(0, 4));
	out.append("-");
	out.append(svw.substr(4, 2));
	out.append("-");
	out.append(svw.substr(6, 2));
	// NOLINTEND
}

// convert Gtfs time "H+:MM:SS" to xsd:time "HH:MM:SS" by wrapping hours mod 24
// GIGO: no validation of minutes/seconds
// NOLINTBEGIN : magic numbers and variable names in this function are clear in this context
export void convertTime2xs_unchecked(Args args, Out1& out) {
	if (args.size() != 1) {
		throw diagnostics::Error(
		    "Transform error in 'convertTime2xs_unchecked': expected exactly 1 argument, got " +
		    std::to_string(args.size()));
	}
	std::string_view svw = args[0];
	if (svw.empty()) {
		return; // no input, no output
	}
	if (svw.size() < 7 || svw.size() > 8) {
		throw diagnostics::Error("Transform error in 'convertTime2xs_unchecked': expected time in "
		                         "format H+:MM:SS, got '" +
		                         std::string(svw) + "'");
	}
	// find first ':'
	size_t pos_colon = svw.find(':');
	if (pos_colon == std::string::npos) {
		return; // invalid format, return empty
	}

	int h = 0;
	for (size_t i = 0; i < pos_colon; ++i) {
		h = h * 10 + (svw[i] - '0');
	}

	int hh = h % 24;

	// keep ":MM:SS" (or whatever follows) exactly as provided
	out.append(hh < 10 ? "0" : ""); // pad single-digit hours with leading zero
	out.append(std::to_string(hh));
	out.append(svw.substr(pos_colon));
}
// NOLINTEND

// convert a GTFS time string "H+:MM:SS" to total seconds after midnight as an integer
export void gtfsTimeToSeconds(Args args, Out1& out) {
	if (args.size() != 1) {
		throw diagnostics::Error(
		    "Transform error in 'gtfsTimeToSeconds': expected exactly 1 argument, got " +
		    std::to_string(args.size()));
	}
	std::string_view svw = args[0];
	if (svw.empty()) {
		return; // no input, no output
	}
	if (svw.size() < 7 || svw.size() > 8) {
		throw diagnostics::Error("Transform error in 'gtfsTimeToSeconds': expected time in "
		                         "format H+:MM:SS, got '" +
		                         std::string(svw) + "'");
	}
	// find first ':'
	size_t pos_colon = svw.find(':');
	if (pos_colon == std::string::npos) {
		return; // invalid format, return empty
	}

	int h = 0;
	for (size_t i = 0; i < pos_colon; ++i) {
		h = h * 10 + (svw[i] - '0');
	}

	int m = 0;
	size_t i = pos_colon + 1;
	for (; i < svw.size() && svw[i] != ':'; ++i) {
		m = m * 10 + (svw[i] - '0');
	}

	int s = 0;
	if (i < svw.size() && svw[i] == ':') {
		++i;
		for (; i < svw.size(); ++i) {
			s = s * 10 + (svw[i] - '0');
		}
	}

	int total_seconds = h * 3600 + m * 60 + s;
	out = std::to_string(total_seconds);
}

// materialises an enum value as a string
// ARG: args[0] = enum integer, args[1...n] = possible enum strings
// invariant: enum integer must be in range [0, 9] and less than number of provided enum strings
export void enumToString(Args args, Out1& out) {
	if (args.size() < 2) {
		throw diagnostics::Error(
		    "Transform error in 'enumToString': expected at least 2 arguments (value, possible "
		    "enum values), got " +
		    std::to_string(args.size()));
	}
	std::string_view value = args[0];
	if (value.empty()) {
		return; // leave empty
	}
	size_t enum_int = args[0][0] - '0'; // convert first character to integer
	if (enum_int >= args.size() - 1) {
		throw diagnostics::Error("Transform error in 'enumToString': got enum integer " +
		                         std::to_string(enum_int) + ", expected in range [0, " +
		                         std::to_string(args.size() - 2) + "]");
	}
	if (enum_int < 0 || enum_int > 9) {
		throw diagnostics::Error(
		    "Transform error in 'enumToString': expected enum integer in range [0, 9], got " +
		    std::to_string(enum_int));
	}
	out = args[enum_int + 1];
}

// same as enumToString but if input is empty, index specified by args[1] points to the default
// enum string to be used
// ARG: args[0] = enum integer, args[1] = default enum index, args[2...n] = possible enum strings
export void enumToStringWithDefault(Args args, Out1& out) {
	if (args.size() < 3) {
		throw diagnostics::Error(
		    "Transform error in 'enumToStringWithDefault': expected at least 3 arguments (value, default index, possible "
		    "enum values), got " +
		    std::to_string(args.size()));
	}
	std::string_view value = args[0];
	std::string_view default_index_str = args[1];
	if (default_index_str.empty()) {
		throw diagnostics::Error(
		    "Transform error in 'enumToStringWithDefault': default index argument cannot be empty");
	}
	size_t default_index = default_index_str[0] - '0'; // convert first character to integer
	if (default_index >= args.size() - 2) {
		throw diagnostics::Error(
		    "Transform error in 'enumToStringWithDefault': got default index " +
		    std::to_string(default_index) + ", expected in range [0, " +
		    std::to_string(args.size() - 3) + "]");
	}
	if (value.empty()) {
		out = args[default_index + 2]; // use default enum value
		return;
	}
	size_t enum_int = args[0][0] - '0'; // convert first character to integer
	if (enum_int >= args.size() - 2) {
		throw diagnostics::Error("Transform error in 'enumToStringWithDefault': got enum integer " +
		                         std::to_string(enum_int) + ", expected in range [0, " +
		                         std::to_string(args.size() - 3) + "]");
	}
	if (enum_int < 0 || enum_int > 9) {
		throw diagnostics::Error(
		    "Transform error in 'enumToStringWithDefault': expected enum integer in range [0, 9], got " +
		    std::to_string(enum_int));
	}
	out = args[enum_int + 2];
}


// register functions in the TransformRegistry to make them available for use in schema files
export void registerLibTransforms(TransformRegistry& registry) {
	registry.registerTransform("isValidInt", isValidInt);
	registry.registerTransform("isValidDecimal", isValidDecimal);
	registry.registerTransform("isUnsigned", isUnsigned);
	registry.registerTransform("isInRange", isInRange);
	registry.registerTransform("convertDate2xs_unchecked", convertDate2xs_unchecked);
	registry.registerTransform("convertTime2xs_unchecked", convertTime2xs_unchecked);
	registry.registerTransform("toBool", toBool);
	registry.registerTransform("enumToString", enumToString);
	registry.registerTransform("enumToStringWithDefault", enumToStringWithDefault);
	registry.registerTransform("gtfsTimeToSeconds", gtfsTimeToSeconds);
}

} // namespace t_lib