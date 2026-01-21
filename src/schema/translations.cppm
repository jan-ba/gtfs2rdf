// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the GTFS2RDF project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.

module;

#include "transform_macros.h"

#include <string>
#include <cctype>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <iostream>

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
// GTFS -> RDF schema for translations.txt
// -----------------------------------------------------------------------------
export Schema buildTranslationsSchema(runtime::RuntimeContainer& rt) {

    // to be used when trying to find translations by field value
    // hence ARGS[0]: table_name, ARGS[1]: field_name, ARGS[2]: field_value
    // TODO: don't use if record_id exists since then translations can just be expressed from
    // within this schema file directly without storage across schemas
    TRANSFORM2MANY(get_translation, ARGS, OUT_VALS, STORAGE)
        if (ARGS[0].empty() || ARGS[1].empty() || ARGS[2].empty()) return;
        for (const auto& tup : STORAGE.get("translations.txt", "translations_by_value",
                                           { ARGS[0], ARGS[1], ARGS[2] })) {
            OUT_VALS.emplace_back(tup[0]);
            // this is a fairly hacky way to store the latest language used for translation
            // so that the language tag can be set correctly in the triple later
            // by adding {latest_translation_lookup@translations.txt} in the language field
            STORAGE.store("translations.txt", "latest_translation_lookup", tup[1]);
        }
    TRANSFORM_END

    // turn snake_case into camelCase (e.g. for stop_name -> stopName)
    // with this translations by record_id can be easily created in this schema file
    TRANSFORM2ONE(capitalise_underscored, ARGS, OUT_VAL, STORAGE)
        if (ARGS[0].empty()) return;
        auto parts = util::split(ARGS[0], '_');

        bool first_part = true;
        for (auto& p : parts) {
            unsigned char c0 = static_cast<unsigned char>(p[0]);

            // first chunk: lowerCamel (lowercase first letter), later chunks: UpperCamel (uppercase first letter)
            if (first_part) {
                OUT_VAL.push_back(std::isalpha(c0) ? static_cast<char>(std::tolower(c0)) : static_cast<char>(c0));
                first_part = false;
            } else {
                OUT_VAL.push_back(std::isalpha(c0) ? static_cast<char>(std::toupper(c0)) : static_cast<char>(c0));
            }

            for (size_t i = 1; i < p.size(); ++i) {
                unsigned char c = static_cast<unsigned char>(p[i]);
                OUT_VAL.push_back(static_cast<char>(std::tolower(c)));
            }
        }
    TRANSFORM_END

    TRANSFORM2ONE(filter_if_record_id_defined, ARGS, OUT_VAL, STORAGE)
        if (STORAGE.get("translations.txt", "is_record_id_defined").empty()) OUT_VAL = "1";
    TRANSFORM_END


    const std::vector<std::string> possible_columns = {
      "table_name",
      "field_name",
      "language",
      "record_id",
      "record_sub_id",
      "field_value",
      "translation"
    };

    const std::unordered_map<std::string, std::string> prefixes = {
      { "trans", "https://gtfs.org/translations/" },

      { "rdf",  "http://www.w3.org/1999/02/22-rdf-syntax-ns#" },
      { "xs",  "http://www.w3.org/2001/XMLSchema#" },
      { "gtfs", "https://w3id.org/gtfs2rdf#" }
    };

    const std::vector<std::string> storage_only_instructions = {
        // store whether record_id is defined in variable
        "{ record_id > is_record_id_defined}"

        "{ table_name, field_name , field_value: (translation, language)"
        " | filter_if_record_id_defined > translations_by_value@translations.txt }"
    };

    const std::vector<Triple> triples = {
        // produces translation triple when record_id is present
        { {"{table_name}", "{record_id}"}, {"gtfs", "{field_name | capitalise_underscored}"},
          {"{translation}", "{language}"} }

    };

  Schema sc("translations.txt", possible_columns, prefixes, triples, storage_only_instructions, rt);
  return sc;
}

} // namespace schema
