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
#include <iostream>
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
    const runtime::RuntimeContainer& rt_;
    size_t threshold_;

    void flush() {
        if (chunk_.empty()) return;
        size_t n = chunk_.size();
        const char* d = chunk_.data(); 
        while (n) {
            size_t w = std::fwrite(d , 1, n, file_);
            if (w == 0) throw std::runtime_error("❌ Write error: Could not flush to disk.");
            d += w;
            n -= w;
        }
        chunk_.clear();
    }

  public:
    Writer(const std::filesystem::path& path, runtime::RuntimeContainer& rt, double buffer_size_mb)
        : rt_(rt), threshold_(buffer_size_mb * 1024 * 1024) {
        if (std::filesystem::exists(path)) {
            if (!rt_.getSettings().isOverwriteOutput()) {
                std::cerr << "❌  Write error: Output file '" << path.string() 
                          << "' already exists. To overwrite, enable the overwrite option.\n";
                std::exit(1);
            }
        }
        file_ = std::fopen(path.string().c_str(), "wb");
        if (!file_) throw std::runtime_error("❌ Write error: Cannot open '" + path.string() + "' for writing.");
        chunk_.reserve(threshold_);
    }

    // TODO: perhaps rename chunk to buffer everywhere?
    Writer(const std::filesystem::path& path, runtime::RuntimeContainer& rt)
        : Writer(path, rt, rt.getSettings().WriteChunkSizeMB()) {
    }


    void writePrefixes(const Schema& sc) {
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

    void convert2RDF(Schema& sc, const std::vector<std::string>& flat, size_t num_cols) {
        auto& instructions = sc.getInstructions();
        for (size_t base = 0; base < flat.size(); base += num_cols) {
            std::span<const std::string> row(flat.data() + base, num_cols);

            for (auto& instr : instructions) {
                append(instr.render(row));
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
