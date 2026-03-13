#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <string>
#include <string_view>
#include <vector>

import t_lib;
import field_transforms;

namespace {
std::string getTransResult(field_transforms::Transform2One transf,
                           std::initializer_list<std::string_view> args) {
	std::string out;
	std::span<const std::string_view> args_span(args);
	transf(args_span, out);
	return out;
}

std::vector<std::string> getTransResult(field_transforms::Transform2Many transf,
                                        std::initializer_list<std::string_view> args) {
	std::vector<std::string> out;
	std::span<const std::string_view> args_span(args);
	transf(args_span, out);
	return out;
}
} // namespace

TEST_CASE("t_lib::isValidInt accepts valid integers and rejects invalid ones") {
	SECTION("Valid") {
		REQUIRE(getTransResult(t_lib::isValidInt, {"42"}) == "42");
		REQUIRE(getTransResult(t_lib::isValidInt, {"-17"}) == "-17");
		REQUIRE(getTransResult(t_lib::isValidInt, {"0"}) == "0");
	}

	SECTION("Empty") {
		REQUIRE(getTransResult(t_lib::isValidInt, {""}) == "");
	}

	SECTION("Invalid format") {
		REQUIRE_THROWS(getTransResult(t_lib::isValidInt, {"-"}));
		REQUIRE_THROWS(getTransResult(t_lib::isValidInt, {"01"}));
		REQUIRE_THROWS(getTransResult(t_lib::isValidInt, {"-01"}));
		REQUIRE_THROWS(getTransResult(t_lib::isValidInt, {"1a"}));
		REQUIRE_THROWS(getTransResult(t_lib::isValidInt, {"1.0"}));
		REQUIRE_THROWS(getTransResult(t_lib::isValidInt, {".12"}));
	}

	SECTION("not even remotely a number") {
		REQUIRE_THROWS(getTransResult(t_lib::isValidInt, {"abc"}));
		REQUIRE_THROWS(getTransResult(t_lib::isValidInt, {"-abc"}));
	}

	SECTION("Arity") {
		REQUIRE_THROWS(getTransResult(t_lib::isValidInt, {}));
		REQUIRE_THROWS(getTransResult(t_lib::isValidInt, {"1", "2"}));
	}
}

TEST_CASE("t_lib::isValidDecimal accepts valid decimals and rejects invalid ones") {
	SECTION("Valid") {
		REQUIRE(getTransResult(t_lib::isValidDecimal, {"3.14"}) == "3.14");
		REQUIRE(getTransResult(t_lib::isValidDecimal, {"-0.001"}) == "-0.001");
		REQUIRE(getTransResult(t_lib::isValidDecimal, {"42"}) == "42");
		REQUIRE(getTransResult(t_lib::isValidDecimal, {"-17"}) == "-17");
		REQUIRE(getTransResult(t_lib::isValidDecimal, {"0"}) == "0");
		REQUIRE(getTransResult(t_lib::isValidDecimal, {".12"}) == ".12");
	}

	SECTION("Empty") {
		REQUIRE(getTransResult(t_lib::isValidDecimal, {""}) == "");
	}

	SECTION("Invalid format") {
		REQUIRE_THROWS(getTransResult(t_lib::isValidDecimal, {"-"}));
		REQUIRE_THROWS(getTransResult(t_lib::isValidDecimal, {"1."}));
		REQUIRE_THROWS(getTransResult(t_lib::isValidDecimal, {"1e10"})); // not turtle decimal
		REQUIRE_THROWS(getTransResult(t_lib::isValidDecimal, {"1a"}));
	}

	SECTION("not even remotely a number") {
		REQUIRE_THROWS(getTransResult(t_lib::isValidDecimal, {"abc"}));
		REQUIRE_THROWS(getTransResult(t_lib::isValidDecimal, {"-abc"}));
	}

	SECTION("Arity") {
		REQUIRE_THROWS(getTransResult(t_lib::isValidDecimal, {}));
		REQUIRE_THROWS(getTransResult(t_lib::isValidDecimal, {"1", "2"}));
	}
}

TEST_CASE("t_lib::isUnsigned accepts unsigned values and rejects negative ones") {
	// isUnsigned does not check whether input is a valid numerical value, to be chained
	// with isValidInt or isValidDecimal for that purpose

	SECTION("Valid") {
		REQUIRE(getTransResult(t_lib::isUnsigned, {"42"}) == "42");
		REQUIRE(getTransResult(t_lib::isUnsigned, {"0"}) == "0");
		REQUIRE(getTransResult(t_lib::isUnsigned, {"3.14"}) == "3.14");
		REQUIRE(getTransResult(t_lib::isUnsigned, {"abc"}) == "abc");
	}

	SECTION("Empty") {
		REQUIRE(getTransResult(t_lib::isUnsigned, {""}) == "");
	}

	SECTION("Negative values") {
		REQUIRE_THROWS(getTransResult(t_lib::isUnsigned, {"-17"}));
		REQUIRE_THROWS(getTransResult(t_lib::isUnsigned, {"-0.001"}));
		REQUIRE_THROWS(getTransResult(t_lib::isUnsigned, {"-abc"}));
	}

	SECTION("Arity") {
		REQUIRE_THROWS(getTransResult(t_lib::isUnsigned, {}));
		REQUIRE_THROWS(getTransResult(t_lib::isUnsigned, {"1", "2"}));
	}
}

TEST_CASE("t_lib::isInRange checks if a value is within a specified range") {
	// isInRange checks for correct input type (val, min, max)

	SECTION("Valid") {
		REQUIRE(getTransResult(t_lib::isInRange, {"5", "0", "10"}) == "5");
		REQUIRE(getTransResult(t_lib::isInRange, {"0", "0", "10"}) == "0");
		REQUIRE(getTransResult(t_lib::isInRange, {"10", "0", "10"}) == "10");
		REQUIRE(getTransResult(t_lib::isInRange, {"3.14", "0", "4"}) == "3.14");
		// negative values
		REQUIRE(getTransResult(t_lib::isInRange, {"-5", "-10", "0"}) == "-5");
		REQUIRE(getTransResult(t_lib::isInRange, {"20", "-10", "30"}) == "20");
	}

	SECTION("Empty") {
		REQUIRE(getTransResult(t_lib::isInRange, {"", "0", "10"}) == "");
		REQUIRE_THROWS(getTransResult(t_lib::isInRange, {"5", "", "10"}));
		REQUIRE_THROWS(getTransResult(t_lib::isInRange, {"5", "0", ""}));
	}

	SECTION("Out of range") {
		REQUIRE_THROWS(getTransResult(t_lib::isInRange, {"-1", "0", "10"}));
		REQUIRE_THROWS(getTransResult(t_lib::isInRange, {"11", "0", "10"}));
		REQUIRE_THROWS(getTransResult(t_lib::isInRange, {"4.5", "0", "4"}));
	}

	SECTION("Invalid format") {
		REQUIRE_THROWS(getTransResult(t_lib::isInRange, {"abc", "0", "10"}));
		REQUIRE_THROWS(getTransResult(t_lib::isInRange, {"5", "abc", "10"}));
		REQUIRE_THROWS(getTransResult(t_lib::isInRange, {"5", "0", "abc"}));
	}

	SECTION("Arity") {
		REQUIRE_THROWS(getTransResult(t_lib::isInRange, {}));
		REQUIRE_THROWS(getTransResult(t_lib::isInRange, {"5"}));
		REQUIRE_THROWS(getTransResult(t_lib::isInRange, {"5", "0"}));
		REQUIRE_THROWS(getTransResult(t_lib::isInRange, {"5", "0", "10", "extra"}));
	}
}

TEST_CASE("t_lib::convertDate2xs_unchecked converts GTFS date to xsd:date format") {
	// correctly formatted input date will result in a correctly formatted output date, else garbage
	SECTION("Valid") {
		REQUIRE(getTransResult(t_lib::convertDate2xs_unchecked, {"20240101"}) == "2024-01-01");
		REQUIRE(getTransResult(t_lib::convertDate2xs_unchecked, {"19991231"}) == "1999-12-31");
	}

	SECTION("Empty") {
		REQUIRE(getTransResult(t_lib::convertDate2xs_unchecked, {""}) == "");
	}

	SECTION("Invalid format") { // only length check performed
		REQUIRE_THROWS(getTransResult(t_lib::convertDate2xs_unchecked, {"2024-01-01"}));
		REQUIRE_THROWS(getTransResult(t_lib::convertDate2xs_unchecked, {"2024011"}));
		REQUIRE_THROWS(getTransResult(t_lib::convertDate2xs_unchecked, {"202401011"}));
	}

	SECTION("Arity") {
		REQUIRE_THROWS(getTransResult(t_lib::convertDate2xs_unchecked, {}));
		REQUIRE_THROWS(getTransResult(t_lib::convertDate2xs_unchecked, {"20240101", "extra"}));
	}
}

TEST_CASE("t_lib::convertTime2xs_unchecked converts GTFS time to xsd:time format") {
	SECTION("Valid") {
		REQUIRE(getTransResult(t_lib::convertTime2xs_unchecked, {"23:59:59"}) == "23:59:59");
		REQUIRE(getTransResult(t_lib::convertTime2xs_unchecked, {"00:00:00"}) == "00:00:00");
		REQUIRE(getTransResult(t_lib::convertTime2xs_unchecked, {"12:34:56"}) == "12:34:56");
		REQUIRE(getTransResult(t_lib::convertTime2xs_unchecked, {"5:24:00"}) == "05:24:00");
	}

	SECTION("Valid but with over carry-over (gtfs time may be e.g. 26:00:00)") {
		REQUIRE(getTransResult(t_lib::convertTime2xs_unchecked, {"24:00:00"}) == "00:00:00");
		REQUIRE(getTransResult(t_lib::convertTime2xs_unchecked, {"25:30:00"}) == "01:30:00");
		REQUIRE(getTransResult(t_lib::convertTime2xs_unchecked, {"48:00:30"}) == "00:00:30");
	}

	SECTION("Empty") {
		REQUIRE(getTransResult(t_lib::convertTime2xs_unchecked, {""}) == "");
	}

	SECTION("Invalid format") { // only length check performed
		REQUIRE_THROWS(getTransResult(t_lib::convertTime2xs_unchecked, {"23:59"}));
		REQUIRE_THROWS(getTransResult(t_lib::convertTime2xs_unchecked, {"abc"}));
	}

	SECTION("Arity") {
		REQUIRE_THROWS(getTransResult(t_lib::convertTime2xs_unchecked, {}));
		REQUIRE_THROWS(getTransResult(t_lib::convertTime2xs_unchecked, {"12:34:56", "extra"}));
	}
}