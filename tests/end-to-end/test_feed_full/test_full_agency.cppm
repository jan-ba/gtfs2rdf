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

export module test_full_agency;

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

export Schema buildTestFullAgencySchema(runtime::RuntimeContainer& rtc) {
	const std::vector<std::string> POSSIBLE_COLUMNS = {"agency_id",
	                                                   "agency_name",
	                                                   "agency_url",
	                                                   "agency_timezone",
	                                                   "agency_lang",
	                                                   "agency_phone",
	                                                   "agency_fare_url",
	                                                   "agency_email"};

	const std::unordered_map<std::string, std::string> PREFIXES = {
	    {"agencies", "https://gtfs.org/agencies/"},
	    {"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
	    {"xsd", "http://www.w3.org/2001/XMLSchema#"},
	    {"gtfs", "https://w3id.org/gtfs2rdf#"}};

	const IRI SUBJ = IRI("agencies", "{agency_id}");

	const std::vector<Triple> TRIPLES = {
	    {SUBJ, {"rdf", "type"}, {IRI("gtfs", "Agency")}},
	    {SUBJ, {"gtfs", "agencyName"}, {"{agency_name}"}},
	    {SUBJ, {"gtfs", "agencyUrl"}, {"{agency_url}", IRI("xsd", "anyURI")}},
	    {SUBJ, {"gtfs", "agencyTimezone"}, {"{agency_timezone}"}},
	    {SUBJ, {"gtfs", "agencyLang"}, {"{agency_lang}", IRI("xsd", "language")}},
	    {SUBJ, {"gtfs", "agencyPhone"}, {"{agency_phone}"}},
	    {SUBJ, {"gtfs", "agencyFareUrl"}, {"{agency_fare_url}", IRI("xsd", "anyURI")}},
	    {SUBJ, {"gtfs", "agencyEmail"}, {"{agency_email}"}}};
	Schema sch("test_full_agency.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, rtc);
	return sch;
}

} // namespace test
