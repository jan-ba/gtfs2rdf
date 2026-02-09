// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
//
// This file is part of the gtfs2rdf project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

import util;

using namespace util::strings;

TEST_CASE("util::strings::parseYYYYMMDD") {
	auto date = parseYYYYMMDD("20240630");
	CHECK(date == std::chrono::sys_days{std::chrono::year{2024} / std::chrono::month{6} /
	                                    std::chrono::day{30}});

	auto date2 = parseYYYYMMDD("19991231");
	CHECK(date2 == std::chrono::sys_days{std::chrono::year{1999} / std::chrono::month{12} /
	                                     std::chrono::day{31}});
}

TEST_CASE("util::strings::replaceAll") {
	std::string result = replaceAll("hello world, hello everyone", "hello", "hi");
	CHECK(result == "hi world, hi everyone");

	std::string result2 = replaceAll("abcde", "x", "y");
	CHECK(result2 == "abcde");

	std::string result3 = replaceAll("aaaaa", "aa", "b");
	CHECK(result3 == "bba");

	std::string result4 = replaceAll("", "a", "b");
	CHECK(result4 == "");
}

TEST_CASE("util::strings::split") {
	std::vector<std::string_view> result = split("a,b,c", ',');
	CHECK(result == std::vector<std::string_view>{"a", "b", "c"});

	std::vector<std::string_view> result2 = split("one;two;three", ';');
	CHECK(result2 == std::vector<std::string_view>{"one", "two", "three"});

	std::vector<std::string_view> result3 = split("no_delimiter", ',');
	CHECK(result3 == std::vector<std::string_view>{"no_delimiter"});

	std::vector<std::string_view> result4 = split("", ',');
	CHECK((result4.size() == 1 && result4[0].empty()));
}

TEST_CASE("util::strings::removeWSOutsideQuotes") {
	std::string result = removeWSOutsideQuotes("  hello   \"  world  \"  ");
	CHECK(result == "hello\"  world  \"");

	std::string result2 = removeWSOutsideQuotes("   \"   spaced   \"   ");
	CHECK(result2 == "\"   spaced   \"");

	std::string result3 = removeWSOutsideQuotes("noquotes");
	CHECK(result3 == "noquotes");

	std::string result4 = removeWSOutsideQuotes("");
	CHECK(result4 == "");
}

TEST_CASE("util::strings::findAtTopLevel") {
	std::string_view svw = "a,(b,c),d";
	size_t pos1 = findAtTopLevel(svw, ',', 0);
	CHECK(pos1 == 1);
	size_t pos2 = findAtTopLevel(svw, ',', pos1 + 1);
	CHECK(pos2 == 7);
	size_t pos3 = findAtTopLevel(svw, ',', pos2 + 1);
	CHECK(pos3 == std::string_view::npos);
}

TEST_CASE("util::strings::splitAtTopLevel") {
	std::string_view svw = "a,(b,c),d";
	std::vector<std::string_view> result = splitAtTopLevel(svw, ',');
	CHECK(result == std::vector<std::string_view>{"a", "(b,c)", "d"});

	std::string_view svw2 = "one;two;(three;four);five";
	std::vector<std::string_view> result2 = splitAtTopLevel(svw2, ';');
	CHECK(result2 == std::vector<std::string_view>{"one", "two", "(three;four)", "five"});

	std::string_view svw3 = "no_delimiter";
	std::vector<std::string_view> result3 = splitAtTopLevel(svw3, ',');
	CHECK(result3 == std::vector<std::string_view>{"no_delimiter"});

	std::string_view svw4 = "";
	std::vector<std::string_view> result4 = splitAtTopLevel(svw4, ',');
	CHECK(result4.empty());
}

TEST_CASE("util::strings::splitOnceAtTopLevel") {
	std::string_view svw = "a,(b,c),d";
	auto result = splitOnceAtTopLevel(svw, ',');
	CHECK(result.first == "a");
	CHECK(result.second == "(b,c),d");

	std::string_view svw2 = "one;two;(three;four);five";
	auto result2 = splitOnceAtTopLevel(svw2, ';');
	CHECK(result2.first == "one");
	CHECK(result2.second == "two;(three;four);five");

	std::string_view svw3 = "no_delimiter";
	auto result3 = splitOnceAtTopLevel(svw3, ',');
	CHECK(result3.first == "no_delimiter");
	CHECK(result3.second.empty());

	std::string_view svw4 = "";
	auto result4 = splitOnceAtTopLevel(svw4, ',');
	CHECK(result4.first.empty());
	CHECK(result4.second.empty());
}

TEST_CASE("util::strings::splitOnceAtIndex") {
	std::string_view svw = "a,b,c";
	auto result = splitOnceAtIndex(svw, 1);
	CHECK(result.first == "a");
	CHECK(result.second == "b,c");

	std::string_view svw2 = "one;two;three";
	auto result2 = splitOnceAtIndex(svw2, 3);
	CHECK(result2.first == "one");
	CHECK(result2.second == "two;three");

	std::string_view svw3 = "no_delimiter";
	auto result3 = splitOnceAtIndex(svw3, 20);
	CHECK(result3.first == "no_delimiter");
	CHECK(result3.second.empty());

	std::string_view svw4 = "    "; // note: 4 spaces
	auto result4 = splitOnceAtIndex(svw4, 0);
	CHECK(result4.first.empty());
	CHECK(result4.second == "   "); // note: 3 spaces
}

TEST_CASE("util::strings::unquote") {
	std::string result = unquote("\"hello \\\"world\\\"\"");
	CHECK(result == "hello \"world\"");

	std::string result2 = unquote("noquotes");
	CHECK(result2 == "noquotes");

	std::string result3 = unquote("\"escaped \\\\ backslash\"");
	CHECK(result3 == "escaped \\ backslash");

	std::string result4 = unquote("");
	CHECK(result4 == "");
}

TEST_CASE("util::strings::enclose") {
	std::string result = enclose("hello", '"', '"');
	CHECK(result == "\"hello\"");

	std::string result2 = enclose("world", '(', ')');
	CHECK(result2 == "(world)");

	std::string result3 = enclose("", '[', ']');
	CHECK(result3 == "[]");
}

TEST_CASE("util::strings::splitAt") {
	std::string_view svw = "\"key:value\"";
	auto result = splitAt(svw, ':');
	CHECK(result.first == "\"key");
	CHECK(result.second == "value\"");
	std::string_view svw2 = "no_delimiter";
	auto result2 = splitAt(svw2, ':');
	CHECK(result2.first == "no_delimiter");
	CHECK(result2.second.empty());
}

TEST_CASE("util::strings::formatValueWithPaddedUnits") {
	std::string result = formatValueWithPaddedUnits(1500, UnitType::COUNT);
	CHECK(result == "1.5k");

	std::string result2 = formatValueWithPaddedUnits(2048, UnitType::SIZE);
	CHECK(result2 == "2.0kB");

	std::string result3 = formatValueWithPaddedUnits(3600000000000, UnitType::TIME);
	CHECK(result3 == "1.0h");

	std::string result4 = formatValueWithPaddedUnits(999, UnitType::COUNT);
	CHECK(result4 == "999");

	std::string result5 = formatValueWithPaddedUnits(999999, UnitType::SIZE);
	CHECK(result5 == "1.0MB");
}

TEST_CASE("util::strings::isValidCTXName") {
	CHECK(isValidCTXName("valid_name.txt"));
	CHECK(isValidCTXName("another_valid_name.txt"));
	CHECK(!isValidCTXName("invalid-name.txt"));
	CHECK(!isValidCTXName("invalid name.txt"));
	CHECK(!isValidCTXName("invalidname.doc"));
}