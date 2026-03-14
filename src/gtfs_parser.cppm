// SPDX-FileCopyrightText: 2026 Jan Babin
// SPDX-License-Identifier: AGPL-3.0-only
//
// Part of gtfs2rdf. See the LICENSE file for details.

module;

#include "macros.h"
#include "util/diagnostics.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
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

namespace gtfs {

// remove UTF-8 BOM from start of string, if present
// NOLINTBEGIN : 'magic numbers' are required and very specific here
void stripUTF8Bom(std::string& str) {
	if (str.size() >= 3 && static_cast<unsigned char>(str[0]) == 0xEF &&
	    static_cast<unsigned char>(str[1]) == 0xBB && static_cast<unsigned char>(str[2]) == 0xBF) {
		str.erase(0, 3);
	}
}
// NOLINTEND

// workspace class to hold shared state between multiple parsers, e.g. shared buffers
// gtfs2rdf uses one parser per GTFS file but one writer in total for the output file
export class GtfsParserWorkspace {
  public:
	GtfsParserWorkspace(runtime::RuntimeContainer& rtc, writer::Writer& writer)
	    : writer_(writer)
	// , rtc_(rtc)  // not currently required, perhaps needed later
	{
		read_buffer_capacity_ = rtc.getSettings().getReadBufferSize_MB() * 1024 * 1024; // NOLINT
		read_buffer_.resize(read_buffer_capacity_);
	}

	std::vector<char>& getReadBuffer() {
		return read_buffer_;
	}
	[[nodiscard]] zip_uint64_t getReadBufferCapacity() const {
		return read_buffer_capacity_;
	}
	writer::Writer& getWriter() {
		return writer_;
	}

	GtfsParserWorkspace(const GtfsParserWorkspace&) = delete;
	GtfsParserWorkspace& operator=(const GtfsParserWorkspace&) = delete;

	GtfsParserWorkspace(GtfsParserWorkspace&&) = delete;
	GtfsParserWorkspace& operator=(GtfsParserWorkspace&&) = delete;

  private:
	std::vector<char> read_buffer_;
	zip_uint64_t read_buffer_capacity_;
	writer::Writer& writer_;
	// runtime::RuntimeContainer& rtc_;  // not currently used, perhaps useful later
};

// state-machine based CSV parser that consumes the GTFS file in chunks and passes each parsed
// row directly to the writer for conversion and output (streaming execution)
export class GtfsParser {
  public:
	GtfsParser(zip_file_t* z_file,
	           schema::Schema& schema,
	           GtfsParserWorkspace& wsp,
	           runtime::RuntimeContainer& rtc)
	    : FILENAME_(schema.getName())
	    , file_(z_file)
	    , schema_(schema)
	    , writer_(wsp.getWriter())
	    , rtc_(rtc)
	    , read_buffer_(wsp.getReadBuffer()) {
#if GTFS2RDF_FULL_STATS
		writer_.resetTiming();
#endif
		stats_.name = FILENAME_;
		buffer_size_ = wsp.getReadBufferCapacity();
		row_buffer_.clear();
		field_buffer_.clear();
		field_buffer_.reserve(256); // NOLINT(readability-magic-numbers): typical field size, avoids
		                            // many small resizes while building fields char by char

		header_seen_ = false;
		num_cols_ = 0;
		col_i_ = 0;
		state_ = CSVState::UNQUOTED_FIELD;
	}

	// parses the file in chunks until EOF
	void parse() {
#if GTFS2RDF_FULL_STATS
		start_time_ = std::chrono::steady_clock::now();
#endif

		while (true) {
			try {
				zip_int64_t len_read = zip_fread(file_, read_buffer_.data(), buffer_size_);
				if (len_read < 0) {
					throw diagnostics::Error("Parsing error: can't read chunk number " +
					                         std::to_string(stats_.chunks + 1));
				}
				if (len_read == 0) {
					break;
				}

				stats_.chunks++;
				consumeChunk_(len_read);

				// row offset because stats_.rows is incremented after parsing a row,
				// but errors are detected during parsing, warnings aren't
				// +1 in any way to account for header row (not for stats, but for user-facing
				// messages)
				rtc_.getWarningCollector().addNode(
				    "while parsing row number " + std::to_string(stats_.rows + 1), 7);
			} catch (const std::exception& excpt) {
				diagnostics::wrapAndRethrow("while parsing row number " +
				                            std::to_string(stats_.rows + 2));
			}
		}

		flushRemainder_();

		// aggregate triples count from all instructions for stats
		for (const auto& instr : schema_.getInstructions()) {
			stats_.triples += instr.getCount();
		}

#if GTFS2RDF_FULL_STATS
		end_time_ = std::chrono::steady_clock::now();
		parse_ns_tmp_ +=
		    (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(end_time_ - start_time_)
		        .count();
#endif
	}

	const diagnostics::Statistics& getStats() const {
#if GTFS2RDF_FULL_STATS
		if (!rtc_.getSettings().isPreRun())
			conversion_ns_ = writer_.getConversionTimeNS();
		stats_.parse_ns = parse_ns_tmp_ - writer_.getWriteTimeNS() - conversion_ns_;
		stats_.write_ns = writer_.getWriteTimeNS();
		stats_.conversion_ns = conversion_ns_;
#endif

		return stats_;
	}

	std::string_view getFilename() const {
		return FILENAME_;
	}

  private:
	// _____________________________________________________________________________________________
	// private helper methods
	// _____________________________________________________________________________________________

	// end of field reached, adds field_buffer_ to row_buffer_ and resets for next field
	void finishField_() {
		if (!header_seen_) {
			row_buffer_.push_back(field_buffer_);
		} else {
			if (col_i_ >= num_cols_) {
				throw diagnostics::Error("Parsing error: too many columns in row number " +
				                         std::to_string(stats_.rows + 1) + " (expected " +
				                         std::to_string(num_cols_) + ")");
			}
			// overwrite in-place (reuses row_buffer_[col_i_] capacity if possible)
			// this is because of fixed width is known after header read (else errror thrown above)
			row_buffer_[col_i_].assign(field_buffer_);
			++col_i_;
		}
		field_buffer_.clear();
	}

	// end of row reached (line break), passes reference to row_buffer_ on for conversion and resets for next row
	void finishRow_() {
		finishField_();

		// if header not seen yet, the current row is the header -> handle accordingly
		if (!header_seen_) {
			if (row_buffer_.empty()) {
				throw diagnostics::Error("Parsing error: empty header row");
			}
			stripUTF8Bom(row_buffer_[0]);
			try {
				schema_.setHeader(row_buffer_);
				rtc_.getWarningCollector().addNode("while setting header '" + row_buffer_ + "'",
				                                   3); // NOLINT(readability-identifier-naming)
			} catch (const std::exception& excpt) {
				diagnostics::wrapAndRethrow("while setting header '" + row_buffer_ + "'");
			}
			num_cols_ = row_buffer_.size();

			row_buffer_.clear();
			row_buffer_.resize(num_cols_); // row_buffer_ size will remain fixed from now on

			header_seen_ = true;
			col_i_ = 0;

		// if header already seen, pass data row on for conversion
		} else {
			// Gtfs validity: enforce fixed width
			if (col_i_ != num_cols_) {
				throw diagnostics::Error("Parsing error: inconsistent row width in row number " +
				                         std::to_string(stats_.rows + 1) + " (expected " +
				                         std::to_string(num_cols_) + " columns, got " +
				                         std::to_string(col_i_) + ")");
			}

			// process first <sample_size> rows to extrapolate whole run stats
			if (rtc_.getSettings().isPreRun()) {
				const auto SAMPLE_SIZE = rtc_.getSettings().getPreRunSampleSize();

				if (stats_.rows < SAMPLE_SIZE) {
					auto& instrs = schema_.getInstructions();
					for (auto& instr : instrs) {
						SCOPED_TIMER_NS(conversion_ns_);
						stats_.num_chars += instr.render(row_buffer_).size();
					}
				}
			}

			else {
				writer_.convertRow(schema_, row_buffer_);
			}

			stats_.rows += 1;

			// prepare for next row
			col_i_ = 0;
		}
	}

	enum class CSVState : uint8_t {
		UNQUOTED_FIELD,
		IN_QUOTED_FIELD,
		QUOTE_IN_QUOTED_FIELD // just saw a " inside a quoted field
	};

	// state-machine based CSV parsing according to GTFS specific rules (more restrictive than general CSV)
	void consumeByte_(char c) {
		switch (state_) {
			case CSVState::UNQUOTED_FIELD:
				if (c == ',') {
					finishField_();
				} else if (c == '"') {
					state_ = CSVState::IN_QUOTED_FIELD;
				} else if (c == '\n') {
					finishRow_();
				} else if (c == '\r') {
					// ignore CR in CRLF
				} else {
					field_buffer_.push_back(c);
				}
				break;

			case CSVState::IN_QUOTED_FIELD:
				if (c == '"') {
					state_ = CSVState::QUOTE_IN_QUOTED_FIELD;
				} else {
					field_buffer_.push_back(c);
				}
				break;

			case CSVState::QUOTE_IN_QUOTED_FIELD:
				if (c == '"') {
					field_buffer_.push_back('"');
					state_ = CSVState::IN_QUOTED_FIELD;
				} else if (c == ',') {
					finishField_();
					state_ = CSVState::UNQUOTED_FIELD;
				} else if (c == '\n') {
					state_ = CSVState::UNQUOTED_FIELD;
					finishRow_();
				} else if (c == '\r') {
					// ignore; wait for '\n'
				} else {
					throw diagnostics::Error("Parsing error: unexpected character '" +
					                         std::string(1, c) +
					                         "' after closing quote in quoted field");
				}
				break;
		}
	}

	void consumeChunk_(zip_int64_t len_read) {
		for (zip_int64_t i = 0; i < len_read; ++i) {
			consumeByte_(read_buffer_[i]);
		}
	}

	void flushRemainder_() {
		// if file doesn't end with newline, finalise last row/field.
		if (!field_buffer_.empty() || (!header_seen_ && !row_buffer_.empty()) || (header_seen_ && col_i_ > 0)) {
			finishRow_();
		}
	}

	// _____________________________________________________________________________________________
	// private members
	// _____________________________________________________________________________________________

	const std::string FILENAME_;
	zip_file_t* file_;
	schema::Schema& schema_;
	writer::Writer& writer_;
	runtime::RuntimeContainer& rtc_;

	// shared buffer buffer
	std::vector<char>& read_buffer_;
	zip_uint64_t buffer_size_ = 0;

	// parsing state (needs to persist across chunk boundaries)
	std::vector<std::string> row_buffer_; // current csv row being built / reused
	std::string field_buffer_;            // current csv field being built
	CSVState state_ = CSVState::UNQUOTED_FIELD;
	bool header_seen_ = false;
	size_t num_cols_ = 0; // fixed width after header
	size_t col_i_ = 0;    // current column index in row_buffer_ (data rows)

	// statistics
	mutable diagnostics::Statistics stats_;

#if GTFS2RDF_FULL_STATS
	std::chrono::steady_clock::time_point start_time_;
	std::chrono::steady_clock::time_point end_time_;
	uint64_t parse_ns_tmp_ = 0; // temporary write time accumulator
	mutable uint64_t conversion_ns_ = 0;
#endif
};

} // namespace gtfs