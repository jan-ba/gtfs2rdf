// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the GTFS2RDF project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.


module;

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <stdexcept>

#include <iostream> 

export module schema:stops;
import :core;
import rdf_components;
import field_transforms;
import t_lib;
import runtime;

using namespace rdf;

namespace schema {

void location_type_to_enum(const field_transforms::ArgSpan& args, std::string& out) {
    int code = std::stoi(std::string(args[0]));
    switch (code) {
        case 0: out = "stop";           break;
        case 1: out = "station";        break;
        case 2: out = "entrance_exit";  break;
        case 3: out = "generic_node";   break;
        case 4: out = "boarding_area";  break;
        default:
            throw std::runtime_error("❌ Transform error: Unknown location_type code: " 
                                     + std::to_string(code));
    }
}

void wkt_point_lon_lat(const field_transforms::ArgSpan& args, std::string& out) {
    // Convert to double
    double lon = 0.0;
    double lat = 0.0;
    try {
        lon = std::stod(std::string(args[0]));
        lat = std::stod(std::string(args[1]));
    } catch (...) {
        throw std::runtime_error(
            "❌ Transform error: invalid numeric lon/lat: '" +
            std::string(args[0]) + "', '" + std::string(args[1]) + "'");
    }

    // Basic range check
    if (lon < -180.0 || lon > 180.0 || lat < -90.0 || lat > 90.0) {
        throw std::runtime_error(
            "❌ Transform error: lon/lat out of range: lon=" +
            std::to_string(lon) + ", lat=" + std::to_string(lat));
    }

    // Build WKT POINT(lon lat)
    out.clear();
    out.reserve(32);
    out += "POINT(";
    out += std::to_string(lon);
    out.push_back(' ');
    out += std::to_string(lat);
    out.push_back(')');
}



// this gtfs->rdf schema is preliminary and only covers a subset of all possible fields
export Schema buildStopsSchema(runtime::RuntimeContainer& rt) {

  // Register field transforms used in this schema
  rt.getTransformRegistry().registerTransform("loc2Enum", location_type_to_enum);
  rt.getTransformRegistry().registerTransform("wktPointLonLat", wkt_point_lon_lat);
  // possibly not required
  const std::vector<std::string> possible_columns = {
      "stop_id", "stop_code", "stop_name", "stop_desc", "stop_lat", "stop_lon",
      "zone_id", "stop_url", "location_type", "parent_station", "stop_timezone",
      "wheelchair_boarding", "level_id", "platform_code" };

  const std::unordered_map<std::string, std::string> prefixes = {
    { "stops",     "https://gtfs.de/stops/" },
    { "stoptimes", "https://gtfs.de/stop_times/" },
    { "rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#" },
    { "xsd", "http://www.w3.org/2001/XMLSchema#" },
    { "gtfs",  "https://w3id.org/gtfs2rdf#" },
    { "wgs", "http://www.w3.org/2003/01/geo/wgs84_pos#" },
    { "geo", "http://www.opengis.net/ont/geosparql#" },
    { "gtfs2rdfgeom", "https://w3id.org/gtfs2rdf/geometry#" } };

  
  const IRI subj = IRI("stops","{stop_id}");

  // Ontology (?)
  const std::vector<Triple> triples = {
    // SUBJECT                   PREDICATE          OBJECT

    // Type
    { subj,  {"rdf","type"},    { IRI("gtfs","Stop") } },

    // Labels / desc / code / URL
    { subj,  {"gtfs","stopName"},     { "{stop_name}", "de" } },
    { subj,  {"gtfs","stopDesc"},     { "{stop_desc}", "de" } },
    { subj,  {"gtfs","stopCode"},     { "{stop_code}" } },
    { subj,  {"gtfs","stopUrl"},      { "{stop_url}", IRI("xsd","anyURI") } },

    // Geometry (WGS84 + GeoSPARQL WKT)
    { subj,  {"wgs","lat"},           { "{stop_lat}", IRI("xsd","decimal") } },
    { subj,  {"wgs","long"},          { "{stop_lon}", IRI("xsd","decimal") } },
    { subj,  {"geo","hasGeometry"},   { IRI("gtfs2rdfgeom","stop_{stop_id}") } },
    { subj,  {"geo","asWKT"},         { "POINT({stop_lon} {stop_lat})", IRI("geo","wktLiteral") } },

    // Hierarchy / location type
    { subj,  {"gtfs","locationType"},     { "{location_type}", IRI("xsd","integer") } },
    { subj,  {"gtfs","locationTypeEnum"}, { "{location_type|loc2Enum}" } },
    { subj,  {"gtfs","parentStation"},    { IRI("stops","{parent_station}") } },

    // Misc
    { subj,  {"gtfs","zoneId"},             { "{zone_id}" } },
    { subj,  {"gtfs","stopTimezone"},       { "{stop_timezone}" } },
    { subj,  {"gtfs","wheelchairBoarding"}, { "{wheelchair_boarding}", IRI("xsd","integer") } },
    { subj,  {"gtfs","levelId"},            { "{level_id}" } },
    { subj,  {"gtfs","platformCode"},       { "{platform_code}" } }
  };

  Schema sc("stops.txt", possible_columns, prefixes, triples, rt);

  return sc;
}

} // namespace