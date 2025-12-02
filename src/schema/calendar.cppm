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

export module schema.calendar;
import schema.core;
import rdf_components;
import field_transforms;
import t_lib;

using namespace rdf;

namespace schema {

// GTFS -> RDF schema for calendar.txt
export Schema buildCalendarSchema(field_transforms::TransformRegistry& registry) {
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

  // NOTE: start_date / end_date are GTFS YYYYMMDD (no dashes).
  // Keep as plain literals unless you normalize to xsd:date (YYYY-MM-DD) during parsing.
  const std::vector<Triple> triples = {
    // SUBJECT                        PREDICATE    OBJECT
    // Type
    { {"services","{service_id}"},    {"rdf","type"},
                                      { IRI("gtfs","Service") } },

    // Weekday flags (0/1)
    { {"services","{service_id}"},    {"gtfs","monday"},
                                      { "{monday}", IRI("xsd","integer") } },
    { {"services","{service_id}"},    {"gtfs","tuesday"},
                                      { "{tuesday}", IRI("xsd","integer") } },
    { {"services","{service_id}"},    {"gtfs","wednesday"},
                                      { "{wednesday}", IRI("xsd","integer") } },
    { {"services","{service_id}"},    {"gtfs","thursday"},
                                      { "{thursday}", IRI("xsd","integer") } },
    { {"services","{service_id}"},    {"gtfs","friday"},
                                      { "{friday}", IRI("xsd","integer") } },
    { {"services","{service_id}"},    {"gtfs","saturday"},
                                      { "{saturday}", IRI("xsd","integer") } },
    { {"services","{service_id}"},    {"gtfs","sunday"},
                                      { "{sunday}", IRI("xsd","integer") } },

    // Date range (plain literals per note above)
    { {"services","{service_id}"},    {"gtfs","startDate"},
                                      { "{start_date}" } },
    { {"services","{service_id}"},    {"gtfs","endDate"},
                                      { "{end_date}" } }
  };

  Schema sc("calendar.txt", possible_columns, prefixes, triples, registry);
  return sc;
}

} // namespace
