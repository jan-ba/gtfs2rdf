module;

#include "../../../src/schema/transform_macros.h"

#include <array>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

export module test_full_stops;

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

export Schema buildTestFullStopsSchema(runtime::RuntimeContainer& rtc) {
	// location_type (0..4) -> enum string
	TRANSFORM2ONE(test_loc2Enum, ARGS, OUT_VAL, STORAGE) {
		if (ARGS.size() != 1) {
			TRANSFORM_ERROR("Expected exactly one argument for location_type, got " +
			                std::to_string(ARGS.size()));
		}
		if (ARGS[0].empty()) {
			OUT_VAL = "";
			return;
		}
		char type_id = ARGS[0][0];
		if (type_id < '0' || type_id > '4') {
			TRANSFORM_ERROR("Unknown location_type code: " + std::string(ARGS[0]));
		}
		static constexpr std::array<std::string_view, 5> kNames = {
		    "stop", "station", "entrance_exit", "generic_node", "boarding_area"};
		OUT_VAL = kNames[type_id - '0'];
	}
	TRANSFORM_END

	const std::vector<std::string> POSSIBLE_COLUMNS = {
	    "stop_id",
	    "stop_name",
	    "stop_lat",
	    "stop_lon",
	    "location_type",
	    "stop_code",
	    "stop_url",
	};

	const std::unordered_map<std::string, std::string> PREFIXES = {
	    {"stops", "https://gtfs.org/stops/"},
	    {"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
	    {"xsd", "http://www.w3.org/2001/XMLSchema#"},
	    {"gtfs", "https://w3id.org/gtfs2rdf#"},
	    {"wgs", "http://www.w3.org/2003/01/geo/wgs84_pos#"},
	    {"geo", "http://www.opengis.net/ont/geosparql#"},
	    {"gtfs2rdfgeom", "https://w3id.org/gtfs2rdf/geometry#"},
	};

	const IRI SUBJ = IRI("stops", "{stop_id}");

	const std::vector<Triple> TRIPLES = {
	    {SUBJ, {"rdf", "type"}, {IRI("gtfs", "Stop")}},
	    {SUBJ, {"gtfs", "stopName"}, {"{stop_name}", "{FEED_LANG@test_full_feed_info.txt}"}},
	    {SUBJ, {"gtfs", "stopCode"}, {"{stop_code}"}},
	    {SUBJ, {"gtfs", "stopUrl"}, {"{stop_url}", IRI("xsd", "anyURI")}},
	    {SUBJ, {"wgs", "lat"}, {"{stop_lat, \"-90\", \"90\" | isInRange}", IRI("xsd", "decimal")}},
	    {SUBJ,
	     {"wgs", "long"},
	     {"{stop_lon, \"-180\", \"180\" | isInRange}", IRI("xsd", "decimal")}},
	    {SUBJ, {"geo", "hasGeometry"}, {IRI("gtfs2rdfgeom", "stop_{stop_id}")}},
	    {{"gtfs2rdfgeom", "stop_{stop_id}"},
	     {"geo", "asWKT"},
	     {"POINT({stop_lon} {stop_lat})", IRI("geo", "wktLiteral")}},
	    {SUBJ, {"gtfs", "locationTypeEnum"}, {"{location_type | test_loc2Enum}"}},
	};

	Schema sch("test_full_stops.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, rtc);
	return sch;
}

} // namespace test
