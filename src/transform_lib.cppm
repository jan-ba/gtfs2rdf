module;

#include "util/diagnostics.h"

#include <cctype>
#include <charconv>
#include <iostream> // remove later
#include <stdexcept>
#include <string>

export module t_lib;
import field_transforms;

using namespace field_transforms;

// TODO: camelCase for function names? Because this will contrast nicely with Gtfs field names
//       which are usually snake_case

// library of field transforms that could be useful for multiple schemas
// functions will need to be registered in the TransformRegistry (see below) in order to be
// available
namespace t_lib {

// _____________________________________________________________________________________________
// factory for range-checking transform
// Use like this in a schema file:
// registry.registerTransform("in_range_0_100", t_lib::in_range(0.0, 100.0));

// _____________________________________________________________________________________________
// Functions for type correctness checks
// if no value is given no output will be generated (empty string)
// if the value is invalid, a custom error 'Error' will be thrown

// checks if the input is a valid xs:integer and returns it unchanged if valid, otherwise throws
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

// checks if the input is a valid xs:decimal and returns it unchanged if valid, otherwise throws
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

// TODO: add more type checks as needed, e.g. for dates, times, datetimes, etc.

// _________________________________________________________________________________________________
// Functions for format conversions

// convert Gtfs date "YYYYMMDD" to xs:date "YYYY-MM-DD"
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

// convert Gtfs time "H+:MM:SS" to xs:time "HH:MM:SS" by wrapping hours mod 24
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

// register functions in the TransformRegistry to make them available for use in schema files
export void registerLibTransforms(TransformRegistry& registry) {
	registry.registerTransform("isValidInt", isValidInt);
	registry.registerTransform("isValidDecimal", isValidDecimal);
	registry.registerTransform("isUnsigned", isUnsigned);
	registry.registerTransform("isInRange", isInRange);
	registry.registerTransform("convertDate2xs_unchecked", convertDate2xs_unchecked);
	registry.registerTransform("convertTime2xs_unchecked", convertTime2xs_unchecked);
}
} // namespace t_lib