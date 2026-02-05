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

export module schema:calendar_dates;
import :core;
import rdf_components;
import field_transforms;
import t_lib;
import runtime;

using namespace rdf;

namespace schema {

// Gtfs -> Rdf schema for calendar_dates.txt
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
	TRANSFORM2ONE(is_disabled_date, ARGS, OUT_VAL, STORAGE) {
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
	    {"xs", "http://www.w3.org/2001/XMLSchema#"},
	    {"gtfs", "https://w3id.org/gtfs2rdf#"}};

	const IRI SUBJ = IRI("caldates", "{service_id}");

	const std::vector<Triple> TRIPLES = {
	    // SUBJECT            PREDICATE                   OBJECT

	    // date of the exception (if exception_type is 1, the service is added for the specified
	    // date,
	    //                        else ignored)
	    {SUBJ,
	     {"gtfs", "serviceDate"},
	     {"{ date, exception_type | ignore_disabled_dates"
	      "| convertDate2xs_unchecked }",
	      IRI("xs", "date")}},
	};

	const std::vector<std::string> STORAGE_ONLY = {
	    // store disabled dates for later use in calendar.txt
	    "{ service_id : date, exception_type | is_disabled_date | convertDate2xs_unchecked"
	    "> disabled_dates@calendar_dates.txt }"};

	Schema sch("calendar_dates.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, STORAGE_ONLY, rtc);
	return sch;
}

} // namespace schema
