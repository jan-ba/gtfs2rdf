#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <string>
#include <string_view>

import schema_parser;
import field_transforms;

namespace {
const field_transforms::TransformRegistry& testRegistry() {
	static field_transforms::TransformRegistry registry;
	registry.registerTransform("transform2One",
	                           [](field_transforms::Args args, field_transforms::Out1& out) {
		                           out = "one:" + std::string(args[0]);
	                           });
	registry.registerTransform("transform2Many",
	                           [](field_transforms::Args args, field_transforms::OutN& out) {
		                           for (const auto& arg : args) {
			                           out.push_back("many:" + std::string(arg));
		                           }
	                           });
	return registry;
}
} // namespace

TEST_CASE("parsePlaceholder parses all basic arg kinds correctly") {
	const auto reg = testRegistry();
	const auto spec_col = schema::parsePlaceholder("column", reg);
	const auto spec_lit = schema::parsePlaceholder("\"literal\"", reg);
	const auto spec_sto = schema::parsePlaceholder("STORAGE_VAR@file.txt", reg);
	REQUIRE((spec_col.args.size() == 1 && spec_lit.args.size() == 1 && spec_sto.args.size() == 1));

	// each arg is asigned their correct kind, name and context (if applicable)
	REQUIRE((spec_col.args[0].kind == schema::ArgKind::COLUMN &&
	         spec_lit.args[0].kind == schema::ArgKind::LITERAL &&
	         spec_sto.args[0].kind == schema::ArgKind::STORAGE_VAR));
	REQUIRE((spec_col.args[0].ctx.empty() && spec_lit.args[0].ctx.empty() &&
	         spec_sto.args[0].ctx == "file.txt"));

	// each arg is assigned the correct name (column name, literal text, variable name)
	REQUIRE((spec_col.args[0].name == "column" && spec_lit.args[0].name == "literal" &&
	         spec_sto.args[0].name == "STORAGE_VAR"));

	// we did not specify any transforms or storage operations, so those should be empty/none
	REQUIRE((spec_col.transforms.empty() && spec_lit.transforms.empty() &&
	         spec_sto.transforms.empty()));
	REQUIRE((spec_col.storage.kind == schema::StorageKind::NONE &&
	         spec_lit.storage.kind == schema::StorageKind::NONE &&
	         spec_sto.storage.kind == schema::StorageKind::NONE));
	REQUIRE((spec_col.storage.mode == schema::StoreMode::NONE &&
	         spec_lit.storage.mode == schema::StoreMode::NONE &&
	         spec_sto.storage.mode == schema::StoreMode::NONE));
}

TEST_CASE("parsePlaceholder throws on empty argument field / empty transform field / empty storage "
          "target") {
	const auto reg = testRegistry();
	REQUIRE_THROWS_WITH(
	    schema::parsePlaceholder("", reg),
	    Catch::Matchers::ContainsSubstring("Placeholder must have at least 1 argument"));
	REQUIRE_THROWS_WITH(
	    schema::parsePlaceholder("column, \"literal\", STORAGE_VAR@file.txt | ", reg),
	    Catch::Matchers::ContainsSubstring("transform"));
	REQUIRE_THROWS_WITH(schema::parsePlaceholder("column > ", reg),
	                    Catch::Matchers::ContainsSubstring("Empty storage target after '>'"));
	REQUIRE_THROWS_WITH(schema::parsePlaceholder("column | transform2One > ", reg),
	                    Catch::Matchers::ContainsSubstring("Empty storage target after '>'"));
	REQUIRE_THROWS_WITH(schema::parsePlaceholder("column | > STORAGE_VAR", reg),
	                    Catch::Matchers::ContainsSubstring("transform"));
}

TEST_CASE("parsePlaceholder throws on non-keyed multi-arg without transforms") {
	const auto reg = testRegistry();
	REQUIRE_THROWS_WITH(schema::parsePlaceholder("arg1, arg2", reg),
	                    Catch::Matchers::ContainsSubstring("requires transform"));
	REQUIRE_THROWS_WITH(schema::parsePlaceholder("arg1, arg2 > STORAGE_VAR", reg),
	                    Catch::Matchers::ContainsSubstring("requires transform"));
}

TEST_CASE("parsePlaceholder parses variable store instructions correctly") {
	const auto reg = testRegistry();
	const auto spec = schema::parsePlaceholder("arg1 > STORAGE_VAR", reg);
	REQUIRE(spec.args.size() == 1);
	REQUIRE(spec.transforms.size() == 0);
	REQUIRE(spec.storage.kind == schema::StorageKind::VARIABLE);
	REQUIRE(spec.storage.mode == schema::StoreMode::STORE_COMPUTED);
	REQUIRE(spec.storage.target_ctx.empty()); // no explicit ctx given, filled later

	const auto spec2 =
	    schema::parsePlaceholder("arg1, arg2 | transform2One > STORAGE_VAR@ctx.txt", reg);
	REQUIRE(spec2.args.size() == 2);
	REQUIRE(spec2.transforms.size() == 1);
	REQUIRE(spec2.transforms[0].transform.name == "transform2One");
	REQUIRE(spec2.storage.kind == schema::StorageKind::VARIABLE);
	REQUIRE(spec2.storage.mode == schema::StoreMode::STORE_COMPUTED);
	REQUIRE(spec2.storage.target_ctx == "ctx.txt"); // explicit ctx given
}

TEST_CASE("parsePlaceholder parses chained transforms correctly, throwing on invalid chaining") {
	const auto reg = testRegistry();
	const auto spec = schema::parsePlaceholder(
	    "arg1 | transform2One | transform2Many@ctx.txt | transform2One", reg);
	REQUIRE(spec.args.size() == 1);
	REQUIRE(spec.transforms.size() == 3);
	REQUIRE(spec.transforms[0].transform.name == "transform2One");
	REQUIRE(spec.transforms[1].transform.name == "transform2Many");
	REQUIRE(spec.transforms[1].ctx_hint == "ctx.txt");
	REQUIRE(spec.transforms[2].transform.name == "transform2One");
	REQUIRE(spec.storage.kind == schema::StorageKind::NONE);
	REQUIRE(spec.storage.mode == schema::StoreMode::NONE);

	REQUIRE_THROWS_WITH(schema::parsePlaceholder("arg1 | transform2Many | transform2Many", reg),
	                    Catch::Matchers::ContainsSubstring("Transform2Many"));
}

TEST_CASE("parsePlaceholder: keyed raw storage selects MULTI_MAP vs TUPLE_MAP and sets arities "
          "correctly") {
	const auto reg = testRegistry();

	// two keys, one value -> store into MULTI_MAP with key_arity=2, value_arity=1
	const auto spec_multi = schema::parsePlaceholder("key1, key2 : val1 > STORAGE_VAR", reg);
	REQUIRE(spec_multi.args.size() == 3);
	REQUIRE(spec_multi.storage.kind == schema::StorageKind::MULTI_MAP);
	REQUIRE(spec_multi.storage.mode == schema::StoreMode::STORE_RAW);
	REQUIRE(spec_multi.storage.key_arity == 2);
	REQUIRE(spec_multi.storage.value_arity == 1);
	REQUIRE(spec_multi.storage.extra_arity == 0);

	// two keys, two values -> store into TUPLE_MAP with key_arity=2, value_arity=2
	const auto spec_tuple = schema::parsePlaceholder("key1, key2 : val1, val2 > STORAGE_VAR", reg);
	REQUIRE(spec_tuple.args.size() == 4);
	REQUIRE(spec_tuple.storage.kind == schema::StorageKind::TUPLE_MAP);
	REQUIRE(spec_tuple.storage.mode == schema::StoreMode::STORE_RAW);
	REQUIRE(spec_tuple.storage.key_arity == 2);
	REQUIRE(spec_tuple.storage.value_arity == 2);
	REQUIRE(spec_tuple.storage.extra_arity == 0);

	const auto spec_shapes = schema::parsePlaceholder(
	    "shape_id : shape_pt_sequence, shape_pt_lat, shape_pt_lon > shapes@shapes.txt", reg);
	REQUIRE(spec_shapes.args.size() == 4);
	REQUIRE(spec_shapes.storage.kind == schema::StorageKind::TUPLE_MAP);
	REQUIRE(spec_shapes.storage.mode == schema::StoreMode::STORE_RAW);
	REQUIRE(spec_shapes.storage.key_arity == 1);
	REQUIRE(spec_shapes.storage.value_arity == 3);
	REQUIRE(spec_shapes.storage.extra_arity == 0);
}

TEST_CASE("parsePlaceholder: keyed computed storage selects MULTI_MAP vs TUPLE_MAP correctly") {
	const auto reg = testRegistry();

	// Transform2One output is a single string -> store into MULTI_MAP
	const auto spec_multi =
	    schema::parsePlaceholder("key1, key2 : val1, val2 | transform2One > mm@ctx.txt", reg);
	REQUIRE(spec_multi.args.size() == 4);
	REQUIRE(spec_multi.transforms.size() == 1);
	REQUIRE(spec_multi.transforms[0].transform.name == "transform2One");
	REQUIRE(spec_multi.storage.kind == schema::StorageKind::MULTI_MAP);
	REQUIRE(spec_multi.storage.mode == schema::StoreMode::STORE_COMPUTED);

	// Transform2Many output is a vector of strings -> store into TUPLE_MAP
	const auto spec_tuple =
	    schema::parsePlaceholder("key1, key2 : val1, val2 | transform2Many > tm@ctx.txt", reg);
	REQUIRE(spec_tuple.args.size() == 4);
	REQUIRE(spec_tuple.transforms.size() == 1);
	REQUIRE(spec_tuple.transforms[0].transform.name == "transform2Many");
	REQUIRE(spec_tuple.storage.kind == schema::StorageKind::TUPLE_MAP);
	REQUIRE(spec_tuple.storage.mode == schema::StoreMode::STORE_COMPUTED);
}

TEST_CASE("parsePlaceholder: filter-mode with '(...)' sets FILTER_STORE_RAW, requires transforms "
          "(acting as filter) and throws on Transform2Many") {
	const auto reg = testRegistry();

	const auto spec = schema::parsePlaceholder(
	    "key1, key2 : (val1, val2), val3| transform2One > mm@ctx.txt", reg);
	REQUIRE(spec.args.size() == 5);
	REQUIRE(spec.transforms.size() == 1);
	REQUIRE(spec.storage.key_arity == 2);
	REQUIRE(spec.storage.value_arity == 2);
	REQUIRE(spec.storage.extra_arity == 1);
	REQUIRE(spec.transforms[0].transform.name == "transform2One");
	REQUIRE(spec.storage.kind == schema::StorageKind::TUPLE_MAP);
	REQUIRE(spec.storage.mode == schema::StoreMode::FILTER_STORE_RAW);

	REQUIRE_THROWS_WITH(
	    schema::parsePlaceholder("key1, key2 : (val1, val2) | transform2Many > tm@ctx.txt", reg),
	    Catch::Matchers::ContainsSubstring("Transform2Many"));
}

TEST_CASE("parsePlaceholder throws on downright wrong order of components") {
	const auto reg = testRegistry();
	REQUIRE_THROWS(schema::parsePlaceholder("| transform2One > STORAGE_VAR", reg));
	REQUIRE_THROWS(schema::parsePlaceholder("> STORAGE_VAR | transform2One", reg));
	REQUIRE_THROWS(schema::parsePlaceholder("arg1 > STORAGE_VAR | transform2One", reg));
	REQUIRE_THROWS(schema::parsePlaceholder("arg1 | > STORAGE_VAR | transform2One", reg));
	REQUIRE_THROWS(
	    schema::parsePlaceholder("arg1 | transform2One > STORAGE_VAR | transform2One", reg));
}