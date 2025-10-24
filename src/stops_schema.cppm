module;

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <stdexcept>

export module stops_schema;
import rdf_schema;
using namespace rdf_schema;

export namespace stops_schema {

// this gtfs->rdf schema is preliminary and only covers a subset of all possible fields
Schema buildStopsSchema() {

  const std::vector<std::string> possible_columns = {
      "stop_id", "stop_code", "stop_name", "stop_desc", "stop_lat", "stop_lon",
      "zone_id", "stop_url", "location_type", "parent_station", "stop_timezone",
      "wheelchair_boarding", "level_id", "platform_code" };

  const std::unordered_map<std::string, std::string> prefixes = {
    {"base", "https://gtfs.de/öv/stops"},
    {"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
    {"xsd", "http://www.w3.org/2001/XMLSchema#"},
    {"gtfs",  "https://w3id.org/gtfs2rdf#"},
    {"wgs", "http://www.w3.org/2003/01/geo/wgs84_pos#"},
    {"geo", "http://www.opengis.net/ont/geosparql#"} };

  const std::vector<std::string> instructions = {
    "base:stops/{stop_id} rdf:type gtfs:Stop .",
    "base:stops/{stop_id} gtfs:stopName {stop_name} .",
    "base:stops/{stop_id} gtfs:parentStation base:stops/{parent_station} .",
    "base:stops/{stop_id} wgs:lat {stop_lat} .",
    "base:stops/{stop_id} wgs:long {stop_lon} .",
    "base:stops/{stop_id} gtfs:platformCode {platform_code} ."
  };

  Schema sc("stops", possible_columns, prefixes, instructions);

  return sc;
}

} // namespace