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

export module schema.routes;
import schema.core;
import rdf_components;

using namespace rdf;

export namespace schema {

// GTFS -> RDF schema for routes.txt
Schema buildRoutesSchema() {
  const std::vector<std::string> possible_columns = {
    "route_id", "agency_id",
    "route_short_name", "route_long_name", "route_desc",
    "route_type",
    "route_url",
    "route_color", "route_text_color",
    "route_sort_order",
    "continuous_pickup", "continuous_drop_off",
    "network_id",
    "cemv_support"
  };

  const std::unordered_map<std::string, std::string> prefixes = {
    { "routes",   "https://gtfs.de/routes/" },
    { "agencies", "https://gtfs.de/agencies/" },
    { "networks", "https://gtfs.de/networks/" },

    { "rdf",  "http://www.w3.org/1999/02/22-rdf-syntax-ns#" },
    { "xsd",  "http://www.w3.org/2001/XMLSchema#" },
    { "gtfs", "https://w3id.org/gtfs2rdf#" }
  };

  const std::vector<Triple> triples = {
    // SUBJECT                        PREDICATE            OBJECT
    // Identity / type
    { {"routes","{route_id}"},        {"rdf","type"},       { IRI("gtfs","Route") } },

    // Foreign keys
    { {"routes","{route_id}"},        {"gtfs","agency"},    { IRI("agencies","{agency_id}") } },
    { {"routes","{route_id}"},        {"gtfs","network"},   { IRI("networks","{network_id}") } },

    // Names / description
    { {"routes","{route_id}"},        {"gtfs","routeShortName"}, { "{route_short_name}" } },
    { {"routes","{route_id}"},        {"gtfs","routeLongName"},  { "{route_long_name}" } },
    { {"routes","{route_id}"},        {"gtfs","routeDesc"},      { "{route_desc}" } },

    // Type (required)
    { {"routes","{route_id}"},        {"gtfs","routeType"},
                                       { "{route_type}", IRI("xsd","integer") } },

    // URL
    { {"routes","{route_id}"},        {"gtfs","routeUrl"},
                                       { "{route_url}", IRI("xsd","anyURI") } },

    // Colors
    { {"routes","{route_id}"},        {"gtfs","routeColor"},     { "{route_color}" } },
    { {"routes","{route_id}"},        {"gtfs","routeTextColor"}, { "{route_text_color}" } },

    // Sort order (non-negative integer)
    { {"routes","{route_id}"},        {"gtfs","routeSortOrder"},
                                       { "{route_sort_order}", IRI("xsd","nonNegativeInteger") } },

    // Continuous pickup/drop-off (enums)
    { {"routes","{route_id}"},        {"gtfs","continuousPickup"},
                                       { "{continuous_pickup}", IRI("xsd","integer") } },
    { {"routes","{route_id}"},        {"gtfs","continuousDropOff"},
                                       { "{continuous_drop_off}", IRI("xsd","integer") } },

    // cEMV support (enum)
    { {"routes","{route_id}"},        {"gtfs","cemvSupport"},
                                       { "{cemv_support}", IRI("xsd","integer") } }
  };

  Schema sc("routes.txt", possible_columns, prefixes, triples);
  return sc;
}

} // namespace
