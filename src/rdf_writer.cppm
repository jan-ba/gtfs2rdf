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
#include <unordered_map>

export module rdf_writer;
import schema;
import runtime;

using namespace schema;

using Rows = std::vector<std::vector<std::string>>;

namespace writer {

// RDF writer with buffered output
// one instance per output file
export class Writer {
  private:
    std::FILE* file_;
    std::string buffer_;
    const runtime::RuntimeContainer& rt_;
    size_t threshold_;

    void flush() {
        if (buffer_.empty()) return;
        size_t n = buffer_.size();
        const char* d = buffer_.data(); 
        while (n) {
            size_t w = std::fwrite(d , 1, n, file_);
            if (w == 0) throw std::runtime_error("❌ Write error: Could not flush to disk.");
            d += w;
            n -= w;
        }
        buffer_.clear();
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
        buffer_.reserve(threshold_);
    }

    Writer(const std::filesystem::path& path, runtime::RuntimeContainer& rt)
        : Writer(path, rt, rt.getSettings().WriteBufferSizeMB()) {
    }


    void writePrefixes(const std::unordered_map<std::string, std::string>& map) {
        std::string out;
        for (const auto& [pfx, iri] : map) {
            out += "@prefix " + pfx + ": <" + iri + "> .\n";
        }
        out += "\n";
        append(out);
    }

    void append(const std::string& s) {
        buffer_.append(s.data(), s.size());
        if (buffer_.size() >= threshold_) flush();
    }

    // converts gtfs row according to given schema and appends to buffer
    void convertRow(Schema& sc, const std::vector<std::string>& row) {
        auto& instructions = sc.getInstructions();
        for (auto& instr : instructions) {
            append(instr.render(row));
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

} // namespace writer
