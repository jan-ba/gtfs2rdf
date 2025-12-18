// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the GTFS2RDF project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.


module;

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <stdexcept>
#include <chrono>
#include <sstream>

export module schema:calendar;
import :core;
import rdf_components;
import field_transforms;
import t_lib;
import runtime;

using namespace rdf;

namespace schema {

// helper function to parse date in format "YYYYMMDD" into native chrono type
std::chrono::sys_days parseYYYYMMDD(const std::string& s) {
    std::istringstream ss(s);
    std::chrono::sys_days dp{};
    ss >> std::chrono::parse("%Y%m%d", dp);
    return dp;
}

// Transform function to generate operating days string from weekday flags
// Expects 9 arguments (Sunday, Monday, Tuesday, Wednesday, Thursday, Friday, Saturday, start_date, end_date)
void generate_dates(const field_transforms::ArgSpan& args, std::vector<std::string>& out) {
    auto start_date = parseYYYYMMDD(args[7]);
    auto end_date = parseYYYYMMDD(args[8]);

    // loop through each day in the date range
    for (auto current = start_date; current <= end_date; current += std::chrono::days{1}) {
        std::chrono::weekday wd{current};
        int wd_index = wd.c_encoding() % 7;  // weekday index of current date
        
        if (args[wd_index] == "1") {
            // if the service operates on current, store date as "YYYY-MM-DD"
            std::ostringstream oss;
            oss << std::chrono::year_month_day{current};
            out.push_back(oss.str());
        }
    }
}

// GTFS -> RDF schema for calendar.txt
export Schema buildCalendarSchema(runtime::RuntimeContainer& rt) {
  
  rt.getTransformRegistry().registerTransform("generate_dates", generate_dates);

  const std::vector<std::string> possible_columns = {
    "service_id",
    "monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday",
    "start_date", "end_date"
  };

  const std::unordered_map<std::string, std::string> prefixes = {
    { "services", "https://gtfs.de/services/" },
    { "rdf",      "http://www.w3.org/1999/02/22-rdf-syntax-ns#" },
    { "xsd",      "http://www.w3.org/2001/XMLSchema#" },
    { "gtfs",     "https://w3id.org/gtfs2rdf#" }
  };

  const IRI subject = IRI("services","{service_id}");

  const std::vector<Triple> triples = {
    // SUBJECT        PREDICATE                 OBJECT
    // Type
    { subject,        {"rdf","type"},           { IRI("gtfs","Service") } },

    // Weekday flags (0/1)
    { subject,        {"gtfs","monday"},        { "{monday}", IRI("xsd","integer") } },
    { subject,        {"gtfs","tuesday"},       { "{tuesday}", IRI("xsd","integer") } },
    { subject,        {"gtfs","wednesday"},     { "{wednesday}", IRI("xsd","integer") } },
    { subject,        {"gtfs","thursday"},      { "{thursday}", IRI("xsd","integer") } },
    { subject,        {"gtfs","friday"},        { "{friday}", IRI("xsd","integer") } },
    { subject,        {"gtfs","saturday"},      { "{saturday}", IRI("xsd","integer") } },
    { subject,        {"gtfs","sunday"},        { "{sunday}", IRI("xsd","integer") } },

    // Operating dates (generated from weekday flags + start_date + end_date)
    { subject,        {"gtfs", "serviceDate"}, { "{sunday, monday, tuesday, wednesday, thursday,"
                                                  "friday, saturday, start_date, end_date |"
                                                  "generate_dates }", IRI("xsd","date") } }, 

    // Date range (plain literals per note above)
    { subject,        {"gtfs","startDate"},     { "{start_date | convert2xsd:date }", IRI("xsd", "date") } },
    { subject,        {"gtfs","endDate"},       { "{end_date | convert2xsd:date}", IRI("xsd", "date") } }
  };

  Schema sc("calendar.txt", possible_columns, prefixes, triples, rt);
  return sc;
}

} // namespace
