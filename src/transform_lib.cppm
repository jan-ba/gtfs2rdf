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

// TODO: camelCase for function names? Because this will contrast nicely with GTFS field names
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

export void is_int(Args args, Out1 &out) {
	std::string_view sv = args[0];
	if (sv.empty())
		return;

	int value = 0;
	const char *first = sv.data();
	const char *last = first + sv.size();

	auto [ptr, ec] = std::from_chars(first, last, value);

	if (ec == std::errc{} && ptr == last) {
		out = sv;
		return;
	} else {
		throw diagnostics::Error("Transform error in 'is_int': expected integer value, got '" +
		                         std::string(sv) + "'");
	}
}

// TODO: rework
export void is_decimal(Args args, Out1 &out) {
	std::string_view sv = args[0];
	auto fail = [&]() {
		throw diagnostics::Error("Transform error in 'is_decimal': expected decimal value, got '" +
		                         std::string(sv) + "'");
	};

	if (sv.empty()) {
		fail();
	}

	std::size_t i = 0;

	// Optional sign
	if (sv[i] == '+' || sv[i] == '-') {
		++i;
		if (i == sv.size()) {
			// string was just "+" or "-" -> invalid
			fail();
		}
	}

	// zero or more digits before the decimal point
	while (i < sv.size() && (sv[i] >= '0' && sv[i] <= '9')) {
		++i;
	}

	// Must have a '.'
	if (i >= sv.size() || sv[i] != '.') {
		fail();
	}
	++i; // skip '.'

	// one or more digits after the decimal point
	std::size_t digits_after_dot = 0;
	while (i < sv.size() && (sv[i] >= '0' && sv[i] <= '9')) {
		++digits_after_dot;
		++i;
	}

	if (digits_after_dot == 0) {
		// must have at least one digit after '.'
		fail();
	}

	// no trailing junk allowed
	if (i != sv.size()) {
		fail();
	}

	out = sv;
}

// _____________________________________________________________________________________________
// convert GTFS date "YYYYMMDD" to xs:date "YYYY-MM-DD"
export void convert_date(Args args, Out1 &out) {
	std::string_view sv = args[0];
	if (sv.empty()) {
		return; // leave empty
	} else if (sv.size() != 8) {
		throw diagnostics::Error(
		    "Transform error in 'convert_date': expected date in format YYYYMMDD, got '" +
		    std::string(sv) + "'");
	}
	out.append(sv.substr(0, 4));
	out.append("-");
	out.append(sv.substr(4, 2));
	out.append("-");
	out.append(sv.substr(6, 2));
}

// convert GTFS time "H+:MM:SS" to xs:time "HH:MM:SS" by wrapping hours mod 24
// GIGO: no validation of minutes/seconds
export void convert_time(Args args, Out1 &out) {
	std::string_view sv = args[0];
	if (sv.empty())
		return; // no input, no output

	// find first ':'
	size_t p = sv.find(':');
	if (p == std::string::npos)
		return; // invalid format, return empty

	int h = 0;
	for (size_t i = 0; i < p; ++i) {
		h = h * 10 + (sv[i] - '0');
	}

	int hh = h % 24;

	// keep ":MM:SS" (or whatever follows) exactly as provided
	out.append(hh < 10 ? "0" : "");
	out.append(std::to_string(hh));
	out.append(sv.substr(p));
}

export void register_lib_transforms(TransformRegistry &registry) {
	registry.registerTransform("is_int", is_int);
	registry.registerTransform("is_decimal", is_decimal);
	registry.registerTransform("convert_date", convert_date);
	registry.registerTransform("convert_time", convert_time);
}
} // namespace t_lib