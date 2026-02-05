module;

#include "third_party/cxxopts/cxxopts.hpp"
#include "util/diagnostics.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

export module runtime;

import util;
import field_transforms;
import storage;

using namespace util::strings;
using namespace util::misc;

namespace runtime {

export class Settings {
  private:
	// default parameters, user-overridable via command line
	const double READ_CHUNK_SIZE_DEFAULT_ = 80.0;
	const double WRITE_CHUNK_SIZE_DEFAULT_ = 80.0;
	const double STORAGE_BUFFER_SIZE_DEFAULT_ = 500.0;

	// settings values
	const size_t PRE_RUN_SAMPLE_SIZE_ = 100; // number of rows to sample per file in pre-run mode

  public:
	// checks validity of command line arguments and sets settings accordingly
	Settings(int argc, char* argv[]) { // NOLINT(modernize-avoid-c-arrays)
		cxxopts::Options opts("gtfs2rdf",
		                      "Gtfs->Rdf converter\n"
		                      "Note: garbage in, garbage out. Validate your Gtfs feed first.\n"
		                      "A pre-run is recommended to validate schemas and estimate "
		                      "requirements before conversion.\n"
		                      "\n"
		                      "Examples:\n"
		                      "  gtfs2rdf feed.zip --format nt\n"
		                      "  gtfs2rdf --feed feed.zip --output out/\n");

		opts.custom_help("[options]");

		opts.add_options("Standard")(
		    "f,feed", "Path to Gtfs .zip archive", cxxopts::value<std::string>())(
		    "o,output", "Output directory", cxxopts::value<std::string>()->default_value("."))(
		    "p,pre-run",
		    "Validate schemas, print file headers, and roughly estimate output size and estimated "
		    "peak RAM usage without writing output. The level of statistics output determines how "
		    "much information is provided. It is recommended to run this in combination "
		    "with --spec-dump to get conversion details before actual conversion.",
		    cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
		    "format", "Output format: ttl|nt", cxxopts::value<std::string>()->default_value("ttl"))(
		    "overwrite",
		    "Overwrite existing output files.",
		    cxxopts::value<bool>()->default_value("false")->implicit_value("true"))("h,help",
		                                                                            "Show help");

		opts.add_options("RAM / runtime")(
		    "read-buffer-size",
		    "Read buffer size in MB (bigger = more RAM, fewer reads)",
		    cxxopts::value<double>()->default_value(std::to_string(READ_CHUNK_SIZE_DEFAULT_)))(
		    "write-buffer-size",
		    "Write buffer size in MB (bigger = more RAM, fewer flushes)",
		    cxxopts::value<double>()->default_value(std::to_string(WRITE_CHUNK_SIZE_DEFAULT_)))(
		    "storage-buffer-size",
		    "Size in MB of RAM that may be allocated for persistent storage cache between Gtfs "
		    "files (bigger = more RAM, might be faster)",
		    cxxopts::value<double>()->default_value(std::to_string(STORAGE_BUFFER_SIZE_DEFAULT_)));

		opts.add_options("Diagnostics / advanced")(
		    "spec-dump",
		    "Dump ontology spec to disk",
		    cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
		    "warning-level",
		    "Warning verbosity: quiet|warning|info",
		    cxxopts::value<std::string>()->default_value("quiet"))(
		    "stats",
		    "Statistics output: quiet (none), brief (overview), verbose (overview per-file)",
		    cxxopts::value<std::string>()->default_value("quiet"));

		opts.parse_positional({"feed"});
		opts.positional_help("Gtfs_ZIP");

		auto result = opts.parse(argc, argv);

		if (result.count("help")) {
			std::cerr << opts.help({"Standard", "RAM / runtime", "Diagnostics / advanced"}) << "\n";
			std::exit(0);
		}

		if (!result.count("feed")) {
			std::cerr << "Input Gtfs feed required. Type --help for more information!\n";
			std::exit(1);
		}

		// pre-run
		pre_run_ = result["pre-run"].as<bool>();

		// output format
		std::string format = result["format"].as<std::string>();
		if (format != "ttl" && format != "nt") {
			throw diagnostics::Error("Settings error: unsupported output format '" + format +
			                         "'. "
			                         "Supported formats are 'ttl' and 'nt'.\n");
		}
		ntriples_output_ = (format == "nt");

		spec_dump_ = result["spec-dump"].as<bool>();

		// read buffer size
		read_buffer_size_mb_ = result["read-buffer-size"].as<double>();
		if (read_buffer_size_mb_ <= 0.0) {
			std::cerr << "⚠️  Read buffer size must be positive. Using default value of "
			          << READ_CHUNK_SIZE_DEFAULT_ << " mb.\n";
			read_buffer_size_mb_ = READ_CHUNK_SIZE_DEFAULT_;
		}

		// write buffer size
		write_buffer_size_mb_ = result["write-buffer-size"].as<double>();
		if (write_buffer_size_mb_ <= 0.0) {
			std::cerr << "⚠️  Write buffer size must be positive. Using default value of "
			          << WRITE_CHUNK_SIZE_DEFAULT_ << " mb.\n";
			write_buffer_size_mb_ = WRITE_CHUNK_SIZE_DEFAULT_;
		}

		// storage buffer size
		storage_buffer_size_mb_ = result["storage-buffer-size"].as<double>();
		if (storage_buffer_size_mb_ < 0.0) {
			std::cerr << "⚠️  Storage buffer size must be non-negative. Using default value of "
			          << STORAGE_BUFFER_SIZE_DEFAULT_ << " mb.\n";
			storage_buffer_size_mb_ = STORAGE_BUFFER_SIZE_DEFAULT_;
		}

		// input path
		input_path_ = result["feed"].as<std::string>();
		if (!std::filesystem::is_regular_file(input_path_) || input_path_.extension() != ".zip") {
			throw diagnostics::Error("Settings error: input '" + input_path_.string() +
			                         "' doesn't exist or "
			                         "is not a zip file.\n");
		}

		// output path
		std::string file_ext = ntriples_output_ ? ".nt" : ".ttl";
		output_path_ =
		    result["output"].as<std::string>() + "/" + input_path_.stem().string() + file_ext;

		overwrite_output_ = result["overwrite"].as<bool>();

		// warning level
		std::string warning_level = result["warning-level"].as<std::string>();
		if (warning_level == "quiet") {
			warning_verbosity_ = diagnostics::VerbosityLevelWarnings::QUIET;
		} else if (warning_level == "warning") {
			warning_verbosity_ = diagnostics::VerbosityLevelWarnings::WARNING;
		} else if (warning_level == "info") {
			warning_verbosity_ = diagnostics::VerbosityLevelWarnings::INFO;
		} else {
			throw diagnostics::Error("Settings error: unsupported warning level '" + warning_level +
			                         "'. Supported levels are 'quiet', 'warning', and 'info'.\n");
		}

		// statistics output level
		std::string stats_level = result["stats"].as<std::string>();
		if (stats_level == "quiet") {
			stats_verbosity_ = diagnostics::VerbosityLevelStats::QUIET;
		} else if (stats_level == "brief") {
			stats_verbosity_ = diagnostics::VerbosityLevelStats::BRIEF;
		} else if (stats_level == "verbose") {
			stats_verbosity_ = diagnostics::VerbosityLevelStats::VERBOSE;
		} else {
			throw diagnostics::Error("Settings error: unsupported statistics level '" +
			                         stats_level +
			                         "'. Supported levels are 'quiet', 'brief', and 'verbose'.\n");
		}
	}

	// GETTERs
	[[nodiscard]] bool isPreRun() const {
		return pre_run_;
	}
	[[nodiscard]] bool isNTriplesOutput() const {
		return ntriples_output_;
	}
	[[nodiscard]] bool isSpecDump() const {
		return spec_dump_;
	}
	[[nodiscard]] double getReadBufferSize_MB() const {
		return read_buffer_size_mb_;
	}
	[[nodiscard]] double getWriteBufferSize_MB() const {
		return write_buffer_size_mb_;
	}
	[[nodiscard]] double getStorageBufferSize_MB() const {
		return storage_buffer_size_mb_;
	}
	[[nodiscard]] double getEstimatedPeakRAM_MB() const {
		return 2 * (read_buffer_size_mb_ + write_buffer_size_mb_) + storage_buffer_size_mb_;
	}
	[[nodiscard]] bool isOverwriteOutput() const {
		return overwrite_output_;
	}
	[[nodiscard]] const std::filesystem::path& getInputPath() const {
		return input_path_;
	}
	[[nodiscard]] const std::filesystem::path& getOutputPath() const {
		return output_path_;
	}
	[[nodiscard]] size_t getPreRunSampleSize() const {
		return PRE_RUN_SAMPLE_SIZE_;
	}
	[[nodiscard]] diagnostics::VerbosityLevelWarnings getWarningsVerbosity() const {
		return warning_verbosity_;
	}
	[[nodiscard]] diagnostics::VerbosityLevelStats getStatsVerbosity() const {
		return stats_verbosity_;
	}

  private:
	bool ntriples_output_;
	bool spec_dump_;
	double read_buffer_size_mb_;
	double write_buffer_size_mb_;
	double storage_buffer_size_mb_;
	bool overwrite_output_;
	bool pre_run_;
	std::filesystem::path input_path_;
	std::filesystem::path output_path_;
	diagnostics::VerbosityLevelWarnings warning_verbosity_;
	diagnostics::VerbosityLevelStats stats_verbosity_;
};

// container for runtime settings, transform registry, and persistent storage across Gtfs files
export class RuntimeContainer {
  public:
	RuntimeContainer(const Settings& settings, field_transforms::TransformRegistry& registry)
	    : settings_(settings)
	    , registry_(registry)
	    , warning_collector_(settings.getWarningsVerbosity())
	    , storage_(warning_collector_,
	               settings.getStatsVerbosity(),
	               settings.getStorageBufferSize_MB()) {
	}

	const Settings& getSettings() const {
		return settings_;
	}
	field_transforms::TransformRegistry& getTransformRegistry() {
		return registry_;
	}
	const field_transforms::TransformRegistry& getConstTransformRegistry() const {
		return registry_;
	}
	storage::PersistentStorageSqlite& getStorage() {
		return storage_;
	}
	diagnostics::WarningCollector& getWarningCollector() {
		return warning_collector_;
	}

  private:
	const Settings& settings_;
	field_transforms::TransformRegistry& registry_;
	diagnostics::WarningCollector warning_collector_;
	storage::PersistentStorageSqlite storage_;
};
} // namespace runtime