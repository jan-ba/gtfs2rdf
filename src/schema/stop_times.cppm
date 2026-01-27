// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the GTFS2RDF project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.

module;

#include "transform_macros.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

export module schema:stop_times;
import :core;
import rdf_components;
import field_transforms;
import t_lib;
import runtime;

using namespace rdf;

namespace schema {

// this gtfs->rdf schema is preliminary and only covers a subset of all possible fields
export Schema buildStopTimesSchema(runtime::RuntimeContainer &rt) {

	const std::vector<std::string> possible_columns = {"trip_id",
	                                                   "arrival_time",
	                                                   "departure_time",
	                                                   "stop_id",
	                                                   "location_group_id",
	                                                   "location_id",
	                                                   "stop_sequence",
	                                                   "stop_headsign",
	                                                   "start_pickup_drop_off_window",
	                                                   "end_pickup_drop_off_window",
	                                                   "pickup_type",
	                                                   "drop_off_type",
	                                                   "continuous_pickup",
	                                                   "continuous_drop_off",
	                                                   "shape_dist_traveled",
	                                                   "timepoint",
	                                                   "pickup_booking_rule_id",
	                                                   "drop_off_booking_rule_id"};

	//
	const std::unordered_map<std::string, std::string> prefixes = {
	    {"stops", "https://gtfs.org/stops/"},
	    {"stop_times", "https://gtfs.org/stop_times/"},
	    {"trips", "https://gtfs.org/trips/"},
	    {"locationgroups", "https://gtfs.org/location_groups/"},
	    {"locations", "https://gtfs.org/locations/"},
	    {"booking", "https://gtfs.org/booking_rules/"},

	    {"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
	    {"xs", "http://www.w3.org/2001/XMLSchema#"},
	    {"gtfs", "https://w3id.org/gtfs2rdf#"},
	    {"wgs", "http://www.w3.org/2003/01/geo/wgs84_pos#"},
	    {"geo", "http://www.opengis.net/ont/geosparql#"},
	    {"gtfs2rdfgeom", "https://w3id.org/gtfs2rdf/geometry#"}};

	const IRI subj = IRI("stop_times", "{trip_id}_{stop_sequence}");

	const std::vector<Triple> triples = {
	    // SUBJECT PREDICATE                          OBJECT
	    // Identity & links
	    {subj, {"rdf", "type"}, {IRI("gtfs", "StopTime")}},
	    {subj, {"gtfs", "trip"}, {IRI("trips", "{trip_id}")}},

	    // One of these (mutually exclusive per GTFS)
	    {subj, {"gtfs", "stop"}, {IRI("stops", "{stop_id}")}},
	    {subj, {"gtfs", "locationGroup"}, {IRI("locationgroups", "{location_group_id}")}},
	    {subj, {"gtfs", "location"}, {IRI("locations", "{location_id}")}},

	    // Core fields
	    {subj, {"gtfs", "stopSequence"}, {"{stop_sequence}", IRI("xs", "integer")}},

	    // Times (converted to xs:time, i.e. capped at 24:00:00)
	    {subj, {"gtfs", "arrivalTime"}, {"{arrival_time | convert_time}", IRI("xs", "time")}},
	    {subj, {"gtfs", "departureTime"}, {"{departure_time | convert_time}", IRI("xs", "time")}},

	    // Optional headsign override
	    {subj, {"gtfs", "stopHeadsign"}, {"{stop_headsign}"}},

	    // On-demand windows
	    {subj,
	     {"gtfs", "startPickupDropOffWindow"},
	     {"{start_pickup_drop_off_window | convert_time}", IRI("xs", "time")}},
	    {subj,
	     {"gtfs", "endPickupDropOffWindow"},
	     {"{end_pickup_drop_off_window | convert_time}", IRI("xs", "time")}},

	    // Enums (as integers)
	    {subj, {"gtfs", "pickupType"}, {"{pickup_type}", IRI("xs", "integer")}},
	    {subj, {"gtfs", "dropOffType"}, {"{drop_off_type}", IRI("xs", "integer")}},
	    {subj, {"gtfs", "continuousPickup"}, {"{continuous_pickup}", IRI("xs", "integer")}},
	    {subj, {"gtfs", "continuousDropOff"}, {"{continuous_drop_off}", IRI("xs", "integer")}},

	    // Distance along shape
	    {subj, {"gtfs", "shapeDistTraveled"}, {"{shape_dist_traveled}", IRI("xs", "decimal")}},

	    // Exact vs. approximate
	    {subj, {"gtfs", "timepoint"}, {"{timepoint}", IRI("xs", "integer")}},

	    // Booking rules
	    {subj, {"gtfs", "pickupBookingRule"}, {IRI("booking", "{pickup_booking_rule_id}")}},
	    {subj, {"gtfs", "dropOffBookingRule"}, {IRI("booking", "{drop_off_booking_rule_id}")}}};

	Schema sc("stop_times.txt", possible_columns, prefixes, triples, rt);
	return sc;
}

} // namespace schema
