#include "../src/util/diagnostics.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

import storage;
import util;

namespace {

storage::PersistentStorageSqlite makeTestStorage(diagnostics::WarningCollector& wcol) {
	return storage::PersistentStorageSqlite(wcol, diagnostics::VerbosityLevelStats::QUIET, 20);
}

} // namespace

TEST_CASE("PersistentStorageSqlite: stores and retrieves variables correctly") {
	diagnostics::WarningCollector wcol(diagnostics::VerbosityLevelWarnings::QUIET);
	auto storage = makeTestStorage(wcol);

	// emptiness check
	REQUIRE(storage.getVariable("feed_info.txt", "agency") == "");
	REQUIRE(storage.getVariable("feed_info.txt", "some_date") == "");

	// store
	storage.storeVariable("feed_info.txt", "agency", "VAG");
	storage.storeVariable("feed_info.txt", "some_date", "20241203");

	// retrieve
	REQUIRE(storage.getVariable("feed_info.txt", "agency") == "VAG");
	REQUIRE(storage.getVariable("feed_info.txt", "some_date") == "20241203");

	// retrieve non-existing variables (should return empty string)
	REQUIRE(storage.getVariable("feed_info.txt", "nonexistent") == "");
	REQUIRE(storage.getVariable("nonexistent_ctx", "var1") == "");
}

TEST_CASE("PersistentStorageSqlite: stores and retrieves multimap values correctly") {
	diagnostics::WarningCollector wcol(diagnostics::VerbosityLevelWarnings::QUIET);
	auto storage = makeTestStorage(wcol);

	// emptiness check
	REQUIRE_FALSE(storage.containsValue("calendar_dates.txt", "disabled_dates", "1", "20241224"));
	REQUIRE_FALSE(storage.containsValue("calendar_dates.txt", "disabled_dates", "1", "20241225"));
	REQUIRE_FALSE(storage.containsValue("calendar_dates.txt", "disabled_dates", "2", "20241224"));

	// store values
	storage.storeValue("calendar_dates.txt", "disabled_dates", "1", "20241224");
	storage.storeValue("calendar_dates.txt", "disabled_dates", "1", "20241225");
	storage.storeValue("calendar_dates.txt", "disabled_dates", "1", "20241224"); // dup
	storage.storeValue(
	    "calendar_dates.txt", "disabled_dates", "1", "20221224"); // before other dates
	storage.storeValue("calendar_dates.txt", "disabled_dates", "2", "20241224");

	// contains
	REQUIRE(storage.containsValue("calendar_dates.txt", "disabled_dates", "1", "20241224"));
	REQUIRE(storage.containsValue("calendar_dates.txt", "disabled_dates", "1", "20241225"));
	REQUIRE(storage.containsValue("calendar_dates.txt", "disabled_dates", "1", "20221224"));
	REQUIRE(storage.containsValue("calendar_dates.txt", "disabled_dates", "2", "20241224"));
	REQUIRE_FALSE(storage.containsValue("calendar_dates.txt", "disabled_dates", "2", "20241225"));

	// results should be be deduplicated and ordered lexicographically
	auto values_1 = storage.getValues("calendar_dates.txt", "disabled_dates", "1");
	REQUIRE(values_1.size() == 3);
	REQUIRE(values_1[0] == "20221224");
	REQUIRE(values_1[1] == "20241224");
	REQUIRE(values_1[2] == "20241225");
	auto values_2 = storage.getValues("calendar_dates.txt", "disabled_dates", "2");
	REQUIRE(values_2.size() == 1);
	REQUIRE(values_2[0] == "20241224");
}

TEST_CASE("PersistentStorageSqlite: stores and retrieves tuplemap values correctly") {
	diagnostics::WarningCollector wcol(diagnostics::VerbosityLevelWarnings::QUIET);
	auto storage = makeTestStorage(wcol);

	// store tuples (shape_id -> (shape_pt_sequence, shape_pt_lat, shape_pt_lon))
	storage.storeTuple("shapes.txt", "shapes", "1", {"5", "52.5220", "13.4070"});
	storage.storeTuple("shapes.txt", "shapes", "1", {"20", "52.5205", "13.4055"});
	storage.storeTuple("shapes.txt", "shapes", "1", {"30", "52.5210", "13.4060"});
	storage.storeTuple("shapes.txt", "shapes", "2", {"10", "48.8566", "2.3522"});
	storage.storeTuple("shapes.txt", "shapes", "1", {"10", "52.5220", "13.4070"});

	// contains
	REQUIRE(storage.containsTuple("shapes.txt", "shapes", "1", {"5", "52.5220", "13.4070"}));
	REQUIRE(storage.containsTuple("shapes.txt", "shapes", "1", {"20", "52.5205", "13.4055"}));
	REQUIRE(storage.containsTuple("shapes.txt", "shapes", "1", {"30", "52.5210", "13.4060"}));
	REQUIRE(storage.containsTuple("shapes.txt", "shapes", "2", {"10", "48.8566", "2.3522"}));
	REQUIRE_FALSE(storage.containsTuple("shapes.txt", "shapes", "1", {"4", "52.5220", "13.4070"}));

	// get tuples (ordered by (val1, val2, ...) -> careful: lexicographical order, not numerical)
	auto tuples_1 = storage.getTuples("shapes.txt", "shapes", "1");
	REQUIRE(tuples_1.size() == 4);
	REQUIRE(tuples_1[0] ==
	        std::vector<std::string>{"10", "52.5220", "13.4070"}); // "10" < "5" lexicographically
	REQUIRE(tuples_1[1] == std::vector<std::string>{"20", "52.5205", "13.4055"});
	REQUIRE(tuples_1[2] == std::vector<std::string>{"30", "52.5210", "13.4060"});
	REQUIRE(tuples_1[3] == std::vector<std::string>{"5", "52.5220", "13.4070"});
	auto tuples_2 = storage.getTuples("shapes.txt", "shapes", "2");
	REQUIRE(tuples_2.size() == 1);
	REQUIRE(tuples_2[0] == std::vector<std::string>{"10", "48.8566", "2.3522"});
}

TEST_CASE("PersistentStorageSqlite: missing key parts throw errors") {
	diagnostics::WarningCollector wcol(diagnostics::VerbosityLevelWarnings::QUIET);
	auto storage = makeTestStorage(wcol);

	// storeVariable with empty variable name
	REQUIRE_THROWS_AS(storage.storeVariable("feed_info.txt", "", "some_value"), diagnostics::Error);

	// storeValue with empty key
	REQUIRE_THROWS_AS(storage.storeValue("calendar_dates.txt", "disabled_dates", "", "20241224"),
	                  diagnostics::Error);

	// storeTuple with missing key parts
	REQUIRE_THROWS_AS(storage.storeTuple("shapes.txt", "shapes", "", {"5", "52.5220", "13.4070"}),
	                  diagnostics::Error);
}

TEST_CASE("PersistentStorageSqlite: tuplemap arity mismatch throws errors") {
	diagnostics::WarningCollector wcol(diagnostics::VerbosityLevelWarnings::QUIET);
	auto storage = makeTestStorage(wcol);

	// store a tuple with arity 3
	storage.storeTuple("shapes.txt", "shapes", "1", {"5", "52.5220", "13.4070"});

	// store a tuple with arity 2 for the same table
	REQUIRE_THROWS_AS(storage.storeTuple("shapes.txt", "shapes", "1", {"5", "52.5220"}),
	                  diagnostics::Error);

	// store a tuple with arity 4 for the same table
	REQUIRE_THROWS_AS(
	    storage.storeTuple("shapes.txt", "shapes", "1", {"5", "52.5220", "13.4070", "extra"}),
	    diagnostics::Error);
}

TEST_CASE("PersistentStorageSqlite: clearContext removes only the given context") {
	diagnostics::WarningCollector wcol(diagnostics::VerbosityLevelWarnings::QUIET);
	auto storage = makeTestStorage(wcol);

	// store variables and tuples in two contexts
	storage.storeVariable("feed_info.txt", "agency", "VAG");
	storage.storeVariable("feed_info.txt", "some_date", "20241203");
	storage.storeValue("feed_info.txt", "some_multimap", "key1", "value1");
	storage.storeTuple("feed_info.txt", "some_tuplemap", "key1", {"val1", "val2"});
	storage.storeVariable("calendar_dates.txt", "disabled_dates", "20241224");
	storage.storeTuple("shapes.txt", "shapes", "1", {"5", "52.5220", "13.4070"});

	// clear one context
	storage.clearContext("feed_info.txt");

	// variables and tuples in cleared context should be gone
	REQUIRE(storage.getVariable("feed_info.txt", "agency") == "");
	REQUIRE(storage.getVariable("feed_info.txt", "some_date") == "");
	REQUIRE(storage.getValues("feed_info.txt", "some_multimap", "key1").empty());
	REQUIRE(!storage.containsTuple("feed_info.txt", "some_tuplemap", "key1", {"val1", "val2"}));

	// variables and tuples in other context should still be there
	REQUIRE(storage.getVariable("calendar_dates.txt", "disabled_dates") == "20241224");
	REQUIRE(storage.containsTuple("shapes.txt", "shapes", "1", {"5", "52.5220", "13.4070"}));
}