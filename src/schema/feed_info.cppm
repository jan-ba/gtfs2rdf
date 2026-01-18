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

export module schema:feed_info;

import :core;
import rdf_components;
import field_transforms;
import t_lib;
import runtime;

using namespace rdf;

namespace schema {

export Schema buildFeedInfoSchema(runtime::RuntimeContainer& rt) {
    const std::vector<std::string> possible_columns = {
        "feed_publisher_name",
        "feed_publisher_url",
        "feed_lang",
        "default_lang",
        "feed_start_date",
        "feed_end_date",
        "feed_version",
        "feed_contact_email",
        "feed_contact_url"
    };

    const std::unordered_map<std::string, std::string> prefixes = {
        { "feedinfo", "https://gtfs.org/feed_info/" },
        { "gtfs",     "https://w3id.org/gtfs2rdf#" },
        { "rdf",      "http://www.w3.org/1999/02/22-rdf-syntax-ns#" },
        { "xsd",      "http://www.w3.org/2001/XMLSchema#" }
    };

    const IRI subject = IRI("feedinfo", "feed");

    const std::vector<Triple> triples = {
        { subject, {"gtfs","feedLang"}, { "{feed_lang > FEED_LANG}" } },
        { subject, {"gtfs","feedStartDate"}, { "{feed_start_date | convert_date}"} },
        { subject, {"gtfs","feedEndDate"}, { "{feed_end_date | convert_date}"} }
    };

    Schema sc("feed_info.txt", possible_columns, prefixes, triples, rt);
    return sc;
}

} // namespace schema
