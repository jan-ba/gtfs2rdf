// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the GTFS2RDF project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.

module;

#include "transform_macros.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <stdexcept>

export module schema:calendar_dates;
import :core;
import rdf_components;
import field_transforms;
import t_lib;
import runtime;

using namespace rdf;

namespace schema {

void ignore_disabled_dates(const field_transforms::ArgSpan& args, std::string& out) {
    // args: date, exception_type
    if (args[1] == "2") {
        // exception_type 2 = removed -> ignore
        return;
    } else if (args[1] == "1") {
        // exception_type 1 = added -> output date
        out = args[0];
    } else {
        throw std::runtime_error("❌  Transform error: invalid exception_type '" + args[1] + "' in ignore_disabled_dates");
    }
}

// filter function: write to string if true else leave empty
void is_disabled_date(const field_transforms::ArgSpan& args, std::string& out) {
    // args: service_id, date, exception_type
    if (args[2] == "2") {
        out = args[1];  // output date
    } else {
        return;  // leave empty
    }
}

// GTFS -> RDF schema for calendar_dates.txt
export Schema buildCalendarDatesSchema(runtime::RuntimeContainer& rt) {
    rt.getTransformRegistry().registerTransform("ignore_disabled_dates", ignore_disabled_dates);
    rt.getTransformRegistry().registerTransform("is_disabled_date", is_disabled_date);

    const std::vector<std::string> possible_columns = {
        "service_id", "date", "exception_type"
    };

    const std::unordered_map<std::string, std::string> prefixes = {
        { "caldates", "https://gtfs.org/calendar_dates/" },
        { "services", "https://gtfs.org/services/" },

        { "rdf",  "http://www.w3.org/1999/02/22-rdf-syntax-ns#" },
        { "xs",  "http://www.w3.org/2001/XMLSchema#" },
        { "gtfs", "https://w3id.org/gtfs2rdf#" }
    };

    const IRI subject = IRI("caldates","{service_id}");

    const std::vector<Triple> triples = {
        // SUBJECT            PREDICATE                   OBJECT

        // date of the exception (if exception_type is 1, the service is added for the specified date,
        //                        else ignored)
        { subject,   {"gtfs","serviceDate"}, { "{ date, exception_type | ignore_disabled_dates | "\
                                                  "convert_date }", IRI("xs", "date") } },
                      
        { subject, {"gtfs","movelater"},
        { "{ service_id : date, exception_type | is_disabled_date | convert_date"
          "> disabled_dates@calendar_dates.txt }" } }
    };

    Schema sc("calendar_dates.txt", possible_columns, prefixes, triples, rt);
    return sc;
}

} // namespace
