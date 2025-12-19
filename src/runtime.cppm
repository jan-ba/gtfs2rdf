module;

#include <iostream>
#include <cstdint>
#include <sstream>
#include <iomanip>

export module runtime;

import field_transforms;

namespace runtime {

export class Settings {
  private:
    bool ntriples_output_;
    bool syntactic_sugar_;
    bool debug_warnings_;
    bool spec_dump_;
    double batch_size_mb_;

  public:
    Settings(bool ntriples_output, bool syntactic_sugar,
             bool debug_warnings, bool spec_dump, double batch_size_mb)
        : ntriples_output_(ntriples_output), syntactic_sugar_(syntactic_sugar),
          debug_warnings_(debug_warnings), spec_dump_(spec_dump), batch_size_mb_(batch_size_mb) {
        if (ntriples_output_ && syntactic_sugar_) {
            std::cerr << "⚠️  Warning: cannot use turtle syntactic sugar in ntriples output. Disabling syntactic sugar.\n";
            syntactic_sugar_ = false;
        }

        if (batch_size_mb_ <= 0.0) {
            std::cerr << "⚠️  Warning: batch size must be positive. Using default value of 10.0 mb.\n";
            batch_size_mb_ = 10.0;
        }
    }

    bool isNTriplesOutput() const { return ntriples_output_; }
    bool isSyntacticSugar() const { return syntactic_sugar_; }
    bool isDebugWarnings() const { return debug_warnings_; }
    bool isSpecDump() const { return spec_dump_; }
    double getBatchSizeMB() const { return batch_size_mb_; }
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

    std::string fancyPrint(const std::string& filename) {
        std::ostringstream oss;
        oss << "\n📊 Statistics for " << filename << ":\n"
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