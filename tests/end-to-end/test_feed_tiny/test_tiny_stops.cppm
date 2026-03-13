// SPDX-FileCopyrightText: 2026 Jan Babin
// SPDX-License-Identifier: AGPL-3.0-only
//
// Part of gtfs2rdf. See the LICENSE file for details.

module;

#include "../../../src/schema/transform_macros.h"

#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

export module test_tiny_stops;
import schema;
import rdf_components;
import field_transforms;
import t_lib;
import runtime;
import util;

using namespace rdf;
using namespace schema;

namespace test {

// _________________________________________________________________________________________________
// THIS SCHEMA WAS SOLELY BUILT FOR TESTING PURPOSES IN THE TINY FEED
// _________________________________________________________________________________________________

export Schema buildTestTinyStopsSchema(runtime::RuntimeContainer& rtc) {
	// prints out enum string for location_type codes
	// ARGS[0]: location_type code
	TRANSFORM2ONE(test_loc2Enum, ARGS, OUT_VAL, STORAGE) {
		if (ARGS[0].empty()) {
			return; // leave empty, triple will not be printed
		}
		if (ARGS[0].size() != 1) {
			TRANSFORM_ERROR("Unknown location_type code: " + std::string(ARGS[0]));
		}
		char type_id = ARGS[0][0];
		if (type_id < '0' || type_id > '4') {
			TRANSFORM_ERROR("Unknown location_type code: " + std::string(ARGS[0]));
		}
		static constexpr std::array<std::string_view, 5> kNames = {
		    "stop", "station", "entrance_exit", "generic_node", "boarding_area"};
		OUT_VAL = kNames[type_id - '0'];
	}
	TRANSFORM_END

	TRANSFORM2MANY(test_splitTags, ARGS, OUT_VALS, STORAGE) {
		if (ARGS[0].empty()) {
			return; // leave empty, triple will not be printed
		}
		auto vals = util::strings::split(ARGS[0], '|');
		for (const auto& val : vals) {
			OUT_VALS.emplace_back(val);
		}
	}
	TRANSFORM_END

	// ARGS[0]: stop_lon, ARGS[1]: stop_lat, ARGS[2]: stop_id
	TRANSFORM2ONE(dedupStopGeom, ARGS, OUT_VAL, STORAGE) {
		if (ARGS.size() != 3) {
			TRANSFORM_ERROR("dedupStopGeom requires 3 arguments, got " +
			                std::to_string(ARGS.size()));
		}
		auto geom_id =
		    STORAGE.getValues("test_tiny_stops.txt", "point_to_geomid", {ARGS[0], ARGS[1]});
		if (!geom_id.empty()) {
			// already seen, leave empty to avoid duplicate triple
			return;
		}
		STORAGE.storeValue("test_tiny_stops.txt", "point_to_geomid", {ARGS[0], ARGS[1]}, ARGS[2]);
		OUT_VAL.append("POINT(");
		OUT_VAL.append(ARGS[0]); // stop_lon
		OUT_VAL.append(" ");
		OUT_VAL.append(ARGS[1]); // stop_lat
		OUT_VAL.append(")");
	}
	TRANSFORM_END

	// ARGS[0]: stop_lon, ARGS[1]: stop_lat
	TRANSFORM2ONE(getGeomID, ARGS, OUT_VAL, STORAGE) {
		if (ARGS.size() != 2) {
			TRANSFORM_ERROR("getGeomID requires 2 arguments, got " + std::to_string(ARGS.size()));
		}
		auto geom_id =
		    STORAGE.getValues("test_tiny_stops.txt", "point_to_geomid", {ARGS[0], ARGS[1]});
		if (geom_id.empty()) {
			TRANSFORM_ERROR("Geometry ID should have been stored by dedupStopGeom transform, but "
			                "was not found for coordinates: " +
			                std::string(ARGS[0]) + ", " + std::string(ARGS[1]));
		}
		OUT_VAL = geom_id[0]; // return existing id
	}
	TRANSFORM_END

	// Possible columns in stops.txt
	const std::vector<std::string> POSSIBLE_COLUMNS = {
	    "stop_id",
	    "stop_name",
	    "stop_desc",
	    "stop_lat",
	    "stop_lon",
	    "stop_url",
	    "location_type",
	    "parent_station",
	    "platform_code",
	    "wheelchair_boarding",
	    "zone_id",
	    "stop_timezone",
	    "test_tags"}; // not an actual GTFS column, only for testing Transform2Many

	const std::unordered_map<std::string, std::string> PREFIXES = {
	    {"stops", "https://gtfs.org/stops/"},
	    {"stop_times", "https://gtfs.org/stop_times/"},
	    {"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
	    {"xsd", "http://www.w3.org/2001/XMLSchema#"},
	    {"gtfs", "https://w3id.org/gtfs2rdf#"},
	    {"wgs", "http://www.w3.org/2003/01/geo/wgs84_pos#"},
	    {"geo", "http://www.opengis.net/ont/geosparql#"},
	    {"gtfs2rdfgeom", "https://w3id.org/gtfs2rdf/geometry#"}};

	const IRI SUBJ = IRI("stops", "{stop_id}");

	// Ontology (?)
	const std::vector<Triple> TRIPLES = {
	    // SUBJECT                   PREDICATE          OBJECT

	    // Type
	    {SUBJ, {"rdf", "type"}, {IRI("gtfs", "Stop")}},
	    {SUBJ, {"gtfs", "stopName"}, {"{stop_name}", "{FEED_LANG@test_tiny_feed_info.txt}"}},
	    {SUBJ, {"gtfs", "stopDesc"}, {"{stop_desc}", "{FEED_LANG@test_tiny_feed_info.txt}"}},
	    {SUBJ, {"wgs", "lat"}, {"{stop_lat, \"-90\", \"90\" | isInRange}", IRI("xsd", "decimal")}},
	    {SUBJ,
	     {"wgs", "long"},
	     {"{stop_lon, \"-180\", \"180\" | isInRange}", IRI("xsd", "decimal")}},
	    {{"gtfs2rdfgeom", "stop_{stop_id}"},
	     {"geo", "asWKT"},
	     {"{stop_lon, stop_lat, stop_id | dedupStopGeom}", IRI("geo", "wktLiteral")}},
	    {SUBJ,
	     {"geo", "hasGeometry"},
	     {IRI("gtfs2rdfgeom", "stop_{stop_lon, stop_lat | getGeomID}")}},
	    {SUBJ, {"gtfs", "stopUrl"}, {"{stop_url}", IRI("xsd", "anyURI")}},
	    {SUBJ, {"gtfs", "locationTypeEnum"}, {"{location_type | test_loc2Enum}"}},
	    {SUBJ, {"gtfs", "parent_station"}, {IRI("stops", "{parent_station}")}},
	    {SUBJ, {"gtfs", "platformCode"}, {"{platform_code}"}},
	    {SUBJ, {"gtfs", "wheelchairBoarding"}, {"{wheelchair_boarding}", IRI("xsd", "integer")}},
	    {SUBJ,
	     {"gtfs", "zoneId"},
	     {"{zone_id}"}}, // this one should not be printed, zone_id not in stops.txt of tiny feed
	    {SUBJ, {"gtfs", "stopTimezone"}, {"{stop_timezone}"}},
	    {SUBJ, {"gtfs", "tag"}, {"{test_tags | test_splitTags}"}}};

	Schema sch("test_tiny_stops.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, rtc);

	return sch;
}

} // namespace test