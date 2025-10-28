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
    { "stops",     "https://gtfs.de/stops/" },
    { "stoptimes", "https://gtfs.de/stoptimes/" },
    {"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
    {"xsd", "http://www.w3.org/2001/XMLSchema#"},
    {"gtfs",  "https://w3id.org/gtfs2rdf#"},
    {"wgs", "http://www.w3.org/2003/01/geo/wgs84_pos#"},
    {"geo", "http://www.opengis.net/ont/geosparql#"},
    {"gtfs2rdfgeom", "https://w3id.org/gtfs2rdf/geometry#"} };

  const std::vector<Triple> triples = {
    { IRI("stoptimes", "{trip_id}_{stop_sequence}"), IRI("rdf", "type"), Object(IRI("gtfs", "StopTime")) },
    { IRI("stoptimes", "{trip_id}_{stop_sequence}"), IRI("gtfs", "trip"), Object(IRI("stoptimes", "{trip_id}")) },
    { IRI("stoptimes", "{trip_id}_{stop_sequence}"), IRI("gtfs", "stop"), Object(IRI("stops", "{stop_id}")) },

    { IRI("stoptimes", "{trip_id}_{stop_sequence}"), IRI("gtfs", "stopSequence"), Object("{stop_sequence}") },
    { IRI("stoptimes", "{trip_id}_{stop_sequence}"), IRI("gtfs", "arrivalTime"), Object("{arrival_time}") },
    { IRI("stoptimes", "{trip_id}_{stop_sequence}"), IRI("gtfs", "departureTime"), Object("{departure_time}") },

    { IRI("stoptimes", "{trip_id}_{stop_sequence}"), IRI("gtfs", "stopHeadsign"), Object("{stop_headsign}") },
    { IRI("stoptimes", "{trip_id}_{stop_sequence}"), IRI("gtfs", "pickupType"), Object("{pickup_type}") },
    { IRI("stoptimes", "{trip_id}_{stop_sequence}"), IRI("gtfs", "dropOffType"), Object("{drop_off_type}") },
    { IRI("stoptimes", "{trip_id}_{stop_sequence}"), IRI("gtfs", "shapeDistTraveled"), Object("{shape_dist_traveled}") },
    { IRI("stoptimes", "{trip_id}_{stop_sequence}"), IRI("gtfs", "timepoint"), Object("{timepoint}") }
  };

  Schema sc("stoptimes", possible_columns, prefixes, triples);
  return sc;
}

} // namespace
