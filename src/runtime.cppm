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
	const double READ_CHUNK_SIZE_DEFAULT = 80.0;
	const double WRITE_CHUNK_SIZE_DEFAULT = 80.0;
	const double STORAGE_BUFFER_SIZE_DEFAULT = 500.0;

	// settings values
	const size_t PRE_RUN_SAMPLE_SIZE = 100; // number of rows to sample per file in pre-run mode

  public:
	// checks validity of command line arguments and sets settings accordingly
	Settings(int argc, char* argv[]) {
		cxxopts::Options opts("gtfs2rdf",
		                      "GTFS->RDF converter\n"
		                      "Note: garbage in, garbage out. Validate your GTFS feed first.\n"
		                      "A pre-run is recommended to validate schemas and estimate "
		                      "requirements before conversion.\n"
		                      "\n"
		                      "Examples:\n"
		                      "  gtfs2rdf feed.zip --format nt\n"
		                      "  gtfs2rdf --feed feed.zip --output out/\n");

		opts.custom_help("[options]");

		opts.add_options("Standard")(
		    "f,feed", "Path to GTFS .zip archive", cxxopts::value<std::string>())(
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
		    cxxopts::value<double>()->default_value(std::to_string(READ_CHUNK_SIZE_DEFAULT)))(
		    "write-buffer-size",
		    "Write buffer size in MB (bigger = more RAM, fewer flushes)",
		    cxxopts::value<double>()->default_value(std::to_string(WRITE_CHUNK_SIZE_DEFAULT)))(
		    "storage-buffer-size",
		    "Size in MB of RAM that may be allocated for persistent storage cache between GTFS "
		    "files (bigger = more RAM, might be faster)",
		    cxxopts::value<double>()->default_value(std::to_string(STORAGE_BUFFER_SIZE_DEFAULT)));

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
		opts.positional_help("GTFS_ZIP");

		auto result = opts.parse(argc, argv);

		if (result.count("help")) {
			std::cerr << opts.help({"Standard", "RAM / runtime", "Diagnostics / advanced"}) << "\n";
			std::exit(0);
		}

		if (!result.count("feed")) {
			std::cerr << "Input GTFS feed required. Type --help for more information!\n";
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
			          << READ_CHUNK_SIZE_DEFAULT << " mb.\n";
			read_buffer_size_mb_ = READ_CHUNK_SIZE_DEFAULT;
		}

		// write buffer size
		write_buffer_size_mb_ = result["write-buffer-size"].as<double>();
		if (write_buffer_size_mb_ <= 0.0) {
			std::cerr << "⚠️  Write buffer size must be positive. Using default value of "
			          << WRITE_CHUNK_SIZE_DEFAULT << " mb.\n";
			write_buffer_size_mb_ = WRITE_CHUNK_SIZE_DEFAULT;
		}

		// storage buffer size
		storage_buffer_size_mb_ = result["storage-buffer-size"].as<double>();
		if (storage_buffer_size_mb_ < 0.0) {
			std::cerr << "⚠️  Storage buffer size must be non-negative. Using default value of "
			          << STORAGE_BUFFER_SIZE_DEFAULT << " mb.\n";
			storage_buffer_size_mb_ = STORAGE_BUFFER_SIZE_DEFAULT;
		}

		// input path
		inputPath_ = result["feed"].as<std::string>();
		if (!std::filesystem::is_regular_file(inputPath_) || inputPath_.extension() != ".zip") {
			throw diagnostics::Error("Settings error: input '" + inputPath_.string() +
			                         "' doesn't exist or "
			                         "is not a zip file.\n");
		}

		// output path
		std::string file_ext = ntriples_output_ ? ".nt" : ".ttl";
		outputPath_ =
		    result["output"].as<std::string>() + "/" + inputPath_.stem().string() + file_ext;

		overwrite_output_ = result["overwrite"].as<bool>();

		// warning level
		std::string warning_level = result["warning-level"].as<std::string>();
		if (warning_level == "quiet") {
			warning_verbosity_ = diagnostics::VerbosityLevelWarnings::Quiet;
		} else if (warning_level == "warning") {
			warning_verbosity_ = diagnostics::VerbosityLevelWarnings::Warning;
		} else if (warning_level == "info") {
			warning_verbosity_ = diagnostics::VerbosityLevelWarnings::Info;
		} else {
			throw diagnostics::Error("Settings error: unsupported warning level '" + warning_level +
			                         "'. Supported levels are 'quiet', 'warning', and 'info'.\n");
		}

		// statistics output level
		std::string stats_level = result["stats"].as<std::string>();
		if (stats_level == "quiet") {
			stats_verbosity_ = diagnostics::VerbosityLevelStats::Quiet;
		} else if (stats_level == "brief") {
			stats_verbosity_ = diagnostics::VerbosityLevelStats::Brief;
		} else if (stats_level == "verbose") {
			stats_verbosity_ = diagnostics::VerbosityLevelStats::Verbose;
		} else {
			throw diagnostics::Error("Settings error: unsupported statistics level '" +
			                         stats_level +
			                         "'. Supported levels are 'quiet', 'brief', and 'verbose'.\n");
		}
	}

	// GETTERs
	bool isPreRun() const {
		return pre_run_;
	}
	bool isNTriplesOutput() const {
		return ntriples_output_;
	}
	bool isSpecDump() const {
		return spec_dump_;
	}
	double ReadBufferSizeMB() const {
		return read_buffer_size_mb_;
	}
	double WriteBufferSizeMB() const {
		return write_buffer_size_mb_;
	}
	double StorageBufferSizeMB() const {
		return storage_buffer_size_mb_;
	}
	double estimatedPeakRAMMB() const {
		return 2 * (read_buffer_size_mb_ + write_buffer_size_mb_) + storage_buffer_size_mb_;
	}
	bool isOverwriteOutput() const {
		return overwrite_output_;
	}
	const std::filesystem::path& InputPath() const {
		return inputPath_;
	}
	const std::filesystem::path& OutputPath() const {
		return outputPath_;
	}
	size_t getPreRunSampleSize() const {
		return PRE_RUN_SAMPLE_SIZE;
	}
	diagnostics::VerbosityLevelWarnings getWarningsVerbosity() const {
		return warning_verbosity_;
	}
	diagnostics::VerbosityLevelStats getStatsVerbosity() const {
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
	std::filesystem::path inputPath_;
	std::filesystem::path outputPath_;
	diagnostics::VerbosityLevelWarnings warning_verbosity_;
	diagnostics::VerbosityLevelStats stats_verbosity_;
};

// container for runtime settings, transform registry, and persistent storage across GTFS files
export class RuntimeContainer {
  public:
	RuntimeContainer(const Settings& settings, field_transforms::TransformRegistry& registry)
	    : settings_(settings)
	    , registry_(registry)
	    , warning_collector_(settings.getWarningsVerbosity())
	    , storage_(
	          warning_collector_, settings.getStatsVerbosity(), settings.StorageBufferSizeMB()) {
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