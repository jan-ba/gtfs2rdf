// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
//
// This file is part of the gtfs2rdf project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <string_view>

import rdf_components;

TEST_CASE("rdf: percentEncodeIRIREF keeps unreserved and encodes space / percent") {
	std::string out = "READY_OUTPUT_"; // should not modify already in the output
	rdf::percentEncodeIRIREF(out, "Hello World%Test");
	REQUIRE(out == "READY_OUTPUT_Hello%20World%25Test");

	out = "READY_OUTPUT_";

	// this may look like aggressive encoding, but remember that urls expanded from prefixes
	// are left as valid URLs
	rdf::percentEncodeIRIREF(
	    out, "https://some_very_long_url.com/path with spaces?query=param&another=value<>");
	REQUIRE(out == "READY_OUTPUT_https%3A%2F%2Fsome_very_long_url.com%2Fpath%20with%20spaces%"
	               "3Fquery%3Dparam%26another%3Dvalue%3C%3E");

	out = "";
	rdf::percentEncodeIRIREF(out, "");
	REQUIRE(out == "");
}

TEST_CASE("rdf: percentEncodeIRIREF encodes UTF-8 characters and special characters") {
	std::string out = "READY_OUTPUT_"; // should not modify already in the output
	rdf::percentEncodeIRIREF(out, "Line1\nLine2\rTab\tBackslash\\Quote\"Control\u0001");
	REQUIRE(out == "READY_OUTPUT_Line1%0ALine2%0DTab%09Backslash%5CQuote%22Control%01");

	out = "READY_OUTPUT_";
	rdf::percentEncodeIRIREF(out, "Emoji:😀, Accented:é,Control:\u0002");
	REQUIRE(out == "READY_OUTPUT_Emoji%3A%F0%9F%98%80%2C%20Accented%3A%C3%A9%2CControl%3A%02");
}

TEST_CASE("rdf: percentEncodePrefixedLocal encodes special characters but keeps safe ones") {
	std::string out = "prefix:"; // should not modify already in the output
	rdf::percentEncodePrefixedLocal(out, "Hello World%Test");
	REQUIRE(out == "prefix:Hello%20World%25Test");

	out = "prefix:";
	// non-trailing period technically allowed but we're more rigorous for simplicity
	rdf::percentEncodePrefixedLocal(out, "SafeChars-_.123");
	REQUIRE(out == "prefix:SafeChars-_%2E123");

	out = "prefix:";
	rdf::percentEncodePrefixedLocal(out, "Special chars: !@#$%^&*()<>");
	REQUIRE(out == "prefix:Special%20chars%3A%20%21%40%23%24%25%5E%26%2A%28%29%3C%3E");

	out = "prefix:";
	rdf::percentEncodePrefixedLocal(out, "Emoji:😀, Accented:é,Control:\u0002");
	REQUIRE(out == "prefix:Emoji%3A%F0%9F%98%80%2C%20Accented%3A%C3%A9%2CControl%3A%02");

	out = "prefix:";
	rdf::percentEncodePrefixedLocal(out, "");
	REQUIRE(out == "prefix:");
}

TEST_CASE("rdf: percentEncodeLiteral makes control characters explicit and escapes quotes and "
          "backslashes") {
	std::string out = "LITERAL: "; // should not modify already in the output
	rdf::percentEncodeLiteral(out, "Line1\nLine2\rTab\tBackslash\\Quote\"Control\u0001");
	REQUIRE(out == "LITERAL: Line1\\nLine2\\rTab\\tBackslash\\\\Quote\\\"Control\\u0001");

	out = "LITERAL: ";
	rdf::percentEncodeLiteral(out, "");
	REQUIRE(out == "LITERAL: ");
}

TEST_CASE("rdf: percentEncodeLiteral leaves UTF-8 characters as is") {
	std::string out = "LITERAL: "; // should not modify already in the output
	rdf::percentEncodeLiteral(
	    out, "Emoji:😀, Accented:é , Góðan daginn, ef þú ert að lesa þetta\nþá ertu fræbart");
	REQUIRE(
	    out ==
	    "LITERAL: Emoji:😀, Accented:é , Góðan daginn, ef þú ert að lesa þetta\\nþá ertu fræbart");
}
