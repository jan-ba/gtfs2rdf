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

export module test_full_stop_times;

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

export Schema buildTestFullStopTimesSchema(runtime::RuntimeContainer& rtc) {
	const std::vector<std::string> POSSIBLE_COLUMNS = {
	    "trip_id", "arrival_time", "departure_time", "stop_id", "stop_sequence", "timepoint"};

	const std::unordered_map<std::string, std::string> PREFIXES = {
	    {"stops", "https://gtfs.org/stops/"},
	    {"stop_times", "https://gtfs.org/stop_times/"},
	    {"trips", "https://gtfs.org/trips/"},
	    {"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
	    {"xs", "http://www.w3.org/2001/XMLSchema#"},
	    {"gtfs", "https://w3id.org/gtfs2rdf#"}};

	const IRI SUBJ = IRI("stop_times", "{trip_id}_{stop_sequence}");

	const std::vector<Triple> TRIPLES = {
	    {SUBJ, {"rdf", "type"}, {IRI("gtfs", "StopTime")}},
	    {SUBJ, {"gtfs", "trip"}, {IRI("trips", "{trip_id}")}},
	    {SUBJ, {"gtfs", "stop"}, {IRI("stops", "{stop_id}")}},
	    {SUBJ, {"gtfs", "stopSequence"}, {"{stop_sequence}", IRI("xs", "integer")}},
	    {SUBJ,
	     {"gtfs", "arrivalTime"},
	     {"{arrival_time | convertTime2xs_unchecked}", IRI("xs", "time")}},
	    {SUBJ,
	     {"gtfs", "departureTime"},
	     {"{departure_time | convertTime2xs_unchecked}", IRI("xs", "time")}},
	    {SUBJ, {"gtfs", "timepoint"}, {"{timepoint}", IRI("xs", "integer")}}};
	Schema sch("test_full_stop_times.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, rtc);
	return sch;
}

} // namespace test
