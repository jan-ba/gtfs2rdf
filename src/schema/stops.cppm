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

export module schema.stops;
import schema.core;
import rdf_components;

using namespace rdf;

export namespace schema {

// this gtfs->rdf schema is preliminary and only covers a subset of all possible fields
Schema buildStopsSchema() {

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


  // Ontology (?)
  const std::vector<Triple> triples = {
    // SUBJECT                   PREDICATE          OBJECT

    // Type
    { {"stops","{stop_id}"},  {"rdf","type"},    { IRI("gtfs","Stop") } },

    // Labels / desc / code / URL
    { {"stops","{stop_id}"},  {"gtfs","stopName"},   { "{stop_name}", "de" } },
    { {"stops","{stop_id}"},  {"gtfs","stopDesc"},   { "{stop_desc}", "de" } },
    { {"stops","{stop_id}"},  {"gtfs","stopCode"},   { "{stop_code}" } },
    { {"stops","{stop_id}"},  {"gtfs","stopUrl"},    { "{stop_url}", IRI("xsd","anyURI") } },

    // Geometry (WGS84 + GeoSPARQL WKT)
    { {"stops","{stop_id}"},  {"wgs","lat"},         { "{stop_lat}", IRI("xsd","decimal") } },
    { {"stops","{stop_id}"},  {"wgs","long"},        { "{stop_lon}", IRI("xsd","decimal") } },
    { {"stops","{stop_id}"},  {"geo","hasGeometry"}, { IRI("gtfs2rdfgeom","stop_{stop_id}") } },
    { {"gtfs2rdfgeom","stop_{stop_id}"}, {"geo","asWKT"},
                                      { "POINT({stop_lon} {stop_lat})", IRI("geo","wktLiteral") } },

    // Hierarchy / location type
    { {"stops","{stop_id}"},  {"gtfs","locationType"},  { "{location_type}", IRI("xsd","integer") } },
    { {"stops","{stop_id}"},  {"gtfs","parentStation"}, { IRI("stops","{parent_station}") } },

    // Misc
    { {"stops","{stop_id}"},  {"gtfs","zoneId"},            { "{zone_id}" } },
    { {"stops","{stop_id}"},  {"gtfs","stopTimezone"},      { "{stop_timezone}" } },
    { {"stops","{stop_id}"},  {"gtfs","wheelchairBoarding"},{ "{wheelchair_boarding}", IRI("xsd","integer") } },
    { {"stops","{stop_id}"},  {"gtfs","levelId"},           { "{level_id}" } },
    { {"stops","{stop_id}"},  {"gtfs","platformCode"},      { "{platform_code}" } }
  };

  Schema sc("stops.txt", possible_columns, prefixes, triples, true, true, true);

  return sc;
}

} // namespace