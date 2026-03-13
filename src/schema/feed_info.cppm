// SPDX-FileCopyrightText: 2026 Jan Babin
// SPDX-License-Identifier: AGPL-3.0-only
//
// Part of gtfs2rdf. See the LICENSE file for details.

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

export Schema buildFeedInfoSchema(runtime::RuntimeContainer& rtc) {
	const std::vector<std::string> POSSIBLE_COLUMNS = {"feed_publisher_name",
	                                                   "feed_publisher_url",
	                                                   "feed_lang",
	                                                   "default_lang",
	                                                   "feed_start_date",
	                                                   "feed_end_date",
	                                                   "feed_version",
	                                                   "feed_contact_email",
	                                                   "feed_contact_url"};

	const std::unordered_map<std::string, std::string> PREFIXES = {
	    {"feed_info", "https://gtfs.org/feed_info/"},
	    {"gtfs", "https://w3id.org/gtfs2rdf#"},
	    {"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
	    {"xsd", "http://www.w3.org/2001/XMLSchema#"}};

	const std::vector<std::string> NO_WRITE_INSTRUCTIONS = {
	    "{ feed_lang > FEED_LANG }",
	    "{ feed_start_date | convertDate2xs_unchecked > FEED_START_DATE }",
	    "{ feed_end_date | convertDate2xs_unchecked > FEED_END_DATE }"};

	const std::vector<Triple> TRIPLES = {};

	Schema sch("feed_info.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, NO_WRITE_INSTRUCTIONS, rtc);
	return sch;
}

} // namespace schema
