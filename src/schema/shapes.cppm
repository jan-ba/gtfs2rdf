// schema_shapes.cppm
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

export Schema buildShapesSchema(runtime::RuntimeContainer &rt) {

	const std::vector<std::string> possible_columns = {
	    "shape_id", "shape_pt_lat", "shape_pt_lon", "shape_pt_sequence", "shape_dist_traveled"};

	const std::unordered_map<std::string, std::string> prefixes = {
	    {"shapes", "https://gtfs.org/shapes/"},
	    {"geo", "http://www.opengis.net/ont/geosparql#"},
	};

	// no row-based triples; we only store
	const std::vector<Triple> triples = {};

	const std::vector<std::string> storage_only = {
	    // key = shape_id
	    // value_inputs = shape_pt_sequence, shape_pt_lon, shape_pt_lat
	    // transform output is stored as the multimap value
	    "{ shape_id : shape_pt_sequence, shape_pt_lon, shape_pt_lat > shapes@shapes.txt }"};

	return Schema("shapes.txt", possible_columns, prefixes, triples, storage_only, rt);
}

} // namespace schema
