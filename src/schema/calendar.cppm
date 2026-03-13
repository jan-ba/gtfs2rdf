// SPDX-FileCopyrightText: 2026 Jan Babin
// SPDX-License-Identifier: AGPL-3.0-only
//
// Part of gtfs2rdf. See the LICENSE file for details.

module;

#include "transform_macros.h"

#include <chrono>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

export module schema:calendar;
import :core;
import rdf_components;
import field_transforms;
import t_lib;
import runtime;
import util;

using namespace rdf;
using namespace util;

namespace schema {

// Gtfs -> Rdf schema for calendar.txt
export Schema buildCalendarSchema(runtime::RuntimeContainer& rtc) {
	// Transform function to generate operating days string from weekday flags
	// ignores disables dates from calendar_dates.txt
	// Expects 10 arguments (Sunday, Monday, Tuesday, Wednesday, Thursday, Friday, Saturday,
	//                       start_date, end_date, service_id)
	TRANSFORM2MANY(generateDates, ARGS, OUT_VAL, STORAGE) {
		if (ARGS[7].size() != 8 || ARGS[8].size() != 8) {
			TRANSFORM_ERROR("invalid date string format, expected 'YYYYMMDD', got '" +
			                std::string(ARGS[7]) + "' and '" + std::string(ARGS[8]) + "'");
		}
		for (char c : ARGS[7]) {
			if (c < '0' || c > '9') {
				TRANSFORM_ERROR("invalid date string format, expected 'YYYYMMDD', got '" +
				                std::string(ARGS[7]) + "'");
			}
		}
		for (char c : ARGS[8]) {
			if (c < '0' || c > '9') {
				TRANSFORM_ERROR("invalid date string format, expected 'YYYYMMDD', got '" +
				                std::string(ARGS[8]) + "'");
			}
		}
		auto start_date = util::parseYYYYMMDD(ARGS[7]);
		auto end_date = util::parseYYYYMMDD(ARGS[8]);
		// loop through each day in the date range
		for (auto current = start_date; current <= end_date; current += std::chrono::days{1}) {
			std::chrono::weekday wd{current};
			int wd_index = wd.c_encoding() % 7; // weekday index of current date

			if (ARGS[wd_index] == "1") {
				// if the service operates on current, store date as "YYYY-MM-DD"
				std::ostringstream oss;
				oss << std::chrono::year_month_day{current};
				auto date = oss.str();
				// this is a quick lookup whether the date is disabled in calendar_dates.txt
				if (!STORAGE.containsValue("calendar_dates.txt", "disabled_dates", ARGS[9], date)) {
					OUT_VAL.push_back(date);
				}
			}
		}
	}
	TRANSFORM_END

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
	    {"xsd", "http://www.w3.org/2001/XMLSchema#"},
	    {"gtfs", "https://w3id.org/gtfs2rdf#"}};

	const IRI SUBJ = IRI("services", "{service_id}");

	const std::vector<Triple> TRIPLES = {
	    {SUBJ, {"rdf", "type"}, {IRI("gtfs", "Service")}},

	    // Operating dates (generated from weekday flags + start_date + end_date)
	    // uses calendar_dates.txt to ignore disabled dates
	    {SUBJ,
	     {"gtfs", "serviceDate"},
	     {"{sunday, monday, tuesday, wednesday, thursday,"
	      "friday, saturday, start_date, end_date,"
	      "service_id | generateDates@calendar_dates.txt }",
	      IRI("xsd", "date")}},

	    // Date range
	    {SUBJ,
	     {"gtfs", "startDate"},
	     {"{start_date | convertDate2xs_unchecked }", IRI("xsd", "date")}},
	    {SUBJ, {"gtfs", "endDate"}, {"{end_date | convertDate2xs_unchecked}", IRI("xsd", "date")}}};

	Schema sch("calendar.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, rtc);
	return sch;
}

} // namespace schema
