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
	Writer(const std::filesystem::path& path,
	       runtime::RuntimeContainer& rtc,
	       double buffer_size_mb,
	       bool active = true)
	    : rtc_(rtc)
	    // NOLINT(readability-magic-numbers, bugprone-narrowing-conversions)
	    , THRESHOLD_(buffer_size_mb * 1024 * 1024)
	    , ACTIVE_(active) {
		if (ACTIVE_ && std::filesystem::exists(path)) {
			if (!rtc_.getSettings().isOverwriteOutput()) {
				throw diagnostics::Error(
				    "IO error: output file '" + path.string() +
				    "' already exists. To overwrite, enable the overwrite option.");
			}
		}
		if (ACTIVE_) {
			file_ = std::fopen(path.string().c_str(), "wb");
			file_path_ = path;
		} else {
			file_ = nullptr; // discard output
		}
		if (ACTIVE_ && !file_) {
			throw diagnostics::Error("IO error: Cannot open '" + path.string() + "' for writing.");
		}
		buffer_.reserve(THRESHOLD_);
	}

	// overload for writing to an already open stream (e.g. stdout)
	Writer(std::ostream& out_stream,
	       runtime::RuntimeContainer& rtc,
	       double buffer_size_mb,
	       bool active = true)
	    : out_stream_(&out_stream)
	    , rtc_(rtc)
	    , THRESHOLD_(buffer_size_mb * 1024 * 1024)
	    , ACTIVE_(active) {
		file_ = nullptr; // not used in this mode
		buffer_.reserve(THRESHOLD_);
	}

	// convenience overload with default buffer size
	Writer(const std::filesystem::path& path, runtime::RuntimeContainer& rtc, bool active = true)
	    : Writer(path, rtc, rtc.getSettings().getWriteBufferSize_MB(), active) {
	}

	// convenience overload with default buffer size for stream output
	Writer(std::ostream& out_stream, runtime::RuntimeContainer& rtc, bool active = true)
	    : Writer(out_stream, rtc, rtc.getSettings().getWriteBufferSize_MB(), active) {
	}

	void writePrefixes(const std::unordered_map<std::string, std::string>& map) {
		SCOPED_TIMER_NS(write_ns_tmp_);

		std::string out;
		out.reserve(map.size() * 20); // NOLINT(readability-magic-numbers) rough estimate
		for (const auto& [pfx, iri] : map) {
			out.append("@prefix ").append(pfx).append(": <").append(iri).append("> .\n");
		}
		out.append("\n");
		append_(out);
	}

	void writeRaw(std::string_view svw) {
		SCOPED_TIMER_NS(write_ns_tmp_);

		append_(svw);
	}

	// converts gtfs row according to given schema and appends to buffer
	void convertRow(Schema& sch, const std::vector<std::string>& row) {
		SCOPED_TIMER_NS(write_ns_tmp_);

		auto& instructions = sch.getInstructions();

		for (auto& instr : instructions) {
			try {
				std::string_view rendered;
				{ // timer scope
					SCOPED_TIMER_NS(conversion_ns_);
					rendered = instr.render(row);
				}
				append_(rendered);
			} catch (const std::exception& excpt) {
				diagnostics::wrapAndRethrow("while rendering instruction '" +
				                            instr.getRawInstruction() + "' in schema '" +
				                            sch.getName() + "'");
			}
		}
	}

	void deleteFile() {
		if (ACTIVE_) {
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

	Writer(const Writer&) = delete;
	Writer& operator=(const Writer&) = delete;

	Writer(Writer&&) = delete;
	Writer& operator=(Writer&&) = delete;

	~Writer() {
		flush_();
		if (ACTIVE_ && file_) {
			std::fclose(file_);
		}
	}

  private:
	// _____________________________________________________________________________________________
	// private helper methods
	// _____________________________________________________________________________________________

	void flush_() {
		if (!ACTIVE_ || buffer_.empty()) {
			return;
		}

		if (file_) {
			size_t len = buffer_.size();
			const char* data = buffer_.data();
			while (len) {
				size_t len_written = std::fwrite(data, 1, len, file_);
				if (len_written == 0) {
					throw diagnostics::Error("IO error: Could not flush to disk.");
				}
				data += len_written;
				len -= len_written;
			}
			buffer_.clear();
		} else if (out_stream_) {
			out_stream_->write(buffer_.data(), buffer_.size());
			buffer_.clear();
			if (!out_stream_->good()) {
				throw diagnostics::Error("IO error: Could not write to output stream.");
			}
		} else {
			throw diagnostics::Error("Internal error: No valid output target for Writer.");
		}
	}

	void append_(std::string_view svw) {
		buffer_.append(svw.data(), svw.size());
		if (buffer_.size() >= THRESHOLD_) {
			flush_();
		}
	}

	// _____________________________________________________________________________________________
	// private members
	// _____________________________________________________________________________________________

	std::filesystem::path file_path_;
	std::FILE* file_;
	std::ostream* out_stream_ = nullptr; // if set, write to this stream instead of file
	std::string buffer_;
	const runtime::RuntimeContainer& rtc_;
	const size_t THRESHOLD_;
	const bool ACTIVE_ = true; // whether this writer should actually write (or discard) data

#if GTFS2RDF_FULL_STATS
	uint64_t write_ns_tmp_ = 0;
	uint64_t conversion_ns_ = 0;
#endif
};

} // namespace writer
