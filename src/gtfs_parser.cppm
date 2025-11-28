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
#include <zip.h>

export module gtfs_parser;

import utility;
import schema.core;
import rdf_writer;

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


void strip_utf8_bom(std::string& s) {
    if (s.size() >= 3 &&
        static_cast<unsigned char>(s[0]) == 0xEF &&
        static_cast<unsigned char>(s[1]) == 0xBB &&
        static_cast<unsigned char>(s[2]) == 0xBF) {
        s.erase(0, 3);
    }
}

enum class CSVState {
    UnquotedField,
    InQuotedField,
    QuoteInQuotedField  // just saw a " inside a quoted field
};

export int64_t translateFileToStream(zip_file_t* zf, schema::Schema& schema,
                              std::string filename, std::ostream& outfs, double batch_size_mb, 
                              const bool first_file)
{
    using Clock = std::chrono::steady_clock;
    std::vector<std::vector<std::string>> rows;
    zip_uint64_t batch_size = batch_size_mb * 1024 * 1024 + 1;  // offset to avoid zero

    std::vector<char> buf(batch_size);

    // Counters
    size_t batch_i     = 0;
    int64_t total_triples = 0;
    size_t total_rows  = 0;
    double parse_s = 0.0;
    double write_s = 0.0;
    std::cout << "\n______________________________________________________________\n";

    std::vector<std::string> row; row.reserve(10); // small prealloc
    std::string cache;            cache.reserve(64);
    CSVState state = CSVState::UnquotedField;
    bool header_seen = false;

    
    while (true) {
        auto t0 = Clock::now();  // start file parsing time measurement
        zip_int64_t n = zip_fread(zf, buf.data(), batch_size);
        // std::cout << "Read batch " << batch_i << ":\n" << buf 
        //           << "\n==============================\n";
        if (n < 0) {
            // handle error
            throw std::runtime_error("❌  Error: can't read batch number " + std::to_string(batch_i)
                                     + " from file '" + filename + "'");
        }
        if (n == 0) {
            // EOF, we're done here
            parse_s += std::chrono::duration<double>(Clock::now() - t0).count();
            break;
        }
        batch_i++;

        // now buf[0..n-1] contains valid bytes, buf[n..] is irrelevant.
        for (zip_int64_t i = 0; i < n; ++i) {
            char c = buf[i];
            switch (state) {
                case CSVState::UnquotedField:
                    if (c == ',') {  // end of field
                        row.push_back(turtle_literal(cache));
                        cache.clear();
                    } else if (c == '"') {  // start quoted field
                        state = CSVState::InQuotedField;
                    } else if (c == '\n') {  // end of field + row
                        row.push_back(turtle_literal(cache));
                        cache.clear();

                        if (!header_seen) {  // handle header row
                            strip_utf8_bom(row[0]);
                            schema.setHeader(row);
                            row.clear();
                            header_seen = true;
                        } else {
                            // row is complete
                            rows.push_back(row);
                            row.clear();
                        }
                    } else if (c == '\r') {  // ignore (CR part of CRLF)
                    } else {
                        cache.push_back(c);
                    }
                    break;

                case CSVState::InQuotedField:
                    if (c == '"') {  // maybe escaped quote, maybe end of quoted field
                        state = CSVState::QuoteInQuotedField;
                    } else {
                        cache.push_back(c);
                    }
                    break;

                case CSVState::QuoteInQuotedField:
                    if (c == '"') {  // "" -> literal "
                        cache.push_back('"');
                        state = CSVState::InQuotedField;
                    } else if (c == ',') {  // closing " followed by comma -> end of field
                        row.push_back(turtle_literal(cache));
                        cache.clear();
                        state = CSVState::UnquotedField;
                    } else if (c == '\n') {  // closing " followed by newline -> end of field + row
                        // finish the last field
                        row.push_back(turtle_literal(cache));
                        cache.clear();
                        state = CSVState::UnquotedField;

                        // header vs data
                        if (!header_seen) {
                            strip_utf8_bom(row[0]);
                            schema.setHeader(row);
                            row.clear();
                            header_seen = true;
                        } else {
                            rows.push_back(row);
                            row.clear();
                        }
                    } else if (c == '\r') {
                        // closing " followed by CR, ignore here;
                        // next char might be '\n'
                        state = CSVState::QuoteInQuotedField;
                    } else {
                        std::cerr << "⚠️  Warning: unexpected character '" << c
                                  << "' after closing quote in quoted field\n";  // TODO: add information package
                        cache.push_back(c);
                        state = CSVState::UnquotedField;
                    }
                    break;
            }
        }
        parse_s += std::chrono::duration<double>(Clock::now() - t0).count();

        // write this batch (prefixes only for the very first batch)
        t0 = Clock::now();
        total_triples += ttl::write2TTL(schema, rows, outfs, first_file && batch_i == 1);
        write_s += std::chrono::duration<double>(Clock::now() - t0).count();

        total_rows += rows.size();
        rows.clear();
    }

    // in case file does not end with newline, flush remaining data
    if (!cache.empty() || !row.empty()) {
        row.push_back(turtle_literal(cache));
        cache.clear();
        if (!header_seen) {
            strip_utf8_bom(row[0]);
            schema.setHeader(row);
            header_seen = true;
        } else {
            rows.push_back(row);
        }
        row.clear();
        auto t0 = Clock::now();
        total_triples += ttl::write2TTL(schema, rows, outfs, first_file && batch_i == 1);
        write_s += std::chrono::duration<double>(Clock::now() - t0).count();
        total_rows += rows.size();
    }

    std::cout << "⌛  Parsed " << filename << " in " << parse_s << " s"
              << "  (" << total_rows << " rows, " << batch_i
              << " batches @ " << batch_size_mb << ")\n";
    std::cout << "✅  Wrote " << total_triples << " triples from "
              << filename << " in " << write_s << " s\n"
              << "______________________________________________________________\n";
    return total_triples;
}


} // namespace