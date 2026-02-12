// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the gtfs2rdf project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.

module;

#include "../../../src/schema/transform_macros.h"

#include <string>
#include <unordered_map>
#include <vector>

export module test_full_routes;

import schema;
import rdf_components;
import field_transforms;
import t_lib;
import runtime;

using namespace rdf;
using namespace schema;

namespace test {

// _________________________________________________________________________________________________
// THIS SCHEMA WAS SOLELY BUILT FOR TESTING PURPOSES IN THE FULL FEED
// _________________________________________________________________________________________________

export Schema buildTestFullRoutesSchema(runtime::RuntimeContainer& rtc) {
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
	    {SUBJ, {"rdf", "type"}, {IRI("gtfs", "Route")}},
	    {SUBJ, {"gtfs", "agency"}, {IRI("agencies", "{agency_id}")}},
	    {SUBJ, {"gtfs", "routeShortName"}, {"{route_short_name}"}},
	    {SUBJ,
	     {"gtfs", "routeLongName"},
	     {"{route_long_name}", "{FEED_LANG@test_full_feed_info.txt}"}},
	    {SUBJ, {"gtfs", "routeType"}, {"{route_type}", IRI("xs", "integer")}},
	    {SUBJ, {"gtfs", "routeUrl"}, {"{route_url}", IRI("xs", "anyURI")}},
	    {SUBJ, {"gtfs", "routeColor"}, {"{route_color}"}},
	    {SUBJ, {"gtfs", "routeTextColor"}, {"{route_text_color}"}}};
	Schema sch("test_full_routes.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, rtc);
	return sch;
}

} // namespace test
