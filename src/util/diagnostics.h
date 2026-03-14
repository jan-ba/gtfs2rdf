// SPDX-FileCopyrightText: 2026 Jan Babin
// SPDX-License-Identifier: AGPL-3.0-only
//
// Part of gtfs2rdf. See the LICENSE file for details.

#pragma once

#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

import util;

using namespace util::strings;

// namespace for diagnostics-related utilities: error handling, warning / logging aggregation, and
// statistics collection
namespace diagnostics {

// _________________________________________________________________________________________________
// error handling

// custom error type for diagnostic errors, specific to gtfs2rdf and meant to be used with
// wrapAndRethrow
struct Error : std::runtime_error {
	explicit Error(const std::string& message)
	    : std::runtime_error(message) {
	}
};

// helper function to wrap and rethrow exceptions with additional context
inline void wrapAndRethrow(std::string_view context_message) {
	std::throw_with_nested(Error(std::string(context_message)));
}

// helper function to print error chain in order 'innermost -> outermost', i.e. error cause is
// printed before its context
inline void printErrorChain(const std::exception& excpt) {
	bool printed_context = false;

	auto recursion = [&](auto&& self, const std::exception& excpt) -> void {
		try {
			// if this doesn't throw, then excpt is the innermost exception / the error cause ->
			// print first
			std::rethrow_if_nested(excpt);
		} catch (const std::exception& inner) {
			self(self, inner); // recurse until innermost exception is reached

			// ensures that the leaf / context split is set exactly once
			if (!printed_context) {
				std::cerr << "Context stack:\n";
				printed_context = true;
			}

			// aggregate context messages as dashed list
			std::cerr << "  - " << excpt.what() << "\n";
			return;
		} catch (...) {
			// non-standard exception, which hopefully doesn't happen
			std::cerr << "❌  NON-STD EXCEPTION: " << excpt.what() << "\n";
			return;
		}
		// leaf / error cause
		std::cerr << "❌  " << excpt.what() << "\n";
	};

	recursion(recursion, excpt);
}

// _________________________________________________________________________________________________
// warnings / logging

// severity of a warning / logging bit
// beware that the enum order matters for proper filtering based on verbosity level, such that
// less severe < more severe is to be ensured
enum class WarningLevel : uint8_t { DEBUG, WARNING };

// beware that enum order matters for proper filtering based on verbosity level, such that
// more verbose < less verbose is to be ensured
enum class VerbosityLevelWarnings : uint8_t { DEBUG, WARNING, QUIET };

// requires setting the depth of context information (i.e. the deeper in the program flow tree, the
// higher the depth value) for proper context aggregation as of now
class Warning {
  public:
	// closed: whether this warning closes the current context (subsequent context not appended to
	// this warning)
	explicit Warning(std::string_view message, WarningLevel level, bool closed = false)
	    : level_(level)
	    , closed_(closed)
	    , message_(message) {
	}

	// level: level / depth of the context in the program flow (a warning in main function could be
	// level 1)
	void addContext(std::string_view context, size_t depth = -1) {
		if (closed_ || depth_ <= depth) {
			return;
		}
		context_.emplace_back(context);
		depth_ = depth;
	}

	void appendWarningMessage(std::string_view message) {
		message_ += " " + std::string(message);
	}

	void print() const {
		if (message_.empty()) {
			return;
		}

		// print innermost message with appropriate emoji
		const char* emoji = getEmoji_(level_);
		std::cerr << emoji << message_ << "\n";

		// print contexts from innermost to outermost
		if (!context_.empty()) {
			std::cerr << "Context stack:\n";
			for (const auto& ctx : context_) {
				std::cerr << "  - " << ctx << "\n";
			}
		}
	}

  private:
	// populate as more WarningLevels are added
	static const char* getEmoji_(WarningLevel level) {
		switch (level) {
			case WarningLevel::WARNING:
				return "⚠️  ";
			case WarningLevel::DEBUG:
				return "ℹ️  ";
		}
		return "";
	}

	WarningLevel level_;
	bool closed_;
	std::string message_;
	std::vector<std::string> context_;
	size_t depth_ = -1; // min(depth(warning), depth(last context))
};

class WarningCollector {
  public:
	WarningCollector(VerbosityLevelWarnings verbosity_level)
	    : verbosity_level_(verbosity_level) {
	}

	void addLeaf(std::string_view message, WarningLevel level, bool closed = false) {
		if (static_cast<uint8_t>(level) < static_cast<uint8_t>(verbosity_level_)) {
			return;
		}
		warnings_.emplace_back(message, level, closed);
		if (!closed) {
			unclosed_warning_indices_.insert(warnings_.size() - 1);
		}
	}

	// add text the the most recently added warning
	void appendToLeaf(std::string_view message, WarningLevel level) {
		if (warnings_.empty()) {
			return;
		}
		if (static_cast<uint8_t>(level) < static_cast<uint8_t>(verbosity_level_)) {
			return;
		}
		warnings_.back().appendWarningMessage(message);
	}

	void addNode(std::string_view message, size_t depth) {
		for (const auto& idx : unclosed_warning_indices_) {
			warnings_[idx].addContext(message, depth);
		}
	}

	void printWarningSummary() const {
		for (const auto& warn : warnings_) {
			warn.print();
			std::cerr << "\n";
		}
		std::cerr << "\n";

		warnings_.clear();
		unclosed_warning_indices_.clear();
	}

  private:
	mutable std::vector<Warning> warnings_;
	mutable std::unordered_set<size_t> unclosed_warning_indices_;
	VerbosityLevelWarnings verbosity_level_;
};

// _________________________________________________________________________________________________
// Statistics

// statistics verbosity levels
enum class VerbosityLevelStats : uint8_t { QUIET, BRIEF, VERBOSE };

// container for runtime statistics collected during Gtfs processing
class Statistics {
  public:
	std::string name;
	uint32_t chunks = 0;
	uint64_t rows = 0;
	uint64_t triples = 0;

#if GTFS2RDF_FULL_STATS
	uint64_t parse_ns = 0;
	uint64_t write_ns = 0;
	uint64_t conversion_ns = 0;
#endif

	// pre-run only: stores the header fields of the corresponding Gtfs file
	std::vector<std::string> header;
	uint64_t num_chars = 0;

	Statistics() = default;

	[[nodiscard]] std::string fancyPrint() const {
		std::ostringstream oss;

		oss << "📊 Statistics for " << name << "\n";

		oss << "  Chunks processed:  " << chunks << "\n";
		oss << "  Rows parsed:       " << rows << "\n";
		oss << "  Triples generated: " << triples << "\n";

#if GTFS2RDF_FULL_STATS
		oss << "  Parse time:        " << formatValueWithPaddedUnits(parse_ns, UnitType::TIME)
		    << "\n";
		oss << "  Write time:        " << formatValueWithPaddedUnits(write_ns, UnitType::TIME)
		    << "\n";
		oss << "  Conversion time:   " << formatValueWithPaddedUnits(conversion_ns, UnitType::TIME)
		    << "\n";
		oss << "  Total time:        "
		    << formatValueWithPaddedUnits(parse_ns + write_ns + conversion_ns, UnitType::TIME);
#endif

		return oss.str();
	}

	[[nodiscard]] std::string briefPrint() const {
		std::ostringstream oss;
		oss << "📈 " << name << ": " << rows << " rows, " << triples << " triples";
#if GTFS2RDF_FULL_STATS
		oss << ", " << formatValueWithPaddedUnits(parse_ns, UnitType::TIME) << " parse + "
		    << formatValueWithPaddedUnits(write_ns, UnitType::TIME) << " write + "
		    << formatValueWithPaddedUnits(conversion_ns, UnitType::TIME) << " convert";
#endif
		return oss.str();
	}

	Statistics operator+(const Statistics& other) const {
		Statistics result = *this;
		result.chunks += other.chunks;
		result.rows += other.rows;
		result.triples += other.triples;
#if GTFS2RDF_FULL_STATS
		result.parse_ns += other.parse_ns;
		result.write_ns += other.write_ns;
		result.conversion_ns += other.conversion_ns;
#endif
		result.num_chars += other.num_chars;
		return result;
	}
};

} // namespace diagnostics