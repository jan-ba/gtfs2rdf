// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the GTFS2RDF project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.

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

// GTFS -> RDF schema for trips.txt (covers common/optional fields)
export Schema buildTripsSchema(runtime::RuntimeContainer& rt) {
	// args: shape_id
	// output: WKT linestring of all shape points for this shape_id
	TRANSFORM2ONE(get_linestring, ARGS, OUT_VAL, STORAGE) {
		struct Row {
			size_t seq;
			std::string_view lon;
			std::string_view lat;
		};

		// early exit if linestring for this shape_id was already created
		if (STORAGE.containsValue("trips.txt", "created_linestrings", ARGS[0], "1"))
			return;

		// else create linestring and store that we created it
		STORAGE.storeValue("trips.txt", "created_linestrings", ARGS[0], "1");
		const auto& seq_lon_lat_vec = STORAGE.getTuples("shapes.txt", "shapes", ARGS[0]);
		if (seq_lon_lat_vec.empty())
			return;

		// sort by sequence number to build correct linestrings
		std::vector<Row> rows(seq_lon_lat_vec.size());
		for (size_t i = 0; i < seq_lon_lat_vec.size(); ++i) {
			const auto& seq_lon_lat = seq_lon_lat_vec[i];
			std::from_chars(
			    seq_lon_lat[0].data(), seq_lon_lat[0].data() + seq_lon_lat[0].size(), rows[i].seq);
			rows[i].lon = seq_lon_lat[1];
			rows[i].lat = seq_lon_lat[2];
		}
		std::sort(
		    rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.seq < b.seq; });

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

	const std::vector<std::string> possible_columns = {"route_id",
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

	const IRI subj = IRI("trips", "{trip_id}");

	const std::unordered_map<std::string, std::string> prefixes = {
	    {"trips", "https://gtfs.org/trips/"},
	    {"routes", "https://gtfs.org/routes/"},
	    {"services", "https://gtfs.org/services/"},
	    {"blocks", "https://gtfs.org/blocks/"},
	    {"shapes", "https://gtfs.org/shapes/"},
	    {"geo", "http://www.opengis.net/ont/geosparql#"},
	    {"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
	    {"xs", "http://www.w3.org/2001/XMLSchema#"},
	    {"gtfs2rdfgeom", "https://w3id.org/gtfs2rdf/geometry#"},
	    {"gtfs", "https://w3id.org/gtfs2rdf#"}};

	const std::vector<Triple> triples = {
	    // SUBJECT                    PREDICATE         OBJECT
	    // Identity
	    {subj, {"rdf", "type"}, {IRI("gtfs", "Trip")}},

	    // Foreign keys
	    {subj, {"gtfs", "route"}, {IRI("routes", "{route_id}")}},
	    {subj, {"gtfs", "service"}, {IRI("services", "{service_id}")}},

	    // Labels
	    {subj, {"gtfs", "tripHeadsign"}, {"{trip_headsign}"}},
	    {subj, {"gtfs", "tripShortName"}, {"{trip_short_name}"}},

	    // Direction (0/1)
	    {subj, {"gtfs", "directionId"}, {"{direction_id}", IRI("xs", "integer")}},

	    // Block and shape
	    {subj, {"gtfs", "block"}, {IRI("blocks", "{block_id}")}},
	    // {subj, {"gtfs", "shape"}, {IRI("shapes", "{shape_id}")}},
	    {subj, {"geo", "hasGeometry"}, {IRI("gtfs2rdfgeom", "shapes_{shape_id}")}},
	    {IRI("gtfs2rdfgeom", "shapes_{shape_id}"),
	     {"geo", "asWKT"},
	     {{"{shape_id | get_linestring@shapes.txt}"}, IRI("geo", "wktLiteral")}},

	    // Accessibility / allowances (enums: 0/1/2)
	    {subj, {"gtfs", "wheelchairAccessible"}, {"{wheelchair_accessible}", IRI("xs", "integer")}},
	    {subj, {"gtfs", "bikesAllowed"}, {"{bikes_allowed}", IRI("xs", "integer")}},
	    {subj, {"gtfs", "carsAllowed"}, {"{cars_allowed}", IRI("xs", "integer")}}};

	Schema sc("trips.txt", possible_columns, prefixes, triples, rt);
	return sc;
}

} // namespace schema