// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the GTFS2RDF project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.


module;

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <optional>
#include <cctype>
#include <cstdio>
#include <string_view>
#include <stdexcept>
#include <span>

export module rdf_writer;
import schema;
import runtime;

using namespace schema;

using Rows = std::vector<std::vector<std::string>>;

namespace writer {

export class Writer {
  private:
    std::FILE* file_;
    std::string chunk_;
    size_t threshold_;

    void flush() {
        if (chunk_.empty()) return;
        size_t n = chunk_.size();
        const char* d = chunk_.data(); 
        while (n) {
            size_t w = std::fwrite(d , 1, n, file_);
            if (w == 0) throw std::runtime_error("fwrite failed");
            d += w;
            n -= w;
        }
        chunk_.clear();
    }

  public:
    Writer(const std::filesystem::path& path, size_t threshold = 50ull<<20)  // xx MiB
        : threshold_(threshold) {
        
        file_ = std::fopen(path.string().c_str(), "wb");
        if (!file_) throw std::runtime_error("fopen failed");
        chunk_.reserve(threshold_);
    }

    void writePrefixes(const Schema& sc) {
        // if (rt.getSettings().isNTriplesOutput()) return;
        std::string out;
        for (const auto& [pfx, iri] : sc.getPrefixes()) {
            out += "@prefix " + pfx + ": <" + iri + "> .\n";
        }
        out += "\n";
        append(out);
    }

    void append(const std::string& s) {
        chunk_.append(s.data(), s.size());
        if (chunk_.size() >= threshold_) flush();
    }

    // void convert2RDF(const Schema& sc, const Rows& rows) {
    //     auto instructions = sc.getInstructions();

    //     for (const auto& row : rows) {
    //         for (auto& instr : instructions) {
    //             append(instr.render(row));
    //         }
    //     }
    // }

    void convert2RDF(Schema& sc, const std::vector<std::string>& flat, size_t num_cols) {
        auto& instructions = sc.getInstructions();
        for (size_t base = 0; base < flat.size(); base += num_cols) {
            std::span<const std::string> row(flat.data() + base, num_cols);

            for (auto& instr : instructions) {
                append(instr.render(row));   // make render accept span (see below)
            }
        }
    }

    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    Writer(Writer&&) = delete;
    Writer& operator=(Writer&&) = delete;

    ~Writer() { 
        flush();
        if (file_) std::fclose(file_); 
    }
};

} // namespace
