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

export module schema:routes;
import :core;
import rdf_components;
import field_transforms;
import t_lib;

using namespace rdf;

namespace schema {

// GTFS -> RDF schema for routes.txt
export Schema buildRoutesSchema(field_transforms::TransformRegistry& registry) {
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

  const IRI subj = IRI("routes","{route_id}");

  const std::vector<Triple> triples = {
    // SUBJECT  PREDICATE                  OBJECT
    // Identity / type
    { subj, {"rdf","type"},               { IRI("gtfs","Route") } },

    // Foreign keys
    { subj, {"gtfs","agency"},            { IRI("agencies","{agency_id}") } },
    { subj, {"gtfs","network"},           { IRI("networks","{network_id}") } },

    // Names / description
    { subj, {"gtfs","routeShortName"},    { "{route_short_name}" } },
    { subj, {"gtfs","routeLongName"},     { "{route_long_name}" } },
    { subj, {"gtfs","routeDesc"},         { "{route_desc}" } },

    // Type (required)
    { subj, {"gtfs","routeType"},         { "{route_type}", IRI("xsd","integer") } },

    // URL
    { subj, {"gtfs","routeUrl"},          { "{route_url}", IRI("xsd","anyURI") } },

    // Colors
    { subj, {"gtfs","routeColor"},        { "{route_color}" } },
    { subj, {"gtfs","routeTextColor"},    { "{route_text_color}" } },

    // Sort order (non-negative integer)
    { subj, {"gtfs","routeSortOrder"},  { "{route_sort_order}", IRI("xsd","nonNegativeInteger") } },

    // Continuous pickup/drop-off (enums)
    { subj, {"gtfs","continuousPickup"},  { "{continuous_pickup}", IRI("xsd","integer") } },
    { subj, {"gtfs","continuousDropOff"}, { "{continuous_drop_off}", IRI("xsd","integer") } },

    // cEMV support (enum)
    { subj, {"gtfs","cemvSupport"},       { "{cemv_support}", IRI("xsd","integer") } }
  };

  Schema sc("routes.txt", possible_columns, prefixes, triples, registry);
  return sc;
}

} // namespace
