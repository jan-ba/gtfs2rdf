module;

#include "transform_macros.h"

#include <algorithm>
#include <charconv>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

export module schema:trips;
import :core;
import rdf_components;
import field_transforms;
import t_lib;
import runtime;

using namespace rdf;

namespace schema {

// Gtfs -> Rdf schema for trips.txt (covers common/optional fields)
export Schema buildTripsSchema(runtime::RuntimeContainer& rtc) {
	// args: shape_id
	// output: WKT linestring of all shape points for this shape_id
	TRANSFORM2ONE(getLinestring, ARGS, OUT_VAL, STORAGE) {
		struct Row {
			size_t seq;
			std::string_view lon;
			std::string_view lat;
		};

		// early exit if linestring for this shape_id was already created
		if (STORAGE.containsValue("trips.txt", "created_linestrings", ARGS[0], "1")) {
			return;
		}

		// else create linestring and store that we created it
		STORAGE.storeValue("trips.txt", "created_linestrings", ARGS[0], "1");
		const auto& seq_lon_lat_vec = STORAGE.getTuples("shapes.txt", "shapes", ARGS[0]);
		if (seq_lon_lat_vec.empty()) {
			return;
		}

		// sort by sequence number to build correct linestrings
		std::vector<Row> rows(seq_lon_lat_vec.size());
		for (size_t i = 0; i < seq_lon_lat_vec.size(); ++i) {
			const auto& seq_lon_lat = seq_lon_lat_vec[i];
			std::from_chars(
			    seq_lon_lat[0].data(), seq_lon_lat[0].data() + seq_lon_lat[0].size(), rows[i].seq);
			rows[i].lon = seq_lon_lat[1];
			rows[i].lat = seq_lon_lat[2];
		}
		std::sort(rows.begin(), rows.end(), [](const Row& row_a, const Row& row_b) {
			return row_a.seq < row_b.seq;
		});

		// build WKT linestring
		OUT_VAL = "LINESTRING(";
		for (const auto& row : rows) {
			if (OUT_VAL.back() != '(') {
				OUT_VAL.append(", ");
			}
			OUT_VAL.append(row.lon).append(" ").append(row.lat);
		}
		OUT_VAL.append(")");
	}
	TRANSFORM_END

	const std::vector<std::string> POSSIBLE_COLUMNS = {"route_id",
	                                                   "service_id",
	                                                   "trip_id",
	                                                   "trip_headsign",
	                                                   "trip_short_name",
	                                                   "direction_id",
	                                                   "block_id",
	                                                   "shape_id",
	                                                   "wheelchair_accessible",
	                                                   "bikes_allowed",
	                                                   "cars_allowed"};

	const IRI SUBJ = IRI("trips", "{trip_id}");

	const std::unordered_map<std::string, std::string> PREFIXES = {
	    {"trips", "https://gtfs.org/trips/"},
	    {"routes", "https://gtfs.org/routes/"},
	    {"services", "https://gtfs.org/services/"},
	    {"blocks", "https://gtfs.org/blocks/"},
	    {"shapes", "https://gtfs.org/shapes/"},
	    {"geo", "http://www.opengis.net/ont/geosparql#"},
	    {"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
	    {"xsd", "http://www.w3.org/2001/XMLSchema#"},
	    {"gtfs2rdfgeom", "https://w3id.org/gtfs2rdf/geometry#"},
	    {"gtfs", "https://w3id.org/gtfs2rdf#"}};

	const std::vector<Triple> TRIPLES = {
	    // SUBJECT                    PREDICATE         OBJECT
	    // Identity
	    {SUBJ, {"rdf", "type"}, {IRI("gtfs", "Trip")}},

	    // Foreign keys
	    {SUBJ, {"gtfs", "route"}, {IRI("routes", "{route_id}")}},
	    {SUBJ, {"gtfs", "service"}, {IRI("services", "{service_id}")}},

	    // Labels
	    {SUBJ, {"gtfs", "tripHeadsign"}, {"{trip_headsign}"}},
	    {SUBJ, {"gtfs", "tripShortName"}, {"{trip_short_name}"}},

	    // Direction (0/1)
	    {SUBJ, {"gtfs", "directionId"}, {"{direction_id}", IRI("xsd", "integer")}},

	    // Block and shape
	    {SUBJ, {"gtfs", "block"}, {IRI("blocks", "{block_id}")}},
	    {SUBJ, {"geo", "hasGeometry"}, {IRI("gtfs2rdfgeom", "shapes_{shape_id}")}},
	    {IRI("gtfs2rdfgeom", "shapes_{shape_id}"),
	     {"geo", "asWKT"},
	     {{"{shape_id | getLinestring@shapes.txt}"}, IRI("geo", "wktLiteral")}},

	    // Accessibility / allowances (enums: 0/1/2)
	    {SUBJ,
	     {"gtfs", "wheelchairAccessible"},
	     {"{wheelchair_accessible}", IRI("xsd", "integer")}},
	    {SUBJ, {"gtfs", "bikesAllowed"}, {"{bikes_allowed}", IRI("xsd", "integer")}},
	    {SUBJ, {"gtfs", "carsAllowed"}, {"{cars_allowed}", IRI("xsd", "integer")}}};

	Schema sch("trips.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, rtc);
	return sch;
}

} // namespace schema