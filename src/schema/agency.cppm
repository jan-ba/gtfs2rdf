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

export module schema:agency;
import :core;
import rdf_components;
import field_transforms;
import t_lib;
import runtime;

using namespace rdf;

namespace schema {

// Gtfs -> Rdf schema for agency.txt
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
	    {"xs", "http://www.w3.org/2001/XMLSchema#"},
	    {"gtfs", "https://w3id.org/gtfs2rdf#"}};

	// since subject does not change per triple, we can use placeholders for agency_id for easier
	// readability
	const IRI SUBJ = IRI("agencies", "{agency_id}");

	const std::vector<Triple> TRIPLES = {
	    // SUBJECT                         PREDICATE           OBJECT
	    // Type
	    {SUBJ, {"rdf", "type"}, {IRI("gtfs", "Agency")}},

	    // Core fields
	    {SUBJ, {"gtfs", "agencyName"}, {"{agency_name}"}},
	    {SUBJ, {"gtfs", "agencyUrl"}, {"{agency_url}", IRI("xs", "anyURI")}},
	    {SUBJ, {"gtfs", "agencyTimezone"}, {"{agency_timezone}"}},

	    // Optional fields
	    {SUBJ, {"gtfs", "agencyLang"}, {"{agency_lang}", IRI("xs", "language")}},
	    {SUBJ, {"gtfs", "agencyPhone"}, {"{agency_phone}"}},
	    {SUBJ, {"gtfs", "agencyFareUrl"}, {"{agency_fare_url}", IRI("xs", "anyURI")}},
	    {SUBJ, {"gtfs", "agencyEmail"}, {"{agency_email}"}},

	    // cEMV support enum (0/1/2)
	    {SUBJ, {"gtfs", "cemvSupport"}, {"{cemv_support}", IRI("xs", "integer")}}};

	Schema sch("agencies.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, rtc);
	return sch;
}

} // namespace schema
