// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the gtfs2rdf project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.

module;

#include "transform_macros.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

export module schema:routes;
import :core;
import rdf_components;
import field_transforms;
import t_lib;
import runtime;

using namespace rdf;

namespace schema {

// Gtfs -> Rdf schema for routes.txt
export Schema buildRoutesSchema(runtime::RuntimeContainer& rtc) {
	const std::vector<std::string> POSSIBLE_COLUMNS = {"route_id",
	                                                   "agency_id",
	                                                   "route_short_name",
	                                                   "route_long_name",
	                                                   "route_desc",
	                                                   "route_type",
	                                                   "route_url",
	                                                   "route_color",
	                                                   "route_text_color",
	                                                   "route_sort_order",
	                                                   "continuous_pickup",
	                                                   "continuous_drop_off",
	                                                   "network_id",
	                                                   "cemv_support"};

	const std::unordered_map<std::string, std::string> PREFIXES = {
	    {"routes", "https://gtfs.org/routes/"},
	    {"agencies", "https://gtfs.org/agencies/"},
	    {"networks", "https://gtfs.org/networks/"},

	    {"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
	    {"xs", "http://www.w3.org/2001/XMLSchema#"},
	    {"gtfs", "https://w3id.org/gtfs2rdf#"}};

	const IRI SUBJ = IRI("routes", "{route_id}");

	const std::vector<Triple> TRIPLES = {
	    // SUBJECT  PREDICATE                  OBJECT
	    // Identity / type
	    {SUBJ, {"rdf", "type"}, {IRI("gtfs", "Route")}},

	    // Foreign keys
	    {SUBJ, {"gtfs", "agency"}, {IRI("agencies", "{agency_id}")}},
	    {SUBJ, {"gtfs", "network"}, {IRI("networks", "{network_id}")}},

	    // Names / description
	    {SUBJ, {"gtfs", "routeShortName"}, {"{route_short_name}"}},
	    {SUBJ, {"gtfs", "routeLongName"}, {"{route_long_name}", "{FEED_LANG@feed_info.txt}"}},
	    {SUBJ, {"gtfs", "routeDesc"}, {"{route_desc}", "{FEED_LANG@feed_info.txt}"}},

	    // Type (required)
	    {SUBJ, {"gtfs", "routeType"}, {"{route_type}", IRI("xs", "integer")}},

	    // URL
	    {SUBJ, {"gtfs", "routeUrl"}, {"{route_url}", IRI("xs", "anyURI")}},

	    // Colors
	    {SUBJ, {"gtfs", "routeColor"}, {"{route_color}"}},
	    {SUBJ, {"gtfs", "routeTextColor"}, {"{route_text_color}"}},

	    // Sort order (non-negative integer)
	    {SUBJ, {"gtfs", "routeSortOrder"}, {"{route_sort_order}", IRI("xs", "nonNegativeInteger")}},

	    // Continuous pickup/drop-off (enums)
	    {SUBJ, {"gtfs", "continuousPickup"}, {"{continuous_pickup}", IRI("xs", "integer")}},
	    {SUBJ, {"gtfs", "continuousDropOff"}, {"{continuous_drop_off}", IRI("xs", "integer")}},

	    // cEMV support (enum)
	    {SUBJ, {"gtfs", "cemvSupport"}, {"{cemv_support}", IRI("xs", "integer")}}};

	Schema sch("routes.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, rtc);
	return sch;
}

} // namespace schema
