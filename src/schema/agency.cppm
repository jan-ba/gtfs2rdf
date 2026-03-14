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

export module schema:agency;
import :core;
import rdf_components;
import field_transforms;
import t_lib;
import runtime;

using namespace rdf;

namespace schema {

// exemplary Gtfs -> Rdf schema for agency.txt
export Schema buildAgencySchema(runtime::RuntimeContainer& rtc) {
	const std::vector<std::string> POSSIBLE_COLUMNS = {"agency_id",
	                                                   "agency_name",
	                                                   "agency_url",
	                                                   "agency_timezone",
	                                                   "agency_lang",
	                                                   "agency_phone",
	                                                   "agency_fare_url",
	                                                   "agency_email",
	                                                   "cemv_support"};

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
	    {SUBJ, {"gtfs", "agencyEmail"}, {"{agency_email}"}},

	    {SUBJ, {"gtfs", "cemvSupport"}, {"{cemv_support}", IRI("xsd", "integer")}}};

	Schema sch("agencies.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, rtc);
	return sch;
}

} // namespace schema
