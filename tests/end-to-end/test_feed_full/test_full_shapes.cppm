// SPDX-FileCopyrightText: 2026 Jan Babin
// SPDX-License-Identifier: AGPL-3.0-only
//
// Part of gtfs2rdf. See the LICENSE file for details.

module;

#include "../../../src/schema/transform_macros.h"

#include <string>
#include <unordered_map>
#include <vector>

export module test_full_shapes;

import schema;
import rdf_components;
import field_transforms;
import runtime;

using namespace rdf;
using namespace schema;

namespace test {

// _________________________________________________________________________________________________
// THIS SCHEMA WAS SOLELY BUILT FOR TESTING PURPOSES IN THE FULL FEED
// _________________________________________________________________________________________________

export Schema buildTestFullShapesSchema(runtime::RuntimeContainer& rtc) {
	const std::vector<std::string> POSSIBLE_COLUMNS = {
	    "shape_id", "shape_pt_lat", "shape_pt_lon", "shape_pt_sequence", "shape_dist_traveled"};

	const std::unordered_map<std::string, std::string> PREFIXES = {
	    {"shapes", "https://gtfs.org/shapes/"}, {"geo", "http://www.opengis.net/ont/geosparql#"}};

	const std::vector<Triple> TRIPLES = {};

	const std::vector<std::string> NO_WRITE_INSTRUCTIONS = {
	    "{ shape_id : shape_pt_sequence, shape_pt_lon, shape_pt_lat > shapes@test_full_shapes.txt "
	    "}"};

	return Schema(
	    "test_full_shapes.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, NO_WRITE_INSTRUCTIONS, rtc);
}

} // namespace test
