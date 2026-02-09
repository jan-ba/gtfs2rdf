// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the gtfs2rdf project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.

module;

#include "transform_macros.h"

#include <cctype>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

export module schema:translations;

import :core;
import rdf_components;
import field_transforms;
import t_lib;
import runtime;
import util;

using namespace rdf;

namespace schema {

// -----------------------------------------------------------------------------
// Gtfs -> Rdf schema for translations.txt
// -----------------------------------------------------------------------------
export Schema buildTranslationsSchema(runtime::RuntimeContainer& rtc) {
	// to be used when trying to find translations by field value
	// hence ARGS[0]: table_name, ARGS[1]: field_name, ARGS[2]: field_value
	// TODO: don't use if record_id exists since then translations can just be expressed from
	// within this schema file directly without storage across schemas
	TRANSFORM2MANY(get_translation, ARGS, OUT_VALS, STORAGE) {
		if (ARGS[0].empty() || ARGS[1].empty() || ARGS[2].empty()) {
			return;
		}
		for (const auto& tup : STORAGE.getTuples(
		         "translations.txt", "translations_by_value", {ARGS[0], ARGS[1], ARGS[2]})) {
			OUT_VALS.emplace_back(tup[0]);
			// this is a fairly hacky way to store the latest language used for translation
			// so that the language tag can be set correctly in the triple later
			// by adding {latest_translation_lookup@translations.txt} in the language field
			STORAGE.storeVariable("translations.txt", "latest_translation_lookup", tup[1]);
		}
	}
	TRANSFORM_END

	// turn snake_case into camelCase (e.g. for stop_name -> stopName)
	// with this translations by record_id can be easily created in this schema file
	// TODO: move to lib?
	TRANSFORM2ONE(capitalise_underscored, ARGS, OUT_VAL, STORAGE) {
		if (ARGS[0].empty()) {
			return;
		}
		auto parts = util::split(ARGS[0], '_');

		bool first_part = true;
		for (auto part : parts) {
			auto c0 = static_cast<unsigned char>(part[0]);

			// first chunk: lowerCamel (lowercase first letter), later chunks: UpperCamel (uppercase
			// first letter)
			if (first_part) {
				OUT_VAL.push_back(std::isalpha(c0) ? static_cast<char>(std::tolower(c0))
				                                   : static_cast<char>(c0));
				first_part = false;
			} else {
				OUT_VAL.push_back(std::isalpha(c0) ? static_cast<char>(std::toupper(c0))
				                                   : static_cast<char>(c0));
			}

			for (size_t i = 1; i < part.size(); ++i) {
				auto c = static_cast<unsigned char>(part[i]);
				OUT_VAL.push_back(static_cast<char>(std::tolower(c)));
			}
		}
	}
	TRANSFORM_END

	TRANSFORM2ONE(filter_if_record_id_defined, ARGS, OUT_VAL, STORAGE) {
		(void)ARGS; // unused on purpose
		if (STORAGE.getVariable("translations.txt", "is_record_id_defined").empty()) {
			OUT_VAL = "1";
		}
	}
	TRANSFORM_END

	const std::vector<std::string> POSSIBLE_COLUMNS = {"table_name",
	                                                   "field_name",
	                                                   "language",
	                                                   "record_id",
	                                                   "record_sub_id",
	                                                   "field_value",
	                                                   "translation"};

	const std::unordered_map<std::string, std::string> PREFIXES = {
	    {"trans", "https://gtfs.org/translations/"},

	    {"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
	    {"xs", "http://www.w3.org/2001/XMLSchema#"},
	    {"gtfs", "https://w3id.org/gtfs2rdf#"}};

	const std::vector<std::string> NO_WRITE_INSTRUCTIONS = {
	    // store whether record_id is defined in variable
	    "{ record_id > is_record_id_defined}",

	    // TODO: this is not implemented in other schemas
	    "{ table_name , field_name, record_id : translation, language "
	    " > translations_by_record@translations.txt }",

	    "{ table_name, field_name , field_value: (translation, language)"
	    " | filter_if_record_id_defined@translations.txt > translations_by_value@translations.txt "
	    "}"};

	const std::vector<Triple> TRIPLES = {};

	Schema sch("translations.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, NO_WRITE_INSTRUCTIONS, rtc);
	return sch;
}

} // namespace schema
