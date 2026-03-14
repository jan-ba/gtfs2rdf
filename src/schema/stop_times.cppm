// SPDX-FileCopyrightText: 2026 Jan Babin
// SPDX-License-Identifier: AGPL-3.0-only
//
// Part of gtfs2rdf. See the LICENSE file for details.

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

// exemplary Gtfs -> Rdf schema for stop_times.txt
export Schema buildStopTimesSchema(runtime::RuntimeContainer& rtc) {
	const std::vector<std::string> POSSIBLE_COLUMNS = {"trip_id",
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

	const std::unordered_map<std::string, std::string> PREFIXES = {
	    {"stops", "https://gtfs.org/stops/"},
	    {"stop_times", "https://gtfs.org/stop_times/"},
	    {"trips", "https://gtfs.org/trips/"},
	    {"locationgroups", "https://gtfs.org/location_groups/"},
	    {"locations", "https://gtfs.org/locations/"},
	    {"booking", "https://gtfs.org/booking_rules/"},

	    {"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
	    {"xsd", "http://www.w3.org/2001/XMLSchema#"},
	    {"gtfs", "https://w3id.org/gtfs2rdf#"},
	    {"wgs", "http://www.w3.org/2003/01/geo/wgs84_pos#"},
	    {"geo", "http://www.opengis.net/ont/geosparql#"},
	    {"gtfs2rdfgeom", "https://w3id.org/gtfs2rdf/geometry#"}};

	const IRI SUBJ = IRI("stop_times", "{trip_id}_{stop_sequence}");

	const std::vector<Triple> TRIPLES = {
	    {SUBJ, {"rdf", "type"}, {IRI("gtfs", "StopTime")}},
	    {SUBJ, {"gtfs", "trip"}, {IRI("trips", "{trip_id}")}},

	    {SUBJ, {"gtfs", "stop"}, {IRI("stops", "{stop_id}")}},
	    {SUBJ, {"gtfs", "locationGroup"}, {IRI("locationgroups", "{location_group_id}")}},
	    {SUBJ, {"gtfs", "location"}, {IRI("locations", "{location_id}")}},

	    {SUBJ, {"gtfs", "stopSequence"}, {"{stop_sequence}", IRI("xsd", "integer")}},

	    // times (converted to xsd:time, i.e. hour mod 24)
	    {SUBJ,
	     {"gtfs", "arrivalTime"},
	     {"{arrival_time | convertTime2xs_unchecked}", IRI("xsd", "time")}},
	    {SUBJ,
	     {"gtfs", "departureTime"},
	     {"{departure_time | convertTime2xs_unchecked}", IRI("xsd", "time")}},

	    {SUBJ, {"gtfs", "stopHeadsign"}, {"{stop_headsign}"}},

	    {SUBJ,
	     {"gtfs", "startPickupDropOffWindow"},
	     {"{start_pickup_drop_off_window | convertTime2xs_unchecked}", IRI("xsd", "time")}},
	    {SUBJ,
	     {"gtfs", "endPickupDropOffWindow"},
	     {"{end_pickup_drop_off_window | convertTime2xs_unchecked}", IRI("xsd", "time")}},

	    // enums (as ints, could be resolved via transforms if desired)
	    {SUBJ, {"gtfs", "pickupType"}, {"{pickup_type}", IRI("xsd", "integer")}},
	    {SUBJ, {"gtfs", "dropOffType"}, {"{drop_off_type}", IRI("xsd", "integer")}},
	    {SUBJ, {"gtfs", "continuousPickup"}, {"{continuous_pickup}", IRI("xsd", "integer")}},
	    {SUBJ, {"gtfs", "continuousDropOff"}, {"{continuous_drop_off}", IRI("xsd", "integer")}},

	    // distance along shape
	    {SUBJ, {"gtfs", "shapeDistTraveled"}, {"{shape_dist_traveled}", IRI("xsd", "decimal")}},

	    {SUBJ, {"gtfs", "timepoint"}, {"{timepoint}", IRI("xsd", "integer")}},

	    {SUBJ, {"gtfs", "pickupBookingRule"}, {IRI("booking", "{pickup_booking_rule_id}")}},
	    {SUBJ, {"gtfs", "dropOffBookingRule"}, {IRI("booking", "{drop_off_booking_rule_id}")}}
	};

	Schema sch("stop_times.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, rtc);
	return sch;
}

} // namespace schema
