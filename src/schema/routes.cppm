// SPDX-FileCopyrightText: 2026 Jan Babin
// SPDX-License-Identifier: AGPL-3.0-only
//
// Part of gtfs2rdf. See the LICENSE file for details.

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

// exemplary Gtfs -> Rdf schema for routes.txt
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
	    {"xsd", "http://www.w3.org/2001/XMLSchema#"},
	    {"gtfs", "https://w3id.org/gtfs2rdf#"}};

	const IRI SUBJ = IRI("routes", "{route_id}");

	const std::vector<Triple> TRIPLES = {
	    {SUBJ, {"rdf", "type"}, {IRI("gtfs", "Route")}},

	    {SUBJ, {"gtfs", "agency"}, {IRI("agencies", "{agency_id}")}},
	    {SUBJ, {"gtfs", "network"}, {IRI("networks", "{network_id}")}},

	    {SUBJ, {"gtfs", "routeShortName"}, {"{route_short_name}"}},
	    {SUBJ, {"gtfs", "routeLongName"}, {"{route_long_name}"}},
	    {SUBJ, {"gtfs", "routeDesc"}, {"{route_desc}"}},

	    {SUBJ, {"gtfs", "routeType"}, {"{route_type}", IRI("xsd", "integer")}},

	    {SUBJ, {"gtfs", "routeUrl"}, {"{route_url}", IRI("xsd", "anyURI")}},

	    {SUBJ, {"gtfs", "routeColor"}, {"{route_color}"}},
	    {SUBJ, {"gtfs", "routeTextColor"}, {"{route_text_color}"}},

	    {SUBJ,
	     {"gtfs", "routeSortOrder"},
	     {"{route_sort_order}", IRI("xsd", "nonNegativeInteger")}},

	    {SUBJ, {"gtfs", "continuousPickup"}, {"{continuous_pickup}", IRI("xsd", "integer")}},
	    {SUBJ, {"gtfs", "continuousDropOff"}, {"{continuous_drop_off}", IRI("xsd", "integer")}},

	    {SUBJ, {"gtfs", "cemvSupport"}, {"{cemv_support}", IRI("xsd", "integer")}}};

	Schema sch("routes.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, rtc);
	return sch;
}

} // namespace schema
