module;

#include <iostream>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <stdexcept>
#include <cstdlib>
#include "util/cxxopts.hpp"

export module runtime;

import field_transforms;

namespace runtime {

export class Settings {
  private:
    // default parameters
    const double READ_CHUNK_SIZE_DEFAULT = 10.0;
    const double WRITE_CHUNK_SIZE_DEFAULT = 20.0;


    bool ntriples_output_;
    bool syntactic_sugar_;
    bool debug_warnings_;
    bool spec_dump_;
    double read_chunk_size_mb_;
    double write_chunk_size_mb_;

    std::filesystem::path inputPath_;
    std::filesystem::path outputPath_;
    

  public:
    Settings(bool ntriples_output, bool syntactic_sugar,
             bool debug_warnings, bool spec_dump, double read_chunk_size_mb)
        : ntriples_output_(ntriples_output), syntactic_sugar_(syntactic_sugar),
          debug_warnings_(debug_warnings), spec_dump_(spec_dump), read_chunk_size_mb_(read_chunk_size_mb) {
        if (ntriples_output_ && syntactic_sugar_) {
            std::cerr << "⚠️  Warning: cannot use turtle syntactic sugar in ntriples output. Disabling syntactic sugar.\n";
            syntactic_sugar_ = false;
        }

        if (read_chunk_size_mb <= 0.0) {
            std::cerr << "⚠️  Warning: batch size must be positive. Using default value of 10.0 mb.\n";
        }
    }

    // checks validity of command line arguments and sets settings accordingly
    Settings(int argc, char* argv[]) {
        cxxopts::Options opts("gtfs2rdf", "GTFS->RDF converter");
        opts.add_options()
            ("d,dataset", "Path to GTFS .zip archive", cxxopts::value<std::string>())
            ("o,output",  "Output directory", cxxopts::value<std::string>()->default_value("."))
            ("read-chunk-size", "Read chunk size in mb", cxxopts::value<double>()->default_value(std::to_string(READ_CHUNK_SIZE_DEFAULT)))
            ("write-chunk-size", "Write chunk size in mb", cxxopts::value<double>()->default_value(std::to_string(WRITE_CHUNK_SIZE_DEFAULT)))
            ("t,triple", "Store as fully resolved triples, without prefixes or other .ttl syntax", 
            cxxopts::value<bool>()->default_value("false"))  // TODO
            ("s,syntactic-sugar", "Enable syntactic .ttl sugar for a more compact file output", 
            cxxopts::value<bool>()->default_value("false"))  // TODO
            ("L,spec-dump", "Dump onthology spec to disk",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))  
            ("w,debug", "Show non-fatal warnings", cxxopts::value<bool>()->default_value("true"))  // TODO
            ("h,help", "Show help");

        opts.positional_help("GTFS_ZIP");
        opts.parse_positional({"dataset"});

        auto result = opts.parse(argc, argv);
        if (result.count("help")) { std::cout << opts.help() << '\n'; exit(0); }
        if (!result.count("dataset")) { 
            std::cerr << "Input GTFS dataset required. Type --help for "\
                        "more information!\n"; exit(0); 
        }

        ntriples_output_ = result["triple"].as<bool>();
        syntactic_sugar_ = result["syntactic-sugar"].as<bool>();
        if (ntriples_output_ && syntactic_sugar_) {
            std::cerr << "⚠️  Warning: cannot use turtle syntactic sugar in ntriples output. Disabling syntactic sugar.\n";
            syntactic_sugar_ = false;
        }

        debug_warnings_ = result["debug"].as<bool>();
        spec_dump_ = result["spec-dump"].as<bool>();

        // read chunk size
        read_chunk_size_mb_ = result["read-chunk-size"].as<double>();
        if (read_chunk_size_mb_ <= 0.0) {
            std::cerr << "⚠️  Warning: read chunk size must be positive. Using default value of " << READ_CHUNK_SIZE_DEFAULT << " mb.\n";
            read_chunk_size_mb_ = READ_CHUNK_SIZE_DEFAULT;
        }

        // write chunk size
        write_chunk_size_mb_ = result["write-chunk-size"].as<double>();
        if (read_chunk_size_mb_ <= 0.0) {
            std::cerr << "⚠️  Warning: write chunk size must be positive. Using default value of " << WRITE_CHUNK_SIZE_DEFAULT << " mb.\n";
            write_chunk_size_mb_ = WRITE_CHUNK_SIZE_DEFAULT;
        }

        // input path       
        inputPath_ = result["dataset"].as<std::string>();
        if (!std::filesystem::is_regular_file(inputPath_) || inputPath_.extension() != ".zip") {
            throw std::runtime_error("❌  Error: Input '" + inputPath_.string() + "' doesn't exist or "\
                                                                   "is not a zip file.\n");
        }

        // output path
        std::string file_ext = ntriples_output_ ? ".nt" : ".ttl";
        outputPath_ = result["output"].as<std::string>() + "/" + inputPath_.stem().string() + file_ext;
        // validate output path (and avoid unwanted overwriting)
        if (std::filesystem::exists(outputPath_)) {
            std::cout << "Output path '" << outputPath_.string() << "' already exists. Overwrite? [y/N]\n";
            std::string a;
            std::getline(std::cin, a);
            if (!(a == "y" || a == "Y" || a == "yes" || a == "YES")) exit(1);
        }       
    }

    // GETTERs
    bool isNTriplesOutput() const { return ntriples_output_; }
    bool isSyntacticSugar() const { return syntactic_sugar_; }
    bool isDebugWarnings() const { return debug_warnings_; }
    bool isSpecDump() const { return spec_dump_; }
    double ReadChunkSizeMB() const { return read_chunk_size_mb_; }
    double WriteChunkSizeMB() const { return write_chunk_size_mb_; }
    const std::filesystem::path& InputPath() const { return inputPath_; }
    const std::filesystem::path& OutputPath() const { return outputPath_; }
};

export class RuntimeContainer {
  private:
    const Settings& settings_;
    field_transforms::TransformRegistry& registry_;

  public:
    RuntimeContainer(const Settings& settings, field_transforms::TransformRegistry& registry)
        : settings_(settings), registry_(registry) {}

    const Settings& getSettings() const { return settings_; }
    field_transforms::TransformRegistry& getTransformRegistry() { return registry_; }
};

export class Statistics {
  public:
    uint32_t chunks = 0;
    uint64_t rows = 0;
    uint64_t triples = 0;
    double parse_s = 0.;
    double write_s = 0.;

    std::string fancyPrint(const std::string& name) {
        std::ostringstream oss;
        oss << "\n📊 Statistics for " << name << ":\n"
          << "  Chunks processed:  " << chunks << "\n"
          << "  Rows parsed:       " << rows << "\n"
          << "  Triples generated: " << triples << "\n"
          << "  Parse time:        " << std::fixed << std::setprecision(2) << parse_s << "s\n"
          << "  Write time:        " << std::fixed << std::setprecision(2) << write_s << "s\n"
          << "  Total time:        " << std::fixed << std::setprecision(2) << (parse_s + write_s) << "s\n";
        return oss.str();
    }

    std::string briefPrint(const std::string& name) {
        std::ostringstream oss;
        oss << "📈 " << name << ": " << rows << " rows, " << triples << " triples, "
          << std::fixed << std::setprecision(2) << parse_s << "s parse + " << write_s << "s write";
        return oss.str();
    }

    Statistics operator+(const Statistics& other) const {
        Statistics result;
        result.chunks = chunks + other.chunks;
        result.rows = rows + other.rows;
        result.triples = triples + other.triples;
        result.parse_s = parse_s + other.parse_s;
        result.write_s = write_s + other.write_s;
        return result;
    }

};


} // namespace