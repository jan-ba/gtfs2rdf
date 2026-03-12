module;

#include "../../../src/schema/transform_macros.h"

#include <algorithm>
#include <charconv>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

export module test_full_trips;

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

export Schema buildTestFullTripsSchema(runtime::RuntimeContainer& rtc) {
	TRANSFORM2ONE(get_linestring_per_trip, ARGS, OUT_VAL, STORAGE) {
		if (ARGS.size() != 1) {
			TRANSFORM_ERROR("get_linestring_per_trip expects 1 arg (shape_id), got " +
			                std::to_string(ARGS.size()));
		}
		auto shape_id = ARGS[0];
		if (shape_id.empty()) {
			TRANSFORM_ERROR("shape_id is empty");
		}

		auto tuples = STORAGE.getTuples("test_full_shapes.txt", "shapes", shape_id);
		if (tuples.empty()) {
			TRANSFORM_ERROR("No shape points found for shape_id '" + std::string(shape_id) + "'");
		}

		struct Row {
			size_t seq;
			std::string_view lon;
			std::string_view lat;
		};
		std::vector<Row> rows;
		rows.reserve(tuples.size());

		for (const auto& t : tuples) {
			if (t.size() < 3) {
				TRANSFORM_ERROR("Invalid stored shape tuple (expected at least 3 fields)");
			}
			size_t seq = 0;
			auto [ptr, ec] = std::from_chars(t[0].data(), t[0].data() + t[0].size(), seq);
			if (ec != std::errc{}) {
				TRANSFORM_ERROR("Invalid shape_pt_sequence '" + std::string(t[0]) +
				                "' for shape_id '" + std::string(shape_id) + "'");
			}
			rows.push_back(Row{seq, t[1], t[2]});
		}

		std::sort(
		    rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.seq < b.seq; });

		OUT_VAL.clear();
		OUT_VAL.append("LINESTRING(");
		for (const auto& r : rows) {
			if (OUT_VAL.back() != '(')
				OUT_VAL.append(", ");
			OUT_VAL.append(r.lon).append(" ").append(r.lat);
		}
		OUT_VAL.append(")");
	}
	TRANSFORM_END

	const std::vector<std::string> POSSIBLE_COLUMNS = {"route_id",
	                                                   "service_id",
	                                                   "trip_id",
	                                                   "trip_headsign",
	                                                   "direction_id",
	                                                   "block_id",
	                                                   "shape_id",
	                                                   "bikes_allowed"};

	const std::unordered_map<std::string, std::string> PREFIXES = {
	    {"trips", "https://gtfs.org/trips/"},
	    {"routes", "https://gtfs.org/routes/"},
	    {"services", "https://gtfs.org/services/"},
	    {"blocks", "https://gtfs.org/blocks/"},
	    {"geo", "http://www.opengis.net/ont/geosparql#"},
	    {"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
	    {"xsd", "http://www.w3.org/2001/XMLSchema#"},
	    {"gtfs2rdfgeom", "https://w3id.org/gtfs2rdf/geometry#"},
	    {"gtfs", "https://w3id.org/gtfs2rdf#"}};

	const IRI SUBJ = IRI("trips", "{trip_id}");

	const std::vector<Triple> TRIPLES = {
	    {SUBJ, {"rdf", "type"}, {IRI("gtfs", "Trip")}},
	    {SUBJ, {"gtfs", "route"}, {IRI("routes", "{route_id}")}},
	    {SUBJ, {"gtfs", "service"}, {IRI("services", "{service_id}")}},
	    {SUBJ,
	     {"gtfs", "tripHeadsign"},
	     {"{trip_headsign}", "{FEED_LANG@test_full_feed_info.txt}"}},
	    {SUBJ, {"gtfs", "directionId"}, {"{direction_id}", IRI("xsd", "integer")}},
	    {SUBJ, {"gtfs", "block"}, {IRI("blocks", "{block_id}")}},
	    {SUBJ, {"geo", "hasGeometry"}, {IRI("gtfs2rdfgeom", "tripshape_{trip_id}")}},
	    {IRI("gtfs2rdfgeom", "tripshape_{trip_id}"),
	     {"geo", "asWKT"},
	     {"{shape_id | get_linestring_per_trip@test_full_shapes.txt}", IRI("geo", "wktLiteral")}},
	    {SUBJ, {"gtfs", "bikesAllowed"}, {"{bikes_allowed}", IRI("xsd", "integer")}}};

	Schema sch("test_full_trips.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, rtc);
	return sch;
}

} // namespace test
