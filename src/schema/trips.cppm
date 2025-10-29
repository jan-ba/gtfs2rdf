module;

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <stdexcept>

export module schema.trips;
import schema.core;
import rdf_components;

using namespace rdf;

export namespace schema {

// GTFS -> RDF schema for trips.txt (covers common/optional fields)
Schema buildTripsSchema() {
  const std::vector<std::string> possible_columns = {
    "route_id", "service_id", "trip_id", "trip_headsign", "trip_short_name", "direction_id",
    "block_id", "shape_id", "wheelchair_accessible", "bikes_allowed", "cars_allowed"
  };

  const std::unordered_map<std::string, std::string> prefixes = {
    { "trips",    "https://gtfs.de/trips/" },
    { "routes",   "https://gtfs.de/routes/" },
    { "services", "https://gtfs.de/services/" },
    { "blocks",   "https://gtfs.de/blocks/" },
    { "shapes",   "https://gtfs.de/shapes/" },

    { "rdf",  "http://www.w3.org/1999/02/22-rdf-syntax-ns#" },
    { "xsd",  "http://www.w3.org/2001/XMLSchema#" },
    { "gtfs", "https://w3id.org/gtfs2rdf#" }
  };

  const std::vector<Triple> triples = {
    // SUBJECT                    PREDICATE         OBJECT
    // Identity
    { {"trips","{trip_id}"}, {"rdf","type"},   { IRI("gtfs","Trip") } },

    // Foreign keys
    { {"trips","{trip_id}"}, {"gtfs","route"}, { IRI("routes","{route_id}") } },
    { {"trips","{trip_id}"}, {"gtfs","service"},{ IRI("services","{service_id}") } },

    // Labels
    { {"trips","{trip_id}"}, {"gtfs","tripHeadsign"},  { "{trip_headsign}" } },
    { {"trips","{trip_id}"}, {"gtfs","tripShortName"}, { "{trip_short_name}" } },

    // Direction (0/1)
    { {"trips","{trip_id}"}, {"gtfs","directionId"},   { "{direction_id}", IRI("xsd","integer") } },

    // Block and shape
    { {"trips","{trip_id}"}, {"gtfs","block"}, { IRI("blocks","{block_id}") } },
    { {"trips","{trip_id}"}, {"gtfs","shape"}, { IRI("shapes","{shape_id}") } },

    // Accessibility / allowances (enums: 0/1/2)
    { {"trips","{trip_id}"}, {"gtfs","wheelchairAccessible"},
                                   { "{wheelchair_accessible}", IRI("xsd","integer") } },
    { {"trips","{trip_id}"}, {"gtfs","bikesAllowed"},
                                   { "{bikes_allowed}", IRI("xsd","integer") } },
    { {"trips","{trip_id}"}, {"gtfs","carsAllowed"},
                                   { "{cars_allowed}", IRI("xsd","integer") } }
  };

  Schema sc("trips", possible_columns, prefixes, triples);
  return sc;
}

} // namespace