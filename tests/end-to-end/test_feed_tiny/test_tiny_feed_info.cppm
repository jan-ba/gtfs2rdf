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

export module test_tiny_feed_info;

import schema;
import rdf_components;
import field_transforms;
import t_lib;
import runtime;

using namespace rdf;
using namespace schema;

namespace test {

// _________________________________________________________________________________________________
// THIS SCHEMA WAS SOLELY BUILT FOR TESTING PURPOSES IN THE TINY FEED
// _________________________________________________________________________________________________

export Schema buildTestTinyFeedInfoSchema(runtime::RuntimeContainer& rtc) {
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
	    {"feed", "https://gtfs.org/feed/"},
	    {"gtfs", "https://w3id.org/gtfs2rdf#"},
	    {"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
	    {"xs", "http://www.w3.org/2001/XMLSchema#"}};

	const std::vector<std::string> NO_WRITE_INSTRUCTIONS = {
	    "{ feed_lang > FEED_LANG }",
	    "{ feed_start_date | convertDate2xs_unchecked > FEED_START_DATE }",
	    "{ feed_end_date | convertDate2xs_unchecked > FEED_END_DATE }"};

	const std::vector<Triple> TRIPLES = {
	    // triples that give metainfo about the feed used
	    {{"feed", "1"}, {"rdf", "type"}, {IRI("gtfs", "Feed")}},
	    {{"feed", "1"}, {"gtfs", "feedPublisherName"}, {"{feed_publisher_name}"}},
	    {{"feed", "1"}, {"gtfs", "feedPublisherUrl"}, {"{feed_publisher_url}"}},
	    {{"feed", "1"}, {"gtfs", "feedLanguage"}, {"{feed_lang}"}},
	    {{"feed", "1"}, {"gtfs", "defaultLanguage"}, {"{default_lang}"}},
	    {{"feed", "1"},
	     {"gtfs", "feedStartDate"},
	     {"{feed_start_date | convertDate2xs_unchecked}", IRI("xs", "date")}},
	    {{"feed", "1"},
	     {"gtfs", "feedEndDate"},
	     {"{feed_end_date | convertDate2xs_unchecked}", IRI("xs", "date")}},
	    {{"feed", "1"}, {"gtfs", "feedVersion"}, {"{feed_version}"}},
	    {{"feed", "1"}, {"gtfs", "feedContactEmail"}, {"{feed_contact_email}"}},
	    {{"feed", "1"}, {"gtfs", "feedContactUrl"}, {"{feed_contact_url}"}}};

	Schema sch(
	    "test_tiny_feed_info.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, NO_WRITE_INSTRUCTIONS, rtc);
	return sch;
}

} // namespace test
