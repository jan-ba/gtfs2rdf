#include "../../src/schema/transform_macros.h"

#include <algorithm>
#include <catch2/catch_template_test_macros.hpp>
#include <filesystem>
#include <iostream>

import gtfs2rdf_runner;
import runtime;

// import schemas for tiny feed
import test_tiny_feed_info;
import test_tiny_stops;

// import schemas for full feed
import test_full_agency;
import test_full_calendar_dates;
import test_full_calendar;
import test_full_feed_info;
import test_full_routes;
import test_full_shapes;
import test_full_stop_times;
import test_full_stops;
import test_full_trips;

namespace {
// helper function to get settings for the end-to-end test
// notice the very small buffers to force carryover between chunks and multiple flushes even on
// small feeds
runtime::Settings getSettingsForTest(const std::filesystem::path& base, bool ntriples = false) {
	return runtime::Settings(ntriples,                                // ntriples output
	                         false,                                   // spec dump
	                         0.1,                                     // read buffer size MB
	                         0.1,                                     // write buffer size MB
	                         20.0,                                    // storage buffer size MB
	                         true,                                    // overwrite output
	                         false,                                   // pre-run
	                         base / "feed.zip",                       // input path
	                         false,                                   // output to stdout
	                         base / (ntriples ? "got.nt" : "got.ttl") // output path
	);
}

std::vector<std::string> readLinesFromFile(const std::filesystem::path& path) {
	std::vector<std::string> lines;
	std::ifstream file(path);
	if (!file.is_open()) {
		throw std::runtime_error("Could not open file: " + path.string());
	}
	std::string line;
	while (std::getline(file, line)) {
		lines.push_back(line);
	}
	return lines;
}
} // namespace

// This tests the output against a curated-by-hand expected output, which itself was validated to be
// correct RDF. The feed is designed to test various branches, such as missing fields, Transform2One
// and Transform2Many (making use of persistent storage functions), various data types, proper
// escaping, etc. but is still small enough to be easily manageable and understandable by a human
TEST_CASE("End-to-end test on tiny feed (ttl output)") {
	gtfs2rdf_runner::Factories factories = {
	    {"test_tiny_feed_info.txt", test::buildTestTinyFeedInfoSchema},
	    {"test_tiny_stops.txt", test::buildTestTinyStopsSchema}};
	auto base = std::filesystem::path(GTFS2RDF_E2E_DIR) / "test_feed_tiny";
	auto settings = getSettingsForTest(base);
	auto wcol = diagnostics::WarningCollector(settings.getWarningsVerbosity());
	int result = gtfs2rdf_runner::gtfs2rdf(settings, wcol, factories);
	REQUIRE(result == 0); // successful run
	auto got_lines = readLinesFromFile(base / "got.ttl");
	auto expected_lines = readLinesFromFile(base / "expected.ttl");

	// sort lines to make order irrelevant
	std::sort(got_lines.begin(), got_lines.end());
	std::sort(expected_lines.begin(), expected_lines.end());
	for (size_t i = 0; i < expected_lines.size(); ++i) {
		REQUIRE(got_lines[i] == expected_lines[i]);
	}

	// remove the output file to keep the test directory clean (comment out for debugging)
	std::filesystem::remove(base / "got.ttl");
}

TEST_CASE("End-to-end test on tiny feed (nt output)") {
	gtfs2rdf_runner::Factories factories = {
	    {"test_tiny_feed_info.txt", test::buildTestTinyFeedInfoSchema},
	    {"test_tiny_stops.txt", test::buildTestTinyStopsSchema}};
	auto base = std::filesystem::path(GTFS2RDF_E2E_DIR) / "test_feed_tiny";
	auto settings = getSettingsForTest(base, true);
	auto wcol = diagnostics::WarningCollector(settings.getWarningsVerbosity());
	int result = gtfs2rdf_runner::gtfs2rdf(settings, wcol, factories);
	REQUIRE(result == 0); // successful run
	auto got_lines = readLinesFromFile(base / "got.nt");
	auto expected_lines = readLinesFromFile(base / "expected.nt");

	// sort lines to make order irrelevant
	std::sort(got_lines.begin(), got_lines.end());
	std::sort(expected_lines.begin(), expected_lines.end());
	for (size_t i = 0; i < expected_lines.size(); ++i) {
		REQUIRE(got_lines[i] == expected_lines[i]);
	}

	// remove the output file to keep the test directory clean (comment out for debugging)
	std::filesystem::remove(base / "got.nt");
}

// To avoid heavy computations and large data files for testing, this is count-based. The input feed
// has no missing columns and we only allow fully predictable triple generation (i.e. no
// Transform2Many or geometry deduplication for example, both were tested with the tiny feed), such
// that we can easily calculate the expected number of triples by hand and compare it to the actual
// output triple count (hence, ntriples format used). Still, this test will cover the entire
// processing pipeline on a real feed, including shapes.txt, therefore testing the integration of
// all components and the handling of more complex geometries. Test feed source: SMTT (Societatea
// Metropolitană de Transport Timișoara) GTFS — publisher “SMTT”, https://smtt.ro (contact:
// relatii.publice@smtt.ro).

/* Row counts (excluding header row) for input files and calculation of expected triple count:

   -- file row counts --         -- triple per row --       -- expected triples --
   agency.txt:1                     8                           8
   calendar_dates.txt:9937          2                           19874
   calendar.txt:151                 10                          1510
   feed_info.txt:1                  0 (storage only)            0
   routes.txt:58                    8                           464
   shapes.txt:42149                 0 (storage only)            0
   stop_times.txt:186392            7                           1304744
   stops.txt:870                    9                           7830
   trips.txt:12649                  9                           113841
   -------------------------------------------------------------------------------
                                        total expected triples: 1448271
*/
TEST_CASE("End-to-end test on real feed") {
	gtfs2rdf_runner::Factories factories = {
	    {"test_full_agency.txt", test::buildTestFullAgencySchema},
	    {"test_full_calendar_dates.txt", test::buildTestFullCalendarDatesSchema},
	    {"test_full_calendar.txt", test::buildTestFullCalendarSchema},
	    {"test_full_feed_info.txt", test::buildTestFullFeedInfoSchema},
	    {"test_full_routes.txt", test::buildTestFullRoutesSchema},
	    {"test_full_shapes.txt", test::buildTestFullShapesSchema},
	    {"test_full_stop_times.txt", test::buildTestFullStopTimesSchema},
	    {"test_full_stops.txt", test::buildTestFullStopsSchema},
	    {"test_full_trips.txt", test::buildTestFullTripsSchema}};
	auto base = std::filesystem::path(GTFS2RDF_E2E_DIR) / "test_feed_full";
	auto settings = getSettingsForTest(base, true); // enabling ntriples output
	auto wcol = diagnostics::WarningCollector(settings.getWarningsVerbosity());
	int result = gtfs2rdf_runner::gtfs2rdf(settings, wcol, factories);
	REQUIRE(result == 0); // successful run
	auto got_lines = readLinesFromFile(base / "got.nt");
	REQUIRE(got_lines.size() == 1448271);

	// remove output file
	std::filesystem::remove(base / "got.nt");
}
