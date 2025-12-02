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

export module schema.stop_times;
import schema.core;
import rdf_components;
import field_transforms;
import t_lib;

using namespace rdf;

namespace schema {

// this gtfs->rdf schema is preliminary and only covers a subset of all possible fields
export Schema buildStopTimesSchema(field_transforms::TransformRegistry& registry) {

  const std::vector<std::string> possible_columns = {
    "trip_id", "arrival_time", "departure_time", "stop_id", "location_group_id", "location_id",
    "stop_sequence", "stop_headsign", "start_pickup_drop_off_window", "end_pickup_drop_off_window",
    "pickup_type", "drop_off_type", "continuous_pickup", "continuous_drop_off",
    "shape_dist_traveled", "timepoint", "pickup_booking_rule_id", "drop_off_booking_rule_id"
  };

  // 
  const std::unordered_map<std::string, std::string> prefixes = {
    { "stops",        "https://gtfs.de/stops/" },
    { "stoptimes",    "https://gtfs.de/stop_times/" },
    { "trips",        "https://gtfs.de/trips/" },
    { "locationgroups","https://gtfs.de/location_groups/" },
    { "locations",    "https://gtfs.de/locations/" },
    { "booking",      "https://gtfs.de/booking_rules/" },

    { "rdf",          "http://www.w3.org/1999/02/22-rdf-syntax-ns#" },
    { "xsd",          "http://www.w3.org/2001/XMLSchema#" },
    { "gtfs",         "https://w3id.org/gtfs2rdf#" },
    { "wgs",          "http://www.w3.org/2003/01/geo/wgs84_pos#" },
    { "geo",          "http://www.opengis.net/ont/geosparql#" },
    { "gtfs2rdfgeom", "https://w3id.org/gtfs2rdf/geometry#" }
  };

  const std::vector<Triple> triples = {
    // SUBJECT                                  PREDICATE          OBJECT
    // Identity & links
    { {"stoptimes","{trip_id}_{stop_sequence}"}, {"rdf","type"}, { IRI("gtfs","StopTime") } },
    { {"stoptimes","{trip_id}_{stop_sequence}"}, {"gtfs","trip"}, { IRI("trips","{trip_id}") } },

    // One of these (mutually exclusive per GTFS)
    { {"stoptimes","{trip_id}_{stop_sequence}"}, {"gtfs","stop"}, { IRI("stops","{stop_id}") } },
    { {"stoptimes","{trip_id}_{stop_sequence}"}, {"gtfs","locationGroup"},   
                                                { IRI("locationgroups","{location_group_id}") } },
    { {"stoptimes","{trip_id}_{stop_sequence}"}, {"gtfs","location"},        
                                                { IRI("locations","{location_id}") } },

    // Core fields
    { {"stoptimes","{trip_id}_{stop_sequence}"}, {"gtfs","stopSequence"},    
                                                    { "{stop_sequence}", IRI("xsd","integer") } },

    // Times (plain literals; GTFS allows >24:00:00)
    { {"stoptimes","{trip_id}_{stop_sequence}"}, {"gtfs","arrivalTime"},   { "{arrival_time}" } },
    { {"stoptimes","{trip_id}_{stop_sequence}"}, {"gtfs","departureTime"}, { "{departure_time}" } },

    // for testing function pumping (TODO: remove later)
    { {"stoptimes","{trip_id}_{stop_sequence}"}, {"gtfs","arrivalTimeSecs"}, 
                                                { "{arrival_time | time_to_seconds}", IRI("xsd","integer") } },
    { {"stoptimes","{trip_id}_{stop_sequence}"}, {"gtfs","departureTimeSecs"}, 
                                              { "{departure_time | time_to_seconds}", IRI("xsd","integer") } },


    // Optional headsign override
    { {"stoptimes","{trip_id}_{stop_sequence}"}, {"gtfs","stopHeadsign"},  { "{stop_headsign}" } },

    // On-demand windows (plain literals)
    { {"stoptimes","{trip_id}_{stop_sequence}"}, {"gtfs","startPickupDropOffWindow"}, 
                                                            { "{start_pickup_drop_off_window}" } },
    { {"stoptimes","{trip_id}_{stop_sequence}"}, {"gtfs","endPickupDropOffWindow"},   
                                                              { "{end_pickup_drop_off_window}" } },

    // Enums (as integers)
    { {"stoptimes","{trip_id}_{stop_sequence}"}, {"gtfs","pickupType"},      
                                                        { "{pickup_type}", IRI("xsd","integer") } },
    { {"stoptimes","{trip_id}_{stop_sequence}"}, {"gtfs","dropOffType"},     
                                                      { "{drop_off_type}", IRI("xsd","integer") } },
    { {"stoptimes","{trip_id}_{stop_sequence}"}, {"gtfs","continuousPickup"},
                                                  { "{continuous_pickup}", IRI("xsd","integer") } },
    { {"stoptimes","{trip_id}_{stop_sequence}"}, {"gtfs","continuousDropOff"},
                                                { "{continuous_drop_off}", IRI("xsd","integer") } },

    // Distance along shape
    { {"stoptimes","{trip_id}_{stop_sequence}"}, {"gtfs","shapeDistTraveled"},
                                                { "{shape_dist_traveled}", IRI("xsd","decimal") } },

    // Exact vs. approximate
    { {"stoptimes","{trip_id}_{stop_sequence}"}, {"gtfs","timepoint"},       
                                                          { "{timepoint}", IRI("xsd","integer") } },

    // Booking rules
    { {"stoptimes","{trip_id}_{stop_sequence}"}, {"gtfs","pickupBookingRule"},
                                                    { IRI("booking","{pickup_booking_rule_id}") } },
    { {"stoptimes","{trip_id}_{stop_sequence}"}, {"gtfs","dropOffBookingRule"},
                                                  { IRI("booking","{drop_off_booking_rule_id}") } }
  };


  Schema sc("stoptimes.txt", possible_columns, prefixes, triples, registry);
  return sc;
}

} // namespace
