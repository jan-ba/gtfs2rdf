// SPDX-FileCopyrightText: 2026 Jan Babin
// SPDX-License-Identifier: AGPL-3.0-only
//
// Part of gtfs2rdf. See the LICENSE file for details.

module;

#include "transform_macros.h"

#include <algorithm>
#include <string>
#include <vector>

export module schema:shapes;
import :core;
import rdf_components;
import field_transforms;
import runtime;

using namespace rdf;

namespace schema {

export Schema buildShapesSchema(runtime::RuntimeContainer& rtc) {
	const std::vector<std::string> POSSIBLE_COLUMNS = {
	    "shape_id", "shape_pt_lat", "shape_pt_lon", "shape_pt_sequence", "shape_dist_traveled"};

	const std::unordered_map<std::string, std::string> PREFIXES = {
	    {"shapes", "https://gtfs.org/shapes/"},
	    {"geo", "http://www.opengis.net/ont/geosparql#"},
	};

	// no row-based triples; we only store
	const std::vector<Triple> TRIPLES = {};

	const std::vector<std::string> NO_WRITE_INSTRUCTIONS = {
	    // key = shape_id
	    // value_inputs = shape_pt_sequence, shape_pt_lon, shape_pt_lat
	    // transform output is stored as the multimap value
	    "{ shape_id : shape_pt_sequence, shape_pt_lon, shape_pt_lat > shapes@shapes.txt }"};

	return Schema("shapes.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, NO_WRITE_INSTRUCTIONS, rtc);
}

} // namespace schema
