// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the GTFS2RDF project.
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
export Schema buildStopsSchema(runtime::RuntimeContainer &rt) {
	// prints out enum string for location_type codes
	// ARGS[0]: location_type code
	// TODO: rework exception handling
	TRANSFORM2ONE(loc2Enum, ARGS, OUT_VAL, STORAGE)
	if (ARGS[0].size() != 1)
		return;
	char c = ARGS[0][0];
	if (c < '0' || c > '4') {
		throw std::runtime_error("❌ Transform error: Unknown location_type code: " +
		                         std::string(ARGS[0]));
	}
	switch (c) {
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
	TRANSFORM_END

	// builds WKT POINT(lon lat) from lon and lat strings
	// ARGS[0]: longitude, ARGS[1]: latitude
	// TODO: rework exception handling
	TRANSFORM2ONE(wkt_point_lon_lat, ARGS, OUT_VAL, STORAGE)
	if (ARGS[0].empty() || ARGS[1].empty())
		return;
	// convert to double
	double lon = 0.0;
	double lat = 0.0;
	try {
		lon = std::stod(std::string(ARGS[0]));
		lat = std::stod(std::string(ARGS[1]));
	} catch (...) {
		throw std::runtime_error("❌ Transform error: invalid numeric lon/lat: '" +
		                         std::string(ARGS[0]) + "', '" + std::string(ARGS[1]) + "'");
	}

	// basic range check
	if (lon < -180.0 || lon > 180.0 || lat < -90.0 || lat > 90.0) {
		throw std::runtime_error("❌ Transform error: lon/lat out of range: lon=" +
		                         std::to_string(lon) + ", lat=" + std::to_string(lat));
	}

	// build WKT POINT(lon lat)
	OUT_VAL.append("POINT(");
	OUT_VAL.append(std::to_string(lon));
	OUT_VAL.push_back(' ');
	OUT_VAL.append(std::to_string(lat));
	OUT_VAL.push_back(')');
	TRANSFORM_END

	// Possible columns in stops.txt
	const std::vector<std::string> possible_columns = {"stop_id",
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

	const std::unordered_map<std::string, std::string> prefixes = {
	    {"stops", "https://gtfs.org/stops/"},
	    {"stop_times", "https://gtfs.org/stop_times/"},
	    {"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
	    {"xs", "http://www.w3.org/2001/XMLSchema#"},
	    {"gtfs", "https://w3id.org/gtfs2rdf#"},
	    {"wgs", "http://www.w3.org/2003/01/geo/wgs84_pos#"},
	    {"geo", "http://www.opengis.net/ont/geosparql#"},
	    {"gtfs2rdfgeom", "https://w3id.org/gtfs2rdf/geometry#"}};

	const IRI subj = IRI("stops", "{stop_id}");

	// Ontology (?)
	const std::vector<Triple> triples = {
	    // SUBJECT                   PREDICATE          OBJECT

	    // Type
	    {subj, {"rdf", "type"}, {IRI("gtfs", "Stop")}},

	    // Labels / desc / code / URL
	    // { subj,  {"gtfs","stopName"},     { "{stop_name}", "de" } },
	    {subj, {"gtfs", "stopName"}, {"{stop_name}", "{FEED_LANG@feed_info.txt}"}},

	    // Using translation transform to get translated stop names
	    // expected args of get_translation: table_name, field_name, field_value
	    // TODO: if  transform not found, ignore instruction (i.e., no translation available)
	    // { subj,  {"gtfs", "stopName"},    { "{\"stops\", \"stop_name\", stop_name"
	    //                                         "| get_translation@translations.txt}",
	    //                                     "{latest_translation_lookup@translations.txt}" } },

	    {subj, {"gtfs", "stopDesc"}, {"{stop_desc}", "{FEED_LANG@feed_info.txt}"}},
	    {subj, {"gtfs", "stopCode"}, {"{stop_code}"}},
	    {subj, {"gtfs", "stopUrl"}, {"{stop_url}", IRI("xs", "anyURI")}},

	    // Geometry (WGS84 + GeoSPARQL WKT)
	    {subj, {"wgs", "lat"}, {"{stop_lat}", IRI("xs", "decimal")}},
	    {subj, {"wgs", "long"}, {"{stop_lon}", IRI("xs", "decimal")}},
	    {subj, {"geo", "hasGeometry"}, {IRI("gtfs2rdfgeom", "stop_{stop_id}")}},
	    {subj, {"geo", "asWKT"}, {"POINT({stop_lon} {stop_lat})", IRI("geo", "wktLiteral")}},

	    // Hierarchy / location type
	    {subj, {"gtfs", "locationType"}, {"{location_type}", IRI("xs", "integer")}},
	    {subj, {"gtfs", "locationTypeEnum"}, {"{location_type|loc2Enum}"}},
	    {subj, {"gtfs", "parentStation"}, {IRI("stops", "{parent_station}")}},

	    // Misc
	    {subj, {"gtfs", "zoneId"}, {"{zone_id}"}},
	    {subj, {"gtfs", "stopTimezone"}, {"{stop_timezone}"}},
	    {subj, {"gtfs", "wheelchairBoarding"}, {"{wheelchair_boarding}", IRI("xs", "integer")}},
	    {subj, {"gtfs", "levelId"}, {"{level_id}"}},
	    {subj, {"gtfs", "platformCode"}, {"{platform_code}"}}};

	Schema sc("stops.txt", possible_columns, prefixes, triples, rt);

	return sc;
}

} // namespace schema