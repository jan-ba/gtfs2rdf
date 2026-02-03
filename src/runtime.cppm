module;

#include "third_party/cxxopts/cxxopts.hpp"
#include "util/diagnostics.h"

#include <algorithm>
#include <cstdint>
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
	Settings(int argc, char *argv[]) {
		cxxopts::Options opts("gtfs2rdf",
		                      "GTFS->RDF converter\n"
		                      "Note: garbage in, garbage out. Validate your GTFS feed first.\n"
		                      "A pre-run is recommended to validate schemas and estimate "
		                      "requirements before conversion.\n"
		                      "\n"
		                      "Examples:\n"
		                      "  gtfs2rdf feed.zip --format nt\n"
		                      "  gtfs2rdf --dataset feed.zip --output out/\n");

		opts.custom_help("[options]");

		opts.add_options("Standard")(
		    "d,dataset", "Path to GTFS .zip archive", cxxopts::value<std::string>())(
		    "o,output", "Output directory", cxxopts::value<std::string>()->default_value("."))(
		    "pre-run",
		    "Validate schemas, print file headers, and roughly estimate output size and estimated "
		    "peak RAM usage without writing output. It is recommended to run this in combination "
		    "with --spec-dump to get conversion details before actual conversion.",
		    cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
		    "format", "Output format: ttl|nt", cxxopts::value<std::string>()->default_value("ttl"))(
		    "overwrite",
		    "Overwrite existing output files.",
		    cxxopts::value<bool>()->default_value("false")->implicit_value("true"))("h,help",
		                                                                            "Show help");

		opts.add_options("RAM / runtime")(
		    "read-buffer-size",
		    "Read buffer size in MB (bigger = more RAM, often faster)",
		    cxxopts::value<double>()->default_value(std::to_string(READ_CHUNK_SIZE_DEFAULT)))(
		    "write-buffer-size",
		    "Write buffer size in MB (bigger = more RAM, fewer flushes)",
		    cxxopts::value<double>()->default_value(std::to_string(WRITE_CHUNK_SIZE_DEFAULT)))(
		    "storage-buffer-size",
		    "Size in MB of RAM that may be allocated for persistent storage cache between GTFS "
		    "files (more = faster, but more RAM)",
		    cxxopts::value<double>()->default_value(std::to_string(STORAGE_BUFFER_SIZE_DEFAULT)));

		opts.add_options("Diagnostics / advanced")(
		    "spec-dump",
		    "Dump ontology spec to disk",
		    cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
		    "warning-level",
		    "Warning verbosity: quiet|default|verbose",
		    cxxopts::value<std::string>()->default_value("quiet"))(
		    "strict",
		    "Fail fast: treat warnings/issues as errors.",
		    cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
		    "stats",
		    "Statistics mode: none|verbose|dry-verbose",
		    cxxopts::value<std::string>()->default_value("none")->implicit_value("verbose"));

		opts.parse_positional({"dataset"});
		opts.positional_help("GTFS_ZIP");

		auto result = opts.parse(argc, argv);

		if (result.count("help")) {
			std::cout << opts.help({"Standard", "RAM / runtime", "Diagnostics / advanced"}) << "\n";
			std::exit(0);
		}

		if (!result.count("dataset")) {
			std::cerr << "Input GTFS dataset required. Type --help for more information!\n";
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
			std::cerr << "⚠️  Warning: read buffer size must be positive. Using default value of "
			          << READ_CHUNK_SIZE_DEFAULT << " mb.\n";
			read_buffer_size_mb_ = READ_CHUNK_SIZE_DEFAULT;
		}

		// write buffer size
		write_buffer_size_mb_ = result["write-buffer-size"].as<double>();
		if (write_buffer_size_mb_ <= 0.0) {
			std::cerr << "⚠️  Warning: write buffer size must be positive. Using default value of "
			          << WRITE_CHUNK_SIZE_DEFAULT << " mb.\n";
			write_buffer_size_mb_ = WRITE_CHUNK_SIZE_DEFAULT;
		}

		// storage buffer size
		storage_buffer_size_mb_ = result["storage-buffer-size"].as<double>();
		if (storage_buffer_size_mb_ < 0.0) {
			std::cerr
			    << "⚠️  Warning: storage buffer size must be non-negative. Using default value of "
			    << STORAGE_BUFFER_SIZE_DEFAULT << " mb.\n";
			storage_buffer_size_mb_ = STORAGE_BUFFER_SIZE_DEFAULT;
		}

		// input path
		inputPath_ = result["dataset"].as<std::string>();
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
	}

	// GETTERs
	bool isPreRun() const {
		return pre_run_;
	}

	bool isNTriplesOutput() const {
		return ntriples_output_;
	}
	bool isDebugWarnings() const {
		return debug_warnings_;
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
	const std::filesystem::path &InputPath() const {
		return inputPath_;
	}
	const std::filesystem::path &OutputPath() const {
		return outputPath_;
	}
	size_t getPreRunSampleSize() const {
		return PRE_RUN_SAMPLE_SIZE;
	}

  private:
	bool ntriples_output_;
	bool debug_warnings_;
	bool spec_dump_;
	double read_buffer_size_mb_;
	double write_buffer_size_mb_;
	double storage_buffer_size_mb_;
	bool overwrite_output_;
	bool pre_run_;

	std::filesystem::path inputPath_;
	std::filesystem::path outputPath_;
};

// container for runtime settings, transform registry, and persistent storage across GTFS files
export class RuntimeContainer {
  public:
	RuntimeContainer(const Settings &settings, field_transforms::TransformRegistry &registry)
	    : settings_(settings)
	    , registry_(registry)
	    , storage_(settings.StorageBufferSizeMB()) {
	}

	const Settings &getSettings() const {
		return settings_;
	}
	field_transforms::TransformRegistry &getTransformRegistry() {
		return registry_;
	}
	const field_transforms::TransformRegistry &getConstTransformRegistry() const {
		return registry_;
	}
	storage::PersistentStorageSqlite &getStorage() {
		return storage_;
	}

  private:
	const Settings &settings_;
	field_transforms::TransformRegistry &registry_;
	storage::PersistentStorageSqlite storage_;
};

// container for runtime statistics collected during GTFS processing
export class Statistics {
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

	std::string fancyPrint() const {
		std::ostringstream oss;

		oss << "\n--------------------------------------------------------------------\n";
		oss << "📊 Statistics for " << name << "\n\n";

		oss << "  Chunks processed:  " << chunks << "\n";
		oss << "  Rows parsed:       " << rows << "\n";
		oss << "  Triples generated: " << triples << "\n";

#if GTFS2RDF_FULL_STATS
		oss << "  Parse time:        " << fmt_suffix_padded(parse_ns, UnitType::Time) << "s\n";
		oss << "  Write time:        " << fmt_suffix_padded(write_ns, UnitType::Time) << "s\n";
		oss << "  Conversion time:   " << fmt_suffix_padded(conversion_ns, UnitType::Time) << "s\n";
		oss << "  Total time:        "
		    << fmt_suffix_padded(parse_ns + write_ns + conversion_ns, UnitType::Time) << "s\n";
#endif

		oss << "--------------------------------------------------------------------\n";
		return oss.str();
	}

	std::string briefPrint() const {
		std::ostringstream oss;
		oss << "📈 " << name << ": " << rows << " rows, " << triples << " triples";
#if GTFS2RDF_FULL_STATS
		oss << ", " << fmt_suffix_padded(parse_ns, UnitType::Time) << " parse + "
		    << fmt_suffix_padded(write_ns, UnitType::Time) << " write + "
		    << fmt_suffix_padded(conversion_ns, UnitType::Time) << " convert";
#endif
		return oss.str();
	}

	Statistics operator+(const Statistics &other) const {
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

} // namespace runtime