#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <string>
#include <vector>

import util;

TEST_CASE("TopologicalSort: empty graph returns empty result") {
	util::topological_sort::TopologicalSort sorter(0);
	auto result = sorter.sort();
	REQUIRE(result.empty());
}

TEST_CASE("TopologicalSort: self-loop not allowed throws and sets error node") {
	util::topological_sort::TopologicalSort sorter(1);
	REQUIRE_THROWS_WITH(sorter.addEdge(4, 4, false),
	                    Catch::Matchers::ContainsSubstring("self-loop"));
	REQUIRE(sorter.getErrorNode() == 4);
}

TEST_CASE("TopologicalSort: self-loop allowed does not throw and keeps node") {
	util::topological_sort::TopologicalSort sorter(1);
	sorter.addEdge(4, 4, true);
	auto result = sorter.sort();
	REQUIRE(result.size() == 1);
	REQUIRE(result[0] == 4);
}

TEST_CASE("TopologicalSort: respects dependencies and includes independent nodes") {
	auto getIndex = [](const std::vector<size_t>& vec, size_t value) {
		auto iter = std::find(vec.begin(), vec.end(), value);
		return iter != vec.end() ? std::distance(vec.begin(), iter) : -1;
	};

	util::topological_sort::TopologicalSort sorter(3); // estimate off by a lot to test resizing
	sorter.addEdge(10, 5, false);                      // 5 depends on 10
	sorter.addEdge(20, 5, false);                      // 5 depends on 20
	sorter.addNode(20);                                // already existent node, should be no-op
	sorter.addNode(100);                               // independent node
	sorter.addNode(100);
	sorter.addEdge(5, 5, true);    // self-loop allowed for 5
	sorter.addEdge(30, 10, false); // 10 depends on 30
	sorter.addEdge(40, 20, false); // 20 depends on 40

	auto result = sorter.sort();
	REQUIRE(result.size() == 6);
	REQUIRE(getIndex(result, 100) != -1); // independent node included
	// check that each node appears after its dependencies
	REQUIRE(getIndex(result, 10) < getIndex(result, 5));
	REQUIRE(getIndex(result, 20) < getIndex(result, 5));
	REQUIRE(getIndex(result, 30) < getIndex(result, 10));
	REQUIRE(getIndex(result, 40) < getIndex(result, 20));
}