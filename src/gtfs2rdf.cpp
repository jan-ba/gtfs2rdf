#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>

import gtfs_parser;
import utility;
import rdf_writer;
import schema.core;
import schema.agency;
import schema.calendar_dates;
import schema.calendar;
import schema.routes;
import schema.stop_times;
import schema.stops;
import schema.trips;

// import schema.registry;

using namespace util;

using Factory = schema::Schema(*)();
const std::unordered_map<std::string, Factory> factories{
  {"agency.txt",         &schema::buildAgencySchema},
  {"calendar.txt",       &schema::buildCalendarSchema},
  {"calendar_dates.txt", &schema::buildCalendarDatesSchema},
  {"routes.txt",         &schema::buildRoutesSchema},
  {"stop_times.txt",     &schema::buildStopTimesSchema},
  {"stops.txt",          &schema::buildStopsSchema},
  {"trips.txt",          &schema::buildTripsSchema},
};

int convertFileToStream(std::filesystem::path inputPath, schema::Schema& schema, std::ostream& out,
                        bool write_prefixes)
{
    using Clock = std::chrono::steady_clock;

    auto t0 = Clock::now();
    std::cout << "\n______________________________________________________________\n";
    auto rows = gtfs::parse_file(inputPath, schema);
    double read_s = std::chrono::duration<double>(Clock::now() - t0).count();


    std::cout << "⌛  Parsed " << inputPath << " (" << rows.size()
              << " rows) in " << read_s << " s\n";

    t0 = Clock::now();
    int triples = ttl::write2TTL(schema, rows, out, write_prefixes);
    double write_s = std::chrono::duration<double>(Clock::now() - t0).count();

    std::cout << "✅  Wrote " << triples << " triples from " << inputPath.filename()
              << " in " << write_s << " s\n"
              << "______________________________________________________________\n";
    return triples;
}


int main(int argc, char* argv[]) {
    if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
        std::cout << "GTFS to RDF Converter\n"
              << "Usage: gtfs2rdf <dataset> [output_path]\n"  // change to path to .zip later
              << "  dataset          Path to a (zip) directory, that contains the .txt files\n"  // add paras later (e.g. buffer / batch size)
              << "  output_path      Optional path of ouput (including filename)\n";
        return argc < 2 ? 1 : 0;
    }

    std::filesystem::path inputDir = argv[1];
    std::filesystem::path outputPath = argc > 2 ? argv[2] : std::filesystem::current_path() / "out.ttl";
    if (std::filesystem::is_directory(outputPath)) { outputPath /= "out.ttl"; }

    if (!std::filesystem::exists(inputDir)) {
        std::cerr << "❌  Error: Input directory '" << inputDir << "' does not exist.\n";
        return 1;
    }

    if (std::filesystem::exists(outputPath)) {
        std::cout << "Output path '" << outputPath << "' already exists. Overwrite? [y/N]\n";
        std::string a;
        std::getline(std::cin, a);
        if (!(a == "y" || a == "Y" || a == "yes" || a == "YES")) return 1;
    }

    std::vector<std::filesystem::path> files_in_dir;
    std::vector<schema::Schema> used_schemas;
 
    // searches for all files in the specified input directory and creates respective schemas (if possible)
    for (const auto& entry : std::filesystem::directory_iterator(inputDir)) {
        const std::string fname = entry.path().filename().string();

        if (auto it = factories.find(fname); it != factories.end()) {
            files_in_dir.push_back(entry.path());
            used_schemas.emplace_back(it->second());  // call factory
        } else {
            std::cerr << "⚠️  Warning: '" << fname << "' not a valid GTFS file or respective "\
                         "schema not inmplemented" << std::endl;
        }
    }

    std::ofstream out(outputPath, std::ios::binary);
    if (!out) {
        std::cerr << "❌  Error: cannot open '" << outputPath << "' for writing.\n";
        return 1;
    }

    // Here dependencies could be accounted for or ordering of conversion
    auto merged_prefixes = schema::merge_prefixes(used_schemas, true);
    long long total_triples = 0;
    for (size_t i = 0; i < files_in_dir.size(); i++) {
        if (i == 0) {
            used_schemas[i].setPrefixes(merged_prefixes);
            total_triples += convertFileToStream(files_in_dir[i], used_schemas[i], out, true);    
        } else {
        total_triples += convertFileToStream(files_in_dir[i], used_schemas[i], out, false);
        }
    }

    std::cout << "🎉  Done. Wrote " << total_triples << " triples to " << outputPath << "\n";
    return 0;
}