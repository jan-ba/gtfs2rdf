// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the gtfs2rdf project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.

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
		switch (type_id) {
		case '0':
			OUT_VAL = "stop";
			break;
		case '1':
			OUT_VAL = "station";
			break;
		case '2':
			OUT_VAL = "entrance_exit";
			break;
		case '3':
			OUT_VAL = "generic_node";
			break;
		case '4':
			OUT_VAL = "boarding_area";
			break;
		}
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
	    {"xs", "http://www.w3.org/2001/XMLSchema#"},
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

	    {SUBJ, {"gtfs", "stopName"}, {"{stop_name}", "{FEED_LANG@feed_info.txt}"}},

	    // Using translation transform to get translated stop names
	    // expected args of get_translation: table_name, field_name, field_value
	    // TODO: if  transform not found, ignore instruction (i.e., no translation available)
	    // { SUBJ,  {"gtfs", "stopName"},    { "{\"stops\", \"stop_name\", stop_name"
	    //                                         "| get_translation@translations.txt}",
	    //                                     "{latest_translation_lookup@translations.txt}" } },

	    {SUBJ, {"gtfs", "stopDesc"}, {"{stop_desc}", "{FEED_LANG@feed_info.txt}"}},
	    {SUBJ, {"gtfs", "stopCode"}, {"{stop_code}"}},
	    {SUBJ, {"gtfs", "stopUrl"}, {"{stop_url}", IRI("xs", "anyURI")}},

	    // Geometry (WGS84 + GeoSPARQL WKT)
	    {SUBJ, {"wgs", "lat"}, {"{stop_lat, \"-90\", \"90\" | isInRange}", IRI("xs", "decimal")}},
	    {SUBJ,
	     {"wgs", "long"},
	     {"{stop_lon, \"-180\", \"180\" | isInRange}", IRI("xs", "decimal")}},
	    {SUBJ, {"geo", "hasGeometry"}, {IRI("gtfs2rdfgeom", "stop_{stop_id}")}},
	    {SUBJ, {"geo", "asWKT"}, {"POINT({stop_lon} {stop_lat})", IRI("geo", "wktLiteral")}},

	    // Hierarchy / location type
	    {SUBJ, {"gtfs", "locationType"}, {"{location_type}", IRI("xs", "integer")}},
	    {SUBJ, {"gtfs", "locationTypeEnum"}, {"{location_type | loc2Enum}"}},
	    {SUBJ, {"gtfs", "parent_station"}, {IRI("stops", "{parent_station}")}},

	    // Misc
	    {SUBJ, {"gtfs", "zoneId"}, {"{zone_id}"}},
	    {SUBJ, {"gtfs", "stopTimezone"}, {"{stop_timezone}"}},
	    {SUBJ, {"gtfs", "wheelchairBoarding"}, {"{wheelchair_boarding}", IRI("xs", "integer")}},
	    {SUBJ, {"gtfs", "levelId"}, {"{level_id}"}},
	    {SUBJ, {"gtfs", "platformCode"}, {"{platform_code}"}},
	};

	Schema sch("stops.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, rtc);

	return sch;
}

} // namespace schema