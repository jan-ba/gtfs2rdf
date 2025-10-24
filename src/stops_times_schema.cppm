module;

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <stdexcept>

export module stop_times_schema;
import rdf_schema;
using namespace rdf_schema;

export namespace stop_times_schema {

// this gtfs->rdf schema is preliminary and only covers a subset of all possible fields
Schema buildStopTimesSchema() {

  const std::vector<std::string> possible_columns = {
      "trip_id",
      "arrival_time",
      "departure_time",
      "stop_id",
      "stop_sequence",
      "stop_headsign",
      "pickup_type",
      "drop_off_type",
      "shape_dist_traveled",
      "timepoint"
    // incomplete list
  };

  // 
  const std::unordered_map<std::string, std::string> prefixes = {
    {"base", "https://gtfs.de/öv/"},
    {"rdf",  "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
    {"xsd",  "http://www.w3.org/2001/XMLSchema#"},
    {"gtfs", "https://w3id.org/gtfs2rdf#"}
  };

  const std::vector<std::string> instructions = {
    "base:stop_times/{trip_id}/{stop_sequence} rdf:type gtfs:StopTime .",
    "base:stop_times/{trip_id}/{stop_sequence} gtfs:trip base:trips/{trip_id} .",
    "base:stop_times/{trip_id}/{stop_sequence} gtfs:stop base:stops/{stop_id} .",

    "base:stop_times/{trip_id}/{stop_sequence} gtfs:stopSequence {stop_sequence} .",
    "base:stop_times/{trip_id}/{stop_sequence} gtfs:arrivalTime {arrival_time} .",
    "base:stop_times/{trip_id}/{stop_sequence} gtfs:departureTime {departure_time} .",

    "base:stop_times/{trip_id}/{stop_sequence} gtfs:stopHeadsign {stop_headsign} .",
    "base:stop_times/{trip_id}/{stop_sequence} gtfs:pickupType {pickup_type} .",
    "base:stop_times/{trip_id}/{stop_sequence} gtfs:dropOffType {drop_off_type} .",
    "base:stop_times/{trip_id}/{stop_sequence} gtfs:shapeDistTraveled {shape_dist_traveled} .",
    "base:stop_times/{trip_id}/{stop_sequence} gtfs:timepoint {timepoint} ."
  };

  Schema sc("stop_times", possible_columns, prefixes, instructions);
  return sc;
}

} // namespace
