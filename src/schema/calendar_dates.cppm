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

export module schema.calendar_dates;
import schema.core;
import rdf_components;
import field_transforms;
import t_lib;

using namespace rdf;

namespace schema {

// GTFS -> RDF schema for calendar_dates.txt
export Schema buildCalendarDatesSchema(field_transforms::TransformRegistry& registry) {
  const std::vector<std::string> possible_columns = {
    "service_id", "date", "exception_type"
  };

  const std::unordered_map<std::string, std::string> prefixes = {
    { "caldates", "https://gtfs.de/calendar_dates/" },
    { "services", "https://gtfs.de/services/" },

    { "rdf",  "http://www.w3.org/1999/02/22-rdf-syntax-ns#" },
    { "xsd",  "http://www.w3.org/2001/XMLSchema#" },
    { "gtfs", "https://w3id.org/gtfs2rdf#" }
  };

  // NOTE:
  // - We keep `date` as a plain literal because GTFS uses YYYYMMDD (no dashes); mapping to
  //   xsd:date would require transforming to YYYY-MM-DD during parsing.
  // TODO: add proper parsing of datatypes such as date
  const std::vector<Triple> triples = {
    // SUBJECT                               PREDICATE         OBJECT
    // Identity / type
    { {"caldates","{service_id}_{date}"},     {"rdf","type"},   { IRI("gtfs","CalendarDate") } },

    // Link to the service this exception refers to
    { {"caldates","{service_id}_{date}"},     {"gtfs","service"},
                                                { IRI("services","{service_id}") } },

    // The date of the exception (plain literal; see note above)
    { {"caldates","{service_id}_{date}"},     {"gtfs","date"},  { "{date}" } },

    // Exception type: 1 = added, 2 = removed
    { {"caldates","{service_id}_{date}"},     {"gtfs","exceptionType"},
                                                { "{exception_type}", IRI("xsd","integer") } }
  };

  Schema sc("calendar_dates.txt", possible_columns, prefixes, triples, registry);
  return sc;
}

} // namespace
