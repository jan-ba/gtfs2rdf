// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the GTFS2RDF project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.

module;

#include "macros.h"
#include "util/diagnostics.h"

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

export module writer;
import schema;
import runtime;
import util;

using namespace schema;

using Rows = std::vector<std::vector<std::string>>;

namespace writer {

// writer with buffered output
// one instance per output file
export class Writer {
  public:
	Writer(const std::filesystem::path &path,
	       runtime::RuntimeContainer &rt,
	       double buffer_size_mb,
	       bool active = true)
	    : rt_(rt)
	    , threshold_(buffer_size_mb * 1024 * 1024)
	    , active_(active) {
		if (std::filesystem::exists(path)) {
			if (!rt_.getSettings().isOverwriteOutput()) {
				throw diagnostics::Error(
				    "IO error: output file '" + path.string() +
				    "' already exists. To overwrite, enable the overwrite option.");
			}
		}
		if (active_) {
			file_ = std::fopen(path.string().c_str(), "wb");
			file_path_ = path;
		} else {
			file_ = nullptr; // discard output
		}
		if (active_ && !file_)
			throw diagnostics::Error("IO error: Cannot open '" + path.string() + "' for writing.");
		buffer_.reserve(threshold_);
	}

	Writer(const std::filesystem::path &path, runtime::RuntimeContainer &rt, bool active = true)
	    : Writer(path, rt, rt.getSettings().WriteBufferSizeMB(), active) {
	}

	void writePrefixes(const std::unordered_map<std::string, std::string> &map) {
		SCOPED_TIMER_NS(write_ns_tmp_);

		std::string out;
		out.reserve(map.size() * 15); // rough estimate
		for (const auto &[pfx, iri] : map) {
			out.append("@prefix ").append(pfx).append(": <").append(iri).append("> .\n");
		}
		out.append("\n");
		append_(out);
	}

	void writeRaw(std::string_view sv) {
		SCOPED_TIMER_NS(write_ns_tmp_);

		append_(std::string(sv));
	}

	// converts gtfs row according to given schema and appends to buffer
	void convertRow(Schema &sc, const std::vector<std::string> &row) {
		SCOPED_TIMER_NS(write_ns_tmp_);

		auto &instructions = sc.getInstructions();

		for (auto &instr : instructions) {
			try {
				std::string_view rendered;
				{ // timer scope
					SCOPED_TIMER_NS(conversion_ns_);
					rendered = instr.render(row);
				}
				append_(rendered);
			} catch (const diagnostics::Error &e) {
				diagnostics::wrap_and_rethrow(e,
				                              "while rendering instruction '" +
				                                  instr.getRawInstruction() + "' in schema '" +
				                                  sc.getName() + "'");
			}
		}
	}

	void deleteFile() {
		if (active_) {
			std::filesystem::remove(file_path_);
		}
	}

#if GTFS2RDF_FULL_STATS
	uint64_t getWriteTimeNS() const {
		return write_ns_tmp_ - conversion_ns_;
	}

	uint64_t getConversionTimeNS() const {
		return conversion_ns_;
	}

	void resetTiming() {
		write_ns_tmp_ = 0;
		conversion_ns_ = 0;
	}
#endif

	Writer(const Writer &) = delete;
	Writer &operator=(const Writer &) = delete;

	Writer(Writer &&) = delete;
	Writer &operator=(Writer &&) = delete;

	~Writer() {
		flush_();
		if (active_ && file_)
			std::fclose(file_);
	}

  private:
	std::filesystem::path file_path_;
	std::FILE *file_;
	std::string buffer_;
	const runtime::RuntimeContainer &rt_;
	size_t threshold_;
	const bool active_ = true; // whether this writer should actually write (or discard) data

#if GTFS2RDF_FULL_STATS
	uint64_t write_ns_tmp_ = 0;
	uint64_t conversion_ns_ = 0;
#endif

	void flush_() {
		if (!active_ || buffer_.empty())
			return;
		size_t n = buffer_.size();
		const char *d = buffer_.data();
		while (n) {
			size_t w = std::fwrite(d, 1, n, file_);
			if (w == 0)
				throw diagnostics::Error("IO error: Could not flush to disk.");
			d += w;
			n -= w;
		}
		buffer_.clear();
	}

	void append_(std::string_view sv) {
		buffer_.append(sv.data(), sv.size());
		if (buffer_.size() >= threshold_)
			flush_();
	}
};

} // namespace writer
