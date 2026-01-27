module;

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
// if the value is invalid, a runtime_error exception will be thrown  TODO: better exception type?

export void is_int(const Args &args, Out1 &out) {
	const std::string &s = args[0];
	if (s.empty())
		return;

	int value = 0;
	const char *first = s.data();
	const char *last = first + s.size();

	auto [ptr, ec] = std::from_chars(first, last, value);

	if (ec == std::errc{} && ptr == last) {
		out = s;
		return;
	} else {
		throw std::runtime_error("❌ Faulty data: expected integer value, got '" + s + "'");
	}
}

// TODO: rework
export void is_decimal(const Args &args, Out1 &out) {
	const std::string &s = args[0];
	auto fail = [&]() {
		throw std::runtime_error("❌ Faulty data: expected decimal value, got '" + s + "'");
	};

	if (s.empty()) {
		fail();
	}

	std::size_t i = 0;

	// Optional sign
	if (s[i] == '+' || s[i] == '-') {
		++i;
		if (i == s.size()) {
			// string was just "+" or "-" -> invalid
			fail();
		}
	}

	// Zero or more digits before the decimal point
	while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
		++i;
	}

	// Must have a '.'
	if (i >= s.size() || s[i] != '.') {
		fail();
	}
	++i; // skip '.'

	// one or more digits after the decimal point
	std::size_t digits_after_dot = 0;
	while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
		++digits_after_dot;
		++i;
	}

	if (digits_after_dot == 0) {
		// must have at least one digit after '.'
		fail();
	}

	// no trailing junk allowed
	if (i != s.size()) {
		fail();
	}

	out = s;
}

// _____________________________________________________________________________________________
// convert GTFS date "YYYYMMDD" to xs:date "YYYY-MM-DD"
export void convert_date(const Args &args, Out1 &out) {
	const std::string &s = args[0];
	if (s.empty()) {
		return; // leave empty
	} else if (s.size() != 8) {
		throw std::runtime_error("❌ Transform error: expected date in format YYYYMMDD, got '" + s +
		                         "'");
	}
	out.append(s.substr(0, 4));
	out.append("-");
	out.append(s.substr(4, 2));
	out.append("-");
	out.append(s.substr(6, 2));
}

// convert GTFS time "H+:MM:SS" to xs:time "HH:MM:SS" by wrapping hours mod 24
// GIGO: no validation of minutes/seconds
export void convert_time(const Args &args, Out1 &out) {
	const std::string &s = args[0];
	if (s.empty())
		return; // no input, no output

	// find first ':'
	size_t p = s.find(':');
	if (p == std::string::npos)
		return; // invalid format, return empty

	int h = 0;
	for (size_t i = 0; i < p; ++i) {
		h = h * 10 + (s[i] - '0');
	}

	int hh = h % 24;

	// keep ":MM:SS" (or whatever follows) exactly as provided
	out.append(hh < 10 ? "0" : "");
	out.append(std::to_string(hh));
	out.append(s.substr(p));
}

export void register_lib_transforms(TransformRegistry &registry) {
	registry.registerTransform("is_int", is_int);
	registry.registerTransform("is_decimal", is_decimal);
	registry.registerTransform("convert_date", convert_date);
	registry.registerTransform("convert_time", convert_time);
}
} // namespace t_lib