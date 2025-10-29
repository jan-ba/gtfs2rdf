module;

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <stdexcept>

export module schema.calendar_dates;
import schema.core;
import rdf_components;

using namespace rdf;

export namespace schema {

// GTFS -> RDF schema for calendar_dates.txt
Schema buildCalendarDatesSchema() {
  const std::vector<std::string> possible_columns = {
    "service_id", "date", "exception_type"
  };

  const std::unordered_map<std::string, std::string> prefixes = {
    { "caldates", "https://gtfs.de/calendar_dates/" },
    { "services", "https://gtfs.de/services/" },

    { "rdf",  "http://www.w3.org/1999/02/22-rdf-syntax-ns#" },
    { "xsd",  "http://www.w3.org/2001/XMLSchema#" },
    { "gtfs", "https://w3id.org/gtfs2rdf#" }
  };

  // NOTE:
  // - We keep `date` as a plain literal because GTFS uses YYYYMMDD (no dashes); mapping to
  //   xsd:date would require transforming to YYYY-MM-DD during parsing.
  // TODO: add proper parsing of datatypes such as date
  const std::vector<Triple> triples = {
    // SUBJECT                               PREDICATE         OBJECT
    // Identity / type
    { {"caldates","{service_id}_{date}"},     {"rdf","type"},   { IRI("gtfs","CalendarDate") } },

    // Link to the service this exception refers to
    { {"caldates","{service_id}_{date}"},     {"gtfs","service"},
                                                { IRI("services","{service_id}") } },

    // The date of the exception (plain literal; see note above)
    { {"caldates","{service_id}_{date}"},     {"gtfs","date"},  { "{date}" } },

    // Exception type: 1 = added, 2 = removed
    { {"caldates","{service_id}_{date}"},     {"gtfs","exceptionType"},
                                                { "{exception_type}", IRI("xsd","integer") } }
  };

  Schema sc("caldates", possible_columns, prefixes, triples);
  return sc;
}

} // namespace
