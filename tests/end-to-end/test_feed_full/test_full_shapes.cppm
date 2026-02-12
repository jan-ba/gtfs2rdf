// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the gtfs2rdf project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.

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
