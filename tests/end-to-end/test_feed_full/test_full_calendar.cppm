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

export module test_full_calendar;

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

export Schema buildTestFullCalendarSchema(runtime::RuntimeContainer& rtc) {
	const std::vector<std::string> POSSIBLE_COLUMNS = {"service_id",
	                                                   "monday",
	                                                   "tuesday",
	                                                   "wednesday",
	                                                   "thursday",
	                                                   "friday",
	                                                   "saturday",
	                                                   "sunday",
	                                                   "start_date",
	                                                   "end_date"};

	const std::unordered_map<std::string, std::string> PREFIXES = {
	    {"services", "https://gtfs.org/services/"},
	    {"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
	    {"xs", "http://www.w3.org/2001/XMLSchema#"},
	    {"gtfs", "https://w3id.org/gtfs2rdf#"}};

	const IRI SUBJ = IRI("services", "{service_id}");

	const std::vector<Triple> TRIPLES = {
	    {SUBJ, {"rdf", "type"}, {IRI("gtfs", "Service")}},
	    {SUBJ, {"gtfs", "monday"}, {"{monday}", IRI("xs", "integer")}},
	    {SUBJ, {"gtfs", "tuesday"}, {"{tuesday}", IRI("xs", "integer")}},
	    {SUBJ, {"gtfs", "wednesday"}, {"{wednesday}", IRI("xs", "integer")}},
	    {SUBJ, {"gtfs", "thursday"}, {"{thursday}", IRI("xs", "integer")}},
	    {SUBJ, {"gtfs", "friday"}, {"{friday}", IRI("xs", "integer")}},
	    {SUBJ, {"gtfs", "saturday"}, {"{saturday}", IRI("xs", "integer")}},
	    {SUBJ, {"gtfs", "sunday"}, {"{sunday}", IRI("xs", "integer")}},
	    {SUBJ,
	     {"gtfs", "startDate"},
	     {"{start_date | convertDate2xs_unchecked}", IRI("xs", "date")}},
	    {SUBJ, {"gtfs", "endDate"}, {"{end_date | convertDate2xs_unchecked}", IRI("xs", "date")}}};
	Schema sch("test_full_calendar.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, rtc);
	return sch;
}

} // namespace test
