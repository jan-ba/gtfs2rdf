// SPDX-FileCopyrightText: 2026 Jan Babin
// SPDX-License-Identifier: AGPL-3.0-only
//
// Part of gtfs2rdf. See the LICENSE file for details.

module;

#include "transform_macros.h"

#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

export module schema:stops;
import :core;
import rdf_components;
import field_transforms;
import t_lib;
import runtime;

using namespace rdf;

namespace schema {

// this gtfs->rdf schema is preliminary and only covers a subset of all possible fields
export Schema buildStopsSchema(runtime::RuntimeContainer& rtc) {
	// prints out enum string for location_type codes
	// ARGS[0]: location_type code
	TRANSFORM2ONE(loc2Enum, ARGS, OUT_VAL, STORAGE) {
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

		static constexpr std::array<const char*, 5> LOCATION_TYPE_STRINGS = {
		    "Stop/platform", "Station", "Entrance/Exit", "Generic node", "Boarding area"};
		OUT_VAL = LOCATION_TYPE_STRINGS[type_id - '0'];
	}
	TRANSFORM_END

	// Possible columns in stops.txt
	const std::vector<std::string> POSSIBLE_COLUMNS = {"stop_id",
	                                                   "stop_code",
	                                                   "stop_name",
	                                                   "stop_desc",
	                                                   "stop_lat",
	                                                   "stop_lon",
	                                                   "zone_id",
	                                                   "stop_url",
	                                                   "location_type",
	                                                   "parent_station",
	                                                   "stop_timezone",
	                                                   "wheelchair_boarding",
	                                                   "level_id",
	                                                   "platform_code"};

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
	    {SUBJ, {"gtfs", "stopName"}, {"{stop_name}"}},
	    {SUBJ, {"gtfs", "stopDesc"}, {"{stop_desc}"}},

	    {SUBJ, {"gtfs", "stopCode"}, {"{stop_code}"}},
	    {SUBJ, {"gtfs", "stopUrl"}, {"{stop_url}", IRI("xsd", "anyURI")}},

	    // Geometry (WGS84 + GeoSPARQL WKT)
	    {SUBJ, {"wgs", "lat"}, {"{stop_lat, \"-90\", \"90\" | isInRange}", IRI("xsd", "decimal")}},
	    {SUBJ,
	     {"wgs", "long"},
	     {"{stop_lon, \"-180\", \"180\" | isInRange}", IRI("xsd", "decimal")}},
	    {SUBJ, {"geo", "hasGeometry"}, {IRI("gtfs2rdfgeom", "stop_{stop_id}")}},

	    // no need to check lat/long for validity again, since triples above will throw else
	    {{IRI("gtfs2rdfgeom", "stop_{stop_id}")},
	     {"geo", "asWKT"},
	     {"POINT({stop_lon} {stop_lat})", IRI("geo", "wktLiteral")}},

	    // Hierarchy / location type
	    {SUBJ, {"gtfs", "locationType"}, {"{location_type}", IRI("xsd", "integer")}},
	    {SUBJ, {"gtfs", "locationTypeEnum"}, {"{location_type | loc2Enum}"}},
	    {SUBJ, {"gtfs", "parentStation"}, {IRI("stops", "{parent_station}")}},

	    // Misc
	    {SUBJ, {"gtfs", "zoneId"}, {"{zone_id}"}},
	    {SUBJ, {"gtfs", "stopTimezone"}, {"{stop_timezone}"}},
	    {SUBJ, {"gtfs", "wheelchairBoarding"}, {"{wheelchair_boarding}", IRI("xsd", "integer")}},
	    {SUBJ, {"gtfs", "levelId"}, {"{level_id}"}},
	    {SUBJ, {"gtfs", "platformCode"}, {"{platform_code}"}},
	};

	Schema sch("stops.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, rtc);

	return sch;
}

} // namespace schema