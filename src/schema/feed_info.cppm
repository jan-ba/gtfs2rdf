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
        { "feed_info", "https://gtfs.org/feed_info/" },
        { "gtfs",     "https://w3id.org/gtfs2rdf#" },
        { "rdf",      "http://www.w3.org/1999/02/22-rdf-syntax-ns#" },
        { "xs",      "http://www.w3.org/2001/XMLSchema#" }
    };

    const std::vector<std::string> storage_only_instructions = {
        "{ feed_lang > FEED_LANG }",
        "{ feed_start_date | convert_date > FEED_START_DATE }",
        "{ feed_end_date | convert_date > FEED_END_DATE }",
    };

    // TODO: overload Schema constructor so that triples are not required as argument
    const std::vector<Triple> triples = {
    };

    Schema sc("feed_info.txt", possible_columns, prefixes, triples, storage_only_instructions, rt);
    return sc;
}

} // namespace schema
