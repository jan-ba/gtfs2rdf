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

export module gtfs;

import utility;
using namespace util;
using util::operator<<;  // only bringing in required operator

namespace gtfs {

export class ColumnInfo {
  public:
    // TODO: could take dependencies of columns too to calculate required columns internally
    ColumnInfo(std::vector <std::string> possible_columns)
        : possible_columns(std::move(possible_columns)) {
        for (const auto& col : this->possible_columns) {
            column_map[col] = -1; // initialize all to -1 (not found)
        }
    }

    void storeColumnMap(const std::vector<std::string>& header) {
        for (size_t file_idx = 0; file_idx < header.size(); ++file_idx) {
            if (column_map.contains(header[file_idx])) {
                column_map[header[file_idx]] = static_cast<int>(file_idx);
            } else {
                throw std::runtime_error("gtfs::ColumnInfo: unknown column: " + header[file_idx]);
            }
        }
    }

    const std::map<std::string, int>& getColumnMap() const {
        return column_map;
    }

  private:
    const std::vector<std::string> possible_columns;
    std::map<std::string, int> column_map;
    
};

export const std::vector<std::string> possible_columns_stops = {
                "stop_id", "stop_code", "stop_name", "stop_desc", "stop_lat", "stop_lon",
                "zone_id", "stop_url", "location_type", "parent_station", "stop_timezone",
                "wheelchair_boarding", "level_id", "platform_code"};

export const std::vector<std::string> possible_columns_stop_times = {"trip_id", "arrival_time",
                "departure_time", "stop_id", "location_group_id" "location_id", "stop_sequence",
                "stop_headsign", "start_pickup_drop_off_window", "end_pickup_drop_off_window",
                "pickup_type", "drop_off_type", "continuous_pickup", "continuous_drop_off",
                "shape_dist_traveled", "timepoint", "pickup_booking_rule_id",
                "drop_off_booking_rule_id"};

// suppose these are required
const std::vector<std::string> required_columns_stops = {
    "stop_name", "parent_station", "stop_id", "stop_lat", "stop_lon",
    "location_type", "platform_code"
};


// A generic record representing a row in a GTFS stops file.
// export class Stop {
//   public:
//     Stop(std::string stop_name, std::string parent_station, std::string stop_id, std::string stop_lat,
//         std::string stop_lon, std::string location_type, std::string platform_code)
//         : stop_name(std::move(stop_name)), parent_station(std::move(parent_station)),
//           stop_id(std::move(stop_id)), stop_lat(std::move(stop_lat)), stop_lon(std::move(stop_lon)), 
//           location_type(std::move(location_type)), platform_code(std::move(platform_code)) {}
    
//     // perhaps string_views better?
//     std::string getStopName() const { return stop_name; }
//     std::string getParentStation() const { return parent_station; }
//     std::string getStopId() const { return stop_id; }
//     std::string getStopLat() const { return stop_lat; }
//     std::string getStopLon() const { return stop_lon; }
//     std::string getLocationType() const { return location_type; }
//     std::string getPlatformCode() const { return platform_code; }

//     friend std::ostream& operator<<(std::ostream& os, const Stop& stop) {
//         return os << "Stop{"
//                   << "name=" << stop.stop_name << ", "
//                   << "parent=" << stop.parent_station << ", "
//                   << "id=" << stop.stop_id << ", "
//                   << "lat=" << stop.stop_lat << ", "
//                   << "lon=" << stop.stop_lon << ", "
//                   << "type=" << stop.location_type << ", "
//                   << "platform=" << stop.platform_code
//                   << "}";
//     }

//   private:
//     std::string stop_name;
//     std::string parent_station;
//     std::string stop_id;
//     std::string stop_lat;
//     std::string stop_lon;
//     std::string location_type;
//     std::string platform_code;
// };


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
    out.reserve(16); // small prealloc

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


export std::vector<std::vector<std::string>> parse_file(const std::filesystem::path& path, ColumnInfo column_info) {
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
            column_info.storeColumnMap(header);
            std::cout << "Mappings: \n" << column_info.getColumnMap() << std::endl;

            // TODO: validity check
            first_line = false;
            continue;
        }
        first_line = false;
        std::vector<std::string> cols = split_line(line);
        result.push_back(cols);
    }

    return result;
}
}