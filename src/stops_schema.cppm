module;

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <stdexcept>

export module stops_schema;
import rdf_schema;
using namespace rdf_schema;

// @prefix vagfr: <https://vag-freiburg.de/stops> .
// @prefix gtfs: <TODO> .
// @prefix geo: <http://www.opengis.net/ont/geosparql#> .
// @prefix gtfs2rdfgeom: <TODO> .
// vagfr:23423423434 rdf:type gtfs:stop .
// vagfr:23423423434 gtfs:stop_name "Freiburg Hbf"
// vagfr:23423423434 gtfs:stop_desc "Bla bla bla"
// vagfr:23423423434 geo:hasGeometry gtfs2rdfgeom:vagfr_23423423434 .
// gtfs2rdfgeom:vagfr_23423423434 geo:asWKT "POINT(9.5467076 47.1989504)"^^geo:wktLiteral .



export namespace stops_schema {

// this gtfs->rdf schema is preliminary and only covers a subset of all possible fields
Schema buildStopsSchema() {

  // possibly not required
  const std::vector<std::string> possible_columns = {
      "stop_id", "stop_code", "stop_name", "stop_desc", "stop_lat", "stop_lon",
      "zone_id", "stop_url", "location_type", "parent_station", "stop_timezone",
      "wheelchair_boarding", "level_id", "platform_code" };

  const std::unordered_map<std::string, std::string> prefixes = {
    { "stops",     "https://gtfs.de/stops/" },
    { "stoptimes", "https://gtfs.de/stop_times/" },
    {"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
    {"xsd", "http://www.w3.org/2001/XMLSchema#"},
    {"gtfs",  "https://w3id.org/gtfs2rdf#"},
    {"wgs", "http://www.w3.org/2003/01/geo/wgs84_pos#"},
    {"geo", "http://www.opengis.net/ont/geosparql#"},
    {"gtfs2rdfgeom", "https://w3id.org/gtfs2rdf/geometry#"} };

  const std::vector<Triple> triples = {
    { IRI("stops", "{stop_id}"), IRI("rdf", "type"), Object(IRI("gtfs", "Stop")) },
    { IRI("stops", "{stop_id}"), IRI("gtfs", "stopName"), Object("{stop_name}", "de") },
    { IRI("stops", "{stop_id}"), IRI("gtfs", "parentStation"), Object(IRI("stops", "{parent_station}")) },
    { IRI("stops", "{stop_id}"), IRI("wgs", "lat"), Object("{stop_lat}", IRI("xsd", "float")) },
    { IRI("stops", "{stop_id}"), IRI("wgs", "long"), Object("{stop_lon}", IRI("xsd", "float")) },
    { IRI("stops", "{stop_id}"), IRI("gtfs", "platformCode"), Object("{platform_code}") },
    { IRI("stops", "{stop_id}"), IRI("geo", "hasGeometry"), Object(IRI("gtfs2rdfgeom", "stop_{stop_id}")) },
    { IRI("gtfs2rdfgeom", "stop_{stop_id}"), IRI("geo", "asWKT"),
      Object("POINT({stop_lon} {stop_lat})", IRI("geo", "wktLiteral")) }
  };

  Schema sc("stops", possible_columns, prefixes, triples, true, true, true);

  return sc;
}

} // namespace