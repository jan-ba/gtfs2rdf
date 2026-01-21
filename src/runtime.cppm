module;

#include <iostream>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <cstdlib>
#include "util/cxxopts.hpp"

export module runtime;

import util;
import field_transforms;

namespace runtime {

// TODO: is it possible to disallow transforms to access more than get / contains, at least outside
// of their own context, if so enforce
class PersistentStorage {
  private:
    // context (this is data for one file/schema) -> (variable name -> value)
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> variables_;
    
    // context -> (multimap name -> (key -> list(values)))
    std::unordered_map<std::string, 
        std::unordered_map<std::string, 
            std::unordered_map<std::string, std::vector<std::string>>>> multimaps_;

    // context -> (tuplemap name -> (key -> list(value_tuples)))
    std::unordered_map<std::string,
        std::unordered_map<std::string,
            std::unordered_map<std::string, std::vector<std::vector<std::string>>>>> tuplemaps_;

    static inline const std::string empty_ = "";

    static inline const std::vector<std::string> empty_vector_ = {};

    static inline const std::vector<std::vector<std::string>> empty_vector_of_vectors_ = {};
  public:
    // VARIABLES API
    void store(const std::string& ctx, const std::string& variable, const std::string& value) {
        variables_[ctx][variable] = value;
    }

    const std::string& get(const std::string& ctx, const std::string& variable) const {
        auto it = variables_.find(ctx);
        if (it == variables_.end()) return empty_;
        auto it2 = it->second.find(variable);
        if (it2 == it->second.end()) return empty_;
        return it2->second;
    }

    // MULTIMAPS API
    void store(const std::string& ctx, const std::string& multimap,
               const std::string& key, const std::string& value) {
        multimaps_[ctx][multimap][key].push_back(value);
    }

    const std::vector<std::string>& get(const std::string& ctx,
                                       const std::string& multimap,
                                       const std::string& key) const {
        auto it = multimaps_.find(ctx);
        if (it == multimaps_.end()) return empty_vector_;
        auto it2 = it->second.find(multimap);
        if (it2 == it->second.end()) return empty_vector_;
        auto it3 = it2->second.find(key);
        if (it3 == it2->second.end()) return empty_vector_;
        return it3->second;
    }

    void finalise_multimaps(const std::string& ctx) {
        auto it = multimaps_.find(ctx);
        if (it == multimaps_.end()) return;
        for (auto& [ multimap_name, map ] : it->second) {
            for (auto& [ key, values ] : map) {
                std::sort(values.begin(), values.end());
            }
        }
    }

    // Check if a value exists in a multimap
    // invariant: multimaps are finalised (sorted)  TODO: enforce this
    bool contains(const std::string& ctx, const std::string& multimap,
                  const std::string& key, const std::string& value) const {
        const auto& vec = get(ctx, multimap, key);
        if (vec.empty()) return false;
        return std::binary_search(vec.begin(), vec.end(), value);
    }

    // TUPLEMAPS API
    void store(const std::string& ctx, const std::string& tuplemap,
               const std::vector<std::string>& key, const std::vector<std::string>& value_tuple) {
        tuplemaps_[ctx][tuplemap][util::concat(key)].push_back(value_tuple);
    }

    const std::vector<std::vector<std::string>>& get(const std::string& ctx,
                                       const std::string& tuplemap,
                                       const std::vector<std::string>& key) const {
        std::string key_combined = util::concat(key);
        auto it = tuplemaps_.find(ctx);
        if (it == tuplemaps_.end()) return empty_vector_of_vectors_;
        auto it2 = it->second.find(tuplemap);
        if (it2 == it->second.end()) return empty_vector_of_vectors_;
        auto it3 = it2->second.find(key_combined);
        if (it3 == it2->second.end()) return empty_vector_of_vectors_;
        return it3->second;
    }

    // clear all stored data for a given context once it goes out of scope
    void clear_context(const std::string& ctx) {
        variables_.erase(ctx);
        multimaps_.erase(ctx);
        tuplemaps_.erase(ctx);
    }

    // getters for testing
    const auto& getAllVariables() const { return variables_; }
    const auto& getAllMultimaps() const { return multimaps_; }
    const auto& getAllTuplemaps() const { return tuplemaps_; }
};

export class Settings {
  private:
    // default parameters
    const double READ_CHUNK_SIZE_DEFAULT = 10.0;
    const double WRITE_CHUNK_SIZE_DEFAULT = 20.0;


    bool ntriples_output_;
    bool debug_warnings_;
    bool spec_dump_;
    double read_chunk_size_mb_;
    double write_chunk_size_mb_;
    bool overwrite_output_;

    std::filesystem::path inputPath_;
    std::filesystem::path outputPath_;
    

  public:
    // checks validity of command line arguments and sets settings accordingly
    Settings(int argc, char* argv[]) {
        cxxopts::Options opts(
            "gtfs2rdf",
            "GTFS->RDF converter\n\n"
            "Note: garbage in, garbage out. It is recommended to validate your GTFS feed\n"
            "with a GTFS validator beforehand to ensure the conversion to RDF is also correct.\n"
        );

        opts.add_options()
            ("d,dataset", "Path to GTFS .zip archive", cxxopts::value<std::string>())
            ("o,output",  "Output directory", cxxopts::value<std::string>()->default_value("."))

            ("read-chunk-size",  "Read chunk size in mb",
                cxxopts::value<double>()->default_value(std::to_string(READ_CHUNK_SIZE_DEFAULT)))
            ("write-chunk-size", "Write chunk size in mb",
                cxxopts::value<double>()->default_value(std::to_string(WRITE_CHUNK_SIZE_DEFAULT)))

            ("format", "Output format: ttl|nt",
                cxxopts::value<std::string>()->default_value("ttl"))

            ("spec-dump", "Dump onthology spec to disk",
                cxxopts::value<bool>()->default_value("false")->implicit_value("true"))

            // NEW: warning verbosity levels
            ("warning-level", "Warning verbosity: quiet|default|verbose",
                cxxopts::value<std::string>()->default_value("quiet")) // TODO
            ("strict", "Fail fast: treat warnings/GTFS+spec issues as errors.",
                cxxopts::value<bool>()->default_value("false")->implicit_value("true")) // TODO: enforce

            ("overwrite", "Overwrite existing output files in output directory.",
                cxxopts::value<bool>()->default_value("false")->implicit_value("true"))

            ("stats",
                "Statistics mode: none|verbose|dry-verbose\n"
                "'dry-verbose' disables output and prints verbose stats.",
                cxxopts::value<std::string>()->default_value("none")->implicit_value("verbose")) // TODO: implement

            ("h,help", "Show help");

        opts.positional_help("GTFS_ZIP");
        opts.parse_positional({"dataset"});

        auto result = opts.parse(argc, argv);

        if (result.count("help")) { std::cout << opts.help() << '\n'; std::exit(0); }
        if (!result.count("dataset")) {
            std::cerr << "Input GTFS dataset required. Type --help for more information!\n";
            std::exit(0);
        }        

        // output format
        std::string format = result["format"].as<std::string>();
        if (format != "ttl" && format != "nt") {
            throw std::runtime_error("❌  Error: Unsupported output format '" + format + "'. "\
                                     "Supported formats are 'ttl' and 'nt'.\n");
        }
        ntriples_output_ = (format == "nt");

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
        
        overwrite_output_ = result["overwrite"].as<bool>();
    }

    // GETTERs
    bool isNTriplesOutput() const { return ntriples_output_; }
    bool isDebugWarnings() const { return debug_warnings_; }
    bool isSpecDump() const { return spec_dump_; }
    double ReadChunkSizeMB() const { return read_chunk_size_mb_; }
    double WriteChunkSizeMB() const { return write_chunk_size_mb_; }
    bool isOverwriteOutput() const { return overwrite_output_; }
    const std::filesystem::path& InputPath() const { return inputPath_; }
    const std::filesystem::path& OutputPath() const { return outputPath_; }
};

export class RuntimeContainer {
  private:
    const Settings& settings_;
    field_transforms::TransformRegistry& registry_;
    PersistentStorage storage_;

  public:
    RuntimeContainer(const Settings& settings, field_transforms::TransformRegistry& registry)
        : settings_(settings), registry_(registry) {}

    const Settings& getSettings() const { return settings_; }
    field_transforms::TransformRegistry& getTransformRegistry() { return registry_; }
    const field_transforms::TransformRegistry& getConstTransformRegistry() const { return registry_; }
    PersistentStorage& getStorage() { return storage_; }
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