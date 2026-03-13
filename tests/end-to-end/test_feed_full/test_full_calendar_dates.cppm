// SPDX-FileCopyrightText: 2026 Jan Babin
// SPDX-License-Identifier: AGPL-3.0-only
//
// Part of gtfs2rdf. See the LICENSE file for details.

module;

#include "../../../src/schema/transform_macros.h"

#include <string>
#include <unordered_map>
#include <vector>

export module test_full_calendar_dates;

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

export Schema buildTestFullCalendarDatesSchema(runtime::RuntimeContainer& rtc) {
	const std::vector<std::string> POSSIBLE_COLUMNS = {"service_id", "date", "exception_type"};

	const std::unordered_map<std::string, std::string> PREFIXES = {
	    {"services", "https://gtfs.org/services/"},
	    {"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
	    {"xsd", "http://www.w3.org/2001/XMLSchema#"},
	    {"gtfs", "https://w3id.org/gtfs2rdf#"}};

	const IRI SUBJ = IRI("services", "{service_id}");

	const std::vector<Triple> TRIPLES = {
	    {SUBJ, {"gtfs", "serviceDate"}, {"{date | convertDate2xs_unchecked}", IRI("xsd", "date")}},
	    {SUBJ, {"gtfs", "exceptionType"}, {"{exception_type}", IRI("xsd", "integer")}}};
	Schema sch("test_full_calendar_dates.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, rtc);
	return sch;
}

} // namespace test
