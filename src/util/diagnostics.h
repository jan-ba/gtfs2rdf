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

namespace diagnostics {

// _________________________________________________________________________________________________
// custom error type for diagnostic errors
struct Error : std::runtime_error {
	explicit Error(const std::string& message)
	    : std::runtime_error(message) {
	}
};

// helper function to wrap and rethrow exceptions with additional context
// unused parameter 'e' to ensure function is only called with custom Error type
inline void wrapAndRethrow([[maybe_unused]] const diagnostics::Error& err,
                           std::string_view context_message) {
	std::throw_with_nested(Error(std::string(context_message)));
}

// helper function to print error chain in order 'innermost -> outermost'
inline void printErrorChain(const diagnostics::Error& err) {
	bool printed_context = false;

	auto recursion = [&](auto&& self, const diagnostics::Error& err) -> void {
		try {
			std::rethrow_if_nested(err);
		} catch (const diagnostics::Error& inner) {
			self(self, inner); // print leaf first

			if (!printed_context) {
				std::cerr << "Context stack:\n";
				printed_context = true;
			}
			std::cerr << "  - " << err.what() << "\n";
			return;
		}
		// leaf
		std::cerr << "❌  " << err.what() << "\n";
	};

	recursion(recursion, err);
}

// _________________________________________________________________________________________________
// warnings / logging

// to be assigned to a Warning in code
// beware that the ordering matters for proper filtering based on verbosity level, such that
// less severe < more severe
enum class WarningLevel : uint8_t { INFO, WARNING };

// beware that ordering matters for proper filtering based on verbosity level, such that
// more verbose < less verbose
enum class VerbosityLevelWarnings : uint8_t { INFO, WARNING, QUIET };

class Warning {
  public:
	explicit Warning(std::string_view message, WarningLevel level, bool closed = false)
	    : level_(level)
	    , closed_(closed)
	    , message_(message) {
	}

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
		case WarningLevel::INFO:
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
	}

  private:
	std::vector<Warning> warnings_;
	std::unordered_set<size_t> unclosed_warning_indices_;
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

	// pre-run only
	std::vector<std::string> header;
	uint64_t num_chars = 0;

	Statistics() = default;

	[[nodiscard]] std::string fancyPrint() const {
		std::ostringstream oss;

		oss << "\n--------------------------------------------------------------------\n";
		oss << "📊 Statistics for " << name << "\n\n";

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
		    << formatValueWithPaddedUnits(parse_ns + write_ns + conversion_ns, UnitType::TIME)
		    << "\n";
#endif

		oss << "--------------------------------------------------------------------\n";
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