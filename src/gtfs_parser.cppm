// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the GTFS2RDF project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.


module;

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <algorithm>
#include <iostream>

export module gtfs_parser;

import utility;
import schema.core;

using namespace util;
using util::operator<<;  // only bringing in required operator

namespace gtfs {

const char _hex_upper(unsigned v) {
  static constexpr char H[] = "0123456789ABCDEF";
  return H[v & 0xF];
}

// Gibt ein vollständig gültiges Turtle-Literal zurück, z.B.:
//   turtle_literal("A\nB", "de")           ->  "A\nB"@de
//   turtle_literal("3.14", {}, "xsd:decimal")-> "3.14"^^xsd:decimal
// Regeln:
//  - Escaped werden: \, ", \n, \r, \t, \b, \f sowie alle ASCII-Steuerzeichen 0x00..0x1F und 0x7F.
//  - Nicht-ASCII (UTF-8) bleibt unverändert (Turtle erlaubt UTF-8 direkt).
//  - Falls sowohl lang als auch datatype gesetzt sind, hat lang Vorrang (datatype wird ignoriert).
export std::string turtle_literal(std::string_view value) {
  // Schneller Vorscan: Brauchen wir überhaupt Escapes?
  bool needsEscape = false;
  for (unsigned char c : value) {
    if (c < 0x20 || c == 0x7F || c == '\\' || c == '"') { needsEscape = true; break; }
  }

  // (Worst-Case für ASCII-Steuerzeichen: \u00XX -> 6 Zeichen)
  std::string out;
  out.reserve(value.size()+ (needsEscape ? value.size() : 0));
  if (!needsEscape) {
    // Direkt übernehmen
    out.append(value);
  } else {
    for (unsigned char c : value) {
      switch (c) {
        case '\\': out.append("\\\\"); break;
        case '"' : out.append("\\\""); break;
        case '\n': out.append("\\n");  break;
        case '\r': out.append("\\r");  break;
        case '\t': out.append("\\t");  break;
        case '\b': out.append("\\b");  break;
        case '\f': out.append("\\f");  break;
        default:
          if (c < 0x20 || c == 0x7F) {
            // \u00XX
            out.push_back('\\'); out.push_back('u');
            out.push_back('0');  out.push_back('0');
            out.push_back(_hex_upper((c >> 4) & 0xF));
            out.push_back(_hex_upper(c & 0xF));
          } else {
            // Nicht-ASCII-Bytes (Teil von UTF-8) bleiben unverändert
            out.push_back(static_cast<char>(c));
          }
      }
    }
  }
  return out;
}




// CSV line splitter for GTFS
std::vector<std::string> split_line(std::string_view line) {
    // strip UTF-8 BOM if present (only relevant for first header line)
    if (line.size() >= 3 &&
        static_cast<unsigned char>(line[0]) == 0xEF &&
        static_cast<unsigned char>(line[1]) == 0xBB &&
        static_cast<unsigned char>(line[2]) == 0xBF) {
        line.remove_prefix(3);
    }

    std::vector<std::string> out;
    out.reserve(10); // small prealloc

    std::string cache;
    cache.reserve(64);

    bool in_quotes = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (in_quotes) {
            if (c == '"') {
                // doubled quote -> literal quote
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    cache.push_back('"');
                    ++i;
                } else {
                    in_quotes = false;
                }
            } else {
                cache.push_back(c);
            }
        } else {
            if (c == ',') {
                // out.push_back(std::move(cache));
                out.push_back(turtle_literal(cache));
                cache.clear();
            } else if (c == '"') {
                in_quotes = true;
            } else if (c == '\r') {
                // ignore line break characters
            } else {
                cache.push_back(c);  // used to be std::move(cache)
            }
        }
    }
    out.push_back(turtle_literal(cache));
    return out;
}


export bool parse_file(std::ifstream& ifs, size_t batch_size, bool first_batch, 
    std::vector<std::vector<std::string>>& result, schema::Schema& schema) {
    // std::ifstream ifs(path);
    // if (!ifs) {
    //     throw std::runtime_error("❌  Parsing error: unable to open file: " + path.string());
    // }

    // std::vector<std::vector<std::string>> result;
    std::string line;
    for (size_t i = 0; i < batch_size; i++) {
      if (std::getline(ifs, line)) {
        if (i == 0 && first_batch) {
            // parse header and remember column order
            auto header = split_line(line);
            schema.setHeader(header);

            // TODO: validity checks?
            i = 0;
            continue;
        }
        std::vector<std::string> cols = split_line(line);
        result.push_back(cols);  // TODO: can this be more efficient?
      } else { return true; } 
    }

    return false;
}

} // namespace