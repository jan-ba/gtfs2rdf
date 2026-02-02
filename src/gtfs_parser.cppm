// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the GTFS2RDF project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.

module;

#include "macros.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <zip.h>

export module gtfs_parser;

import util;
import schema;
import writer;
import runtime;

using namespace util;
using util::operator<<; // only bringing in required operator
using Rows = std::vector<std::string>;

namespace gtfs {

// remove UTF-8 BOM from start of string, if present
void strip_utf8_bom(std::string &s) {
	if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEF &&
	    static_cast<unsigned char>(s[1]) == 0xBB && static_cast<unsigned char>(s[2]) == 0xBF) {
		s.erase(0, 3);
	}
}

enum class CSVState {
	UnquotedField,
	InQuotedField,
	QuoteInQuotedField // just saw a " inside a quoted field
};

export class GTFSParser_Workspace {
  private:
	std::vector<char> read_buffer_;
	zip_uint64_t read_buffer_capacity_;
	writer::Writer &writer_;
	runtime::RuntimeContainer &rt_;

  public:
	GTFSParser_Workspace(runtime::RuntimeContainer &rt, writer::Writer &writer)
	    : writer_(writer)
	    , rt_(rt) {
		read_buffer_capacity_ = rt.getSettings().ReadBufferSizeMB() * 1024 * 1024 + 1;
		read_buffer_.resize(read_buffer_capacity_);
	}

	std::vector<char> &getReadBuffer() {
		return read_buffer_;
	}
	zip_uint64_t getReadBufferCapacity() {
		return read_buffer_capacity_;
	}
	writer::Writer &getWriter() {
		return writer_;
	}

	GTFSParser_Workspace(const GTFSParser_Workspace &) = delete;
	GTFSParser_Workspace &operator=(const GTFSParser_Workspace &) = delete;

	GTFSParser_Workspace(GTFSParser_Workspace &&) = delete;
	GTFSParser_Workspace &operator=(GTFSParser_Workspace &&) = delete;
};

export class GTFSParser {
  private:
	const std::string filename_;
	zip_file_t *file_;
	schema::Schema &schema_;
	writer::Writer &writer_;
	runtime::RuntimeContainer &rt_;

	// shared buffer buffer
	std::vector<char> &read_buffer_;
	zip_uint64_t buffer_size_ = 0;

	// parsing state (needs to persist across chunk boundaries)
	std::vector<std::string> row_; // current csv row being built / reused
	std::string cache_;            // current csv field being built
	CSVState state_ = CSVState::UnquotedField;
	bool header_seen_ = false;
	size_t num_cols_ = 0; // fixed width after header
	size_t col_i_ = 0;    // current column index in row_ (data rows)

	// statistics
	mutable runtime::Statistics stats_;

#if GTFS2RDF_FULL_STATS
	std::chrono::steady_clock::time_point start_time_;
	std::chrono::steady_clock::time_point end_time_;
	uint64_t parse_ns_tmp_ = 0; // temporary write time accumulator
	mutable uint64_t conversion_ns_ = 0;
#endif

	inline void beginDataRow_IfNeeded_() {
		// For data rows, ensure row_ has fixed width (already resized after header)
		// and reset the column index. We don't clear all strings here because we enforce
		// exact width; every position will be assigned exactly once.
		col_i_ = 0;
	}

	void finishField_() {
		if (!header_seen_) {
			row_.push_back(cache_);
		} else {
			if (col_i_ >= num_cols_) {
				throw std::runtime_error("❌  Parsing error: too many columns in file '" +
				                         filename_ + "' (expected " + std::to_string(num_cols_) +
				                         " columns)");
			}
			// overwrite in-place (reuses row_[col_i_] capacity when possible)
			row_[col_i_].assign(cache_);
			++col_i_;
		}
		cache_.clear();
	}

	void finishRow_() {
		finishField_();

		if (!header_seen_) {
			if (row_.empty()) {
				throw std::runtime_error("❌  Parsing error: empty header in file '" + filename_ +
				                         "'");
			}
			strip_utf8_bom(row_[0]);

			schema_.setHeader(row_);
			num_cols_ = row_.size();

			row_.clear();
			row_.resize(num_cols_); // row_ size will remain fixed from now on

			header_seen_ = true;
			beginDataRow_IfNeeded_();
		} else {
			// GTFS validity: enforce fixed width
			if (col_i_ != num_cols_) {
				throw std::runtime_error("❌  Parsing error: inconsistent row width in file '" +
				                         filename_ + "' (expected " + std::to_string(num_cols_) +
				                         " columns, got " + std::to_string(col_i_) + ")");
			}

			// process first <sample_size> rows to extrapolate whole run stats
			if (rt_.getSettings().isPreRun()) {
				const auto sample = rt_.getSettings().getPreRunSampleSize();

				if (stats_.rows < sample) {
					auto &instrs = schema_.getInstructions();
					for (auto &instr : instrs) {
						SCOPED_TIMER_NS(conversion_ns_);
						stats_.num_chars += instr.render(row_).size();
					}
				}
			}

			else
				writer_.convertRow(schema_, row_);

			stats_.rows += 1;

			// prepare for next row
			beginDataRow_IfNeeded_();
		}
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
				std::cerr << "⚠️  Parsing warning: unexpected character '" << c
				          << "' after closing quote in quoted field\n";
				cache_.push_back(c);
				state_ = CSVState::UnquotedField;
			}
			break;
		}
	}

	void consumeChunk_(zip_int64_t n) {
		for (zip_int64_t i = 0; i < n; ++i) {
			consumeByte_(read_buffer_[i]);
		}
	}

	void flushRemainder_() {
		// if file doesn't end with newline, finalise last row/field.
		if (!cache_.empty() || (!header_seen_ && !row_.empty()) || (header_seen_ && col_i_ > 0)) {
			finishRow_();
		}
	}

  public:
	GTFSParser(zip_file_t *zf,
	           schema::Schema &schema,
	           GTFSParser_Workspace &ws,
	           runtime::RuntimeContainer &rt)
	    : filename_(schema.getName())
	    , file_(zf)
	    , schema_(schema)
	    , writer_(ws.getWriter())
	    , rt_(rt)
	    , read_buffer_(ws.getReadBuffer()) {
#if GTFS2RDF_FULL_STATS
		writer_.resetTiming();
#endif

		buffer_size_ = ws.getReadBufferCapacity();
		row_.clear();
		cache_.clear();
		cache_.reserve(256);

		header_seen_ = false;
		num_cols_ = 0;
		col_i_ = 0;
		state_ = CSVState::UnquotedField;
	}

	void parse() {
#if GTFS2RDF_FULL_STATS
		start_time_ = std::chrono::steady_clock::now();
#endif

		while (true) {
			zip_int64_t n = zip_fread(file_, read_buffer_.data(), buffer_size_);
			if (n < 0) {
				throw std::runtime_error("❌  Parsing error: can't read buffer number " +
				                         std::to_string(stats_.chunks) + " from file '" +
				                         filename_ + "'");
			}
			if (n == 0)
				break;

			stats_.chunks++;
			consumeChunk_(n);
		}

		flushRemainder_();

		for (const auto &instr : schema_.getInstructions()) {
			stats_.triples += instr.getCount();
		}

#if GTFS2RDF_FULL_STATS
		end_time_ = std::chrono::steady_clock::now();
		parse_ns_tmp_ +=
		    (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(end_time_ - start_time_)
		        .count();
#endif
	}

	const runtime::Statistics &getStats() const {
#if GTFS2RDF_FULL_STATS
		if (!rt_.getSettings().isPreRun())
			conversion_ns_ = writer_.getConversionTimeNS();
		stats_.parse_ns = parse_ns_tmp_ - writer_.getWriteTimeNS() - conversion_ns_;
		stats_.write_ns = writer_.getWriteTimeNS();
		stats_.conversion_ns = conversion_ns_;
#endif

		return stats_;
	}
	std::string_view getFilename() const {
		return filename_;
	}
};

} // namespace gtfs