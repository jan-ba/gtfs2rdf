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

export module schema:calendar_dates;
import :core;
import rdf_components;
import field_transforms;
import t_lib;
import runtime;

using namespace rdf;

namespace schema {

// exemplary Gtfs -> Rdf schema for calendar_dates.txt
export Schema buildCalendarDatesSchema(runtime::RuntimeContainer& rtc) {
	// args: date, exception_type
	TRANSFORM2ONE(ignore_disabled_dates, ARGS, OUT_VAL, STORAGE) {
		if (ARGS[1] == "2") {
			return; // exception_type 2 = removed -> ignore
		}
		if (ARGS[1] == "1") {
			// exception_type 1 = added -> output date
			OUT_VAL = ARGS[0];
		} else {
			TRANSFORM_ERROR("invalid exception type '" + std::string(ARGS[1]) + "'");
		}
	}
	TRANSFORM_END

	// args: service_id, date, exception_type
	TRANSFORM2ONE(isDisabledDate, ARGS, OUT_VAL, STORAGE) {
		if (ARGS[2] == "2") {
			OUT_VAL = ARGS[1]; // output date
		} else {
			return; // leave empty
		}
	}
	TRANSFORM_END

	const std::vector<std::string> POSSIBLE_COLUMNS = {"service_id", "date", "exception_type"};

	const std::unordered_map<std::string, std::string> PREFIXES = {
	    {"caldates", "https://gtfs.org/calendar_dates/"},
	    {"services", "https://gtfs.org/services/"},

	    {"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
	    {"xsd", "http://www.w3.org/2001/XMLSchema#"},
	    {"gtfs", "https://w3id.org/gtfs2rdf#"}};

	const IRI SUBJ = IRI("caldates", "{service_id}");

	const std::vector<Triple> TRIPLES = {
	    {SUBJ,
	     {"gtfs", "serviceDate"},
	     {"{ date, exception_type | ignore_disabled_dates | convertDate2xs_unchecked }",
	      IRI("xsd", "date")}}};

	const std::vector<std::string> STORAGE_ONLY = {
	    // store disabled dates for later use in calendar.txt
	    "{ service_id : date, exception_type | isDisabledDate | convertDate2xs_unchecked"
	    "> disabled_dates@calendar_dates.txt }"};

	Schema sch("calendar_dates.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, STORAGE_ONLY, rtc);
	return sch;
}

} // namespace schema
