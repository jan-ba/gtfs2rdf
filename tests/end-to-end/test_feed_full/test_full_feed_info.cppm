// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the gtfs2rdf project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.

module;

#include "../../../src/schema/transform_macros.h"

#include <string>
#include <unordered_map>
#include <vector>

export module test_full_feed_info;

import schema;
import rdf_components;
import field_transforms;
import t_lib;
import runtime;

using namespace rdf;
using namespace schema;

namespace test {

// _________________________________________________________________________________________________
// THIS SCHEMA WAS SOLELY BUILT FOR TESTING PURPOSES IN THE FULL FEED
// _________________________________________________________________________________________________

export Schema buildTestFullFeedInfoSchema(runtime::RuntimeContainer& rtc) {
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
	Schema sch(
	    "test_full_feed_info.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, NO_WRITE_INSTRUCTIONS, rtc);
	return sch;
}

} // namespace test
