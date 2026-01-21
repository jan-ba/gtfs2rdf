// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the GTFS2RDF project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.


module;
#include <chrono>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <filesystem>
#include <utility>
#include <stdexcept>
#include <zip.h>
#include <string>   

export module gtfs_parser;

import util;
import schema;
import rdf_writer;
import runtime;

using namespace util;
using util::operator<<;  // only bringing in required operator
using Rows = std::vector<std::string>;

namespace gtfs {

char _hex_upper(unsigned v) {
  static constexpr char H[] = "0123456789ABCDEF";
  return H[v & 0xF];
}

// Gibt ein vollständig gültiges Turtle-Literal zurück, z.B.:
//   escape_literal_ttl("A\nB", "de")           ->  "A\nB"@de
//   escape_literal_ttl("3.14", {}, "xs:decimal")-> "3.14"^^xs:decimal
// Regeln:
//  - Escaped werden: \, ", \n, \r, \t, \b, \f sowie alle ASCII-Steuerzeichen 0x00..0x1F und 0x7F.
//  - Nicht-ASCII (UTF-8) bleibt unverändert (Turtle erlaubt UTF-8 direkt).
//  - Falls sowohl lang als auch datatype gesetzt sind, hat lang Vorrang (datatype wird ignoriert).
export std::string escape_literal_ttl(std::string_view value) {
  // Schneller Vorscan: Brauchen wir überhaupt Escapes? Beeinträchtigt Vorscan Performance?
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


export class GTFSParser_Workspace {
  private:
    std::vector<char> read_buffer_;
    zip_uint64_t read_buffer_capacity_;

    Rows parse_buffer_;

    writer::Writer& writer_;
    runtime::RuntimeContainer& rt_;

  public:
    GTFSParser_Workspace(runtime::RuntimeContainer& rt, writer::Writer& writer)
    : writer_(writer), rt_(rt) {
        read_buffer_capacity_ = rt.getSettings().ReadChunkSizeMB() * 1024 * 1024 + 1;
        read_buffer_.resize(read_buffer_capacity_);

        // TODO: can this be optimised? (this stores strings so cant really preallocate, 
        // creating new strings for every read chunk is wasteful)
        parse_buffer_.reserve(1024);
    }

    std::vector<char>& getReadBuffer() { return read_buffer_; }
    zip_uint64_t getReadBufferCapacity() { return read_buffer_capacity_; }
    Rows& getParseBuffer() { return parse_buffer_; }
    writer::Writer& getWriter() { return writer_; }

    GTFSParser_Workspace(const GTFSParser_Workspace&) = delete;
    GTFSParser_Workspace& operator=(const GTFSParser_Workspace&) = delete;

    GTFSParser_Workspace(GTFSParser_Workspace&&) = delete;
    GTFSParser_Workspace& operator=(GTFSParser_Workspace&&) = delete;
};

export class GTFSParser {
  private:
    using Clock = std::chrono::steady_clock;

    // 
    const std::string filename_;
    zip_file_t* file_;
    schema::Schema& schema_;
    writer::Writer& writer_;
    const bool first_file_;
    runtime::RuntimeContainer& rt_;

    // shared chunk buffer
    std::vector<char>& read_buffer_;
    zip_uint64_t chunk_size_ = 0;

    // parsing state (needs to persist across chunk boundaries)
    Rows& parse_buffer_;                     // current chunks parsed rows
    std::vector<std::string> row_;   // current csv row being built
    std::string cache_;              // current csv field being built
    CSVState state_ = CSVState::UnquotedField;
    bool header_seen_ = false;
    bool prefixes_written_ = false;
    size_t num_cols_ = 0;          // size of virtual rows in flattened parse_buffer_ vector

    // statistics
    runtime::Statistics stats_;

  private:
    void finishField_() {
        // TODO: think about escaping
        row_.push_back(escape_literal_ttl(cache_));
        cache_.clear();
    }

    void finishRow_() {
        finishField_();

        if (!header_seen_) {
            strip_utf8_bom(row_[0]);
            schema_.setHeader(row_);
            num_cols_ = row_.size();

            row_.clear();
            row_.reserve(num_cols_);
            header_seen_ = true;
        } else {
            // GTFS validity: enforce fixed width
            if (row_.size() != num_cols_) {
                // TODO: might output row here for more information
                throw std::runtime_error(
                    "❌  Parsing error: inconsistent row width in file '" + filename_ +
                    "' (expected " + std::to_string(num_cols_) +
                    " columns, got " + std::to_string(row_.size()) + ")");
            }

            // move current row into rows
            parse_buffer_.insert(parse_buffer_.end(),
                         std::make_move_iterator(row_.begin()),
                         std::make_move_iterator(row_.end()));
            row_.clear();
        }
    }

    void flushRows_() {
        if (parse_buffer_.empty()) return;

        auto t0 = Clock::now();

        // prefixes only once: first file && first write && not N-Triples
        if (first_file_ && !prefixes_written_ && !rt_.getSettings().isNTriplesOutput()) {
            writer_.writePrefixes(schema_);
            prefixes_written_ = true;
        }

        // writer expects full rows
        if (parse_buffer_.size() % num_cols_ != 0) {
            throw std::runtime_error(
                "❌  Internal error: flushRows_ called with incomplete rows in file '" + filename_ + "'"
            );
        }

        writer_.convert2RDF(schema_, parse_buffer_, num_cols_);

        stats_.write_s += std::chrono::duration<double>(Clock::now() - t0).count();
        stats_.rows += (parse_buffer_.size() / num_cols_);
        parse_buffer_.clear();
    }

    void consumeByte_(char c) {
        switch (state_) {
            case CSVState::UnquotedField:
                if (c == ',') {
                    finishField_();
                } else if (c == '"') {
                    state_ = CSVState::InQuotedField;
                } else if (c == '\n') {
                    finishRow_();
                } else if (c == '\r') {
                    // ignore CR in CRLF
                } else {
                    cache_.push_back(c);
                }
                break;

            case CSVState::InQuotedField:
                if (c == '"') {
                    state_ = CSVState::QuoteInQuotedField;
                } else {
                    cache_.push_back(c);
                }
                break;

            case CSVState::QuoteInQuotedField:
                if (c == '"') {
                    cache_.push_back('"');
                    state_ = CSVState::InQuotedField;
                } else if (c == ',') {
                    finishField_();
                    state_ = CSVState::UnquotedField;
                } else if (c == '\n') {
                    state_ = CSVState::UnquotedField;
                    finishRow_();
                } else if (c == '\r') {
                    // ignore; wait for '\n'
                } else {
                    // TODO: check this behaviour
                    std::cerr << "⚠️  Parsing warning: unexpected character '" << c
                              << "' after closing quote in quoted field\n";
                    cache_.push_back(c);
                    state_ = CSVState::UnquotedField;
                }
            break;
        }
    }

    void consumeChunk_(zip_int64_t n) {
        auto t0 = Clock::now();
        for (zip_int64_t i = 0; i < n; ++i) {
            consumeByte_(read_buffer_[i]);
        }
        stats_.parse_s += std::chrono::duration<double>(Clock::now() - t0).count();
    }

    void flushRemainder_() {
        // if file doesn't end with newline, finalise last row/field.
        if (!cache_.empty() || !row_.empty()) {
            finishRow_();
        }
        flushRows_();
    }

  public:
    GTFSParser(zip_file_t* zf, schema::Schema& schema, bool first_file,
               GTFSParser_Workspace& ws, runtime::RuntimeContainer& rt)
    : filename_(schema.getName()),
      file_(zf),
      schema_(schema),
      writer_(ws.getWriter()),
      first_file_(first_file),
      rt_(rt),
      read_buffer_(ws.getReadBuffer()),
      parse_buffer_(ws.getParseBuffer())
    {
        chunk_size_ = ws.getReadBufferCapacity();
        parse_buffer_.clear();
        row_.clear();
        cache_.clear();

        cache_.reserve(64);
    }

    // do the whole file in one pass
    void parse() {
        while (true) {
            zip_int64_t n = zip_fread(file_, read_buffer_.data(), chunk_size_);
            if (n < 0) {
            throw std::runtime_error("❌  Parsing error: can't read chunk number " + std::to_string(stats_.chunks)
                                    + " from file '" + filename_ + "'");
            }
            if (n == 0) break;

            stats_.chunks++;
            consumeChunk_(n);
            flushRows_();
        }

        flushRemainder_();
        for (const auto& instr : schema_.getInstructions()) {
            stats_.triples += instr.getCount();
        }
    }

    // getters
    const runtime::Statistics& getStats() const { return stats_; }
    std::string_view getFilename() const { return filename_; }
};



} // namespace