module;

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <algorithm>
#include <iostream>
#include <map>

export module gtfs_parser;

import utility;
import rdf_schema;

using namespace util;
using util::operator<<;  // only bringing in required operator

namespace gtfs {

// CSV line splitter for GTFS
std::vector<std::string> split_line(std::string_view line) {
    // strip UTF-8 BOM if present (only relevant for first header line)
    if (line.size() >= 3 &&
        static_cast<unsigned char>(line[0]) == 0xEF &&
        static_cast<unsigned char>(line[1]) == 0xBB &&
        static_cast<unsigned char>(line[2]) == 0xBF) {
        line.remove_prefix(3);
    }

    std::vector<std::string> out;
    out.reserve(10); // small prealloc

    std::string cache;
    cache.reserve(64);

    bool in_quotes = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (in_quotes) {
            if (c == '"') {
                // doubled quote -> literal quote
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    cache.push_back('"');
                    ++i;
                } else {
                    in_quotes = false;
                }
            } else {
                cache.push_back(c);
            }
        } else {
            if (c == ',') {
                out.push_back(std::move(cache));
                cache.clear();
            } else if (c == '"') {
                in_quotes = true;
            } else if (c == '\r') {
                // ignore line break characters
            } else {
                cache.push_back(c);
            }
        }
    }
    out.push_back(std::move(cache));
    return out;
}


export std::vector<std::vector<std::string>> parse_file(const std::filesystem::path& path,
    rdf_schema::Schema& schema) {
    std::ifstream ifs(path);
    if (!ifs) {
        throw std::runtime_error("gtfs::parse_file: unable to open file: " + path.string());
    }

    std::vector<std::vector<std::string>> result;
    std::string line;
    bool first_line = true;

    while (std::getline(ifs, line)) {
        if (first_line) {
            // parse header and remember column order
            auto header = split_line(line);
            std::cout << "GTFS Header Columns: " << header << "\n";
            schema.setHeader(header);
            std::cout << "Mappings: \n" << schema.getColumnMap() << std::endl;

            // TODO: validity checks?
            first_line = false;
            continue;
        }
        first_line = false;
        std::vector<std::string> cols = split_line(line);
        result.push_back(cols);
    }

    return result;
}

} // namespace