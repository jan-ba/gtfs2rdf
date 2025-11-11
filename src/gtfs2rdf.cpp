#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>
#include "util/cxxopts.hpp"

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

long long convertFileToStream(const std::filesystem::path& inputPath, schema::Schema& schema,
                              std::ostream& outfs, size_t batch_size, bool first_file)   
                              // first_file: true only for first file's first batch
{
    using Clock = std::chrono::steady_clock;

    std::ifstream ifs(inputPath, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error("❌  Parsing error: unable to open file: " + inputPath.string());
    }

    std::vector<std::vector<std::string>> rows;
    if (batch_size > 0) rows.reserve(batch_size);

    long long total_triples = 0;
    size_t total_rows  = 0;
    size_t batches     = 0;
    double parse_s = 0.0;
    double write_s = 0.0;
    bool eof = false;
    bool first_batch = true;
    std::cout << "\n______________________________________________________________\n";

    while (!eof) {
        rows.clear();

        // Parse next batch (header handled inside on firstBatch=true)
        auto t0 = Clock::now();
        eof = gtfs::parse_file(ifs, batch_size, first_batch, rows, schema); // fills 'rows'
        parse_s += std::chrono::duration<double>(Clock::now() - t0).count();

        // Write this batch (prefixes only for the very first batch)
        t0 = Clock::now();
        total_triples += ttl::write2TTL(schema, rows, outfs, first_file && first_batch);
        write_s += std::chrono::duration<double>(Clock::now() - t0).count();

        total_rows += rows.size();
        ++batches;

        // Subsequent batches must not emit prefixes nor re-init header logic
        first_batch = false;
    }

    std::cout << "⌛  Parsed " << inputPath << " in " << parse_s << " s"
              << "  (" << total_rows << " rows, " << batches
              << " batches @ " << batch_size << ")\n";
    std::cout << "✅  Wrote " << total_triples << " triples from "
              << inputPath.filename() << " in " << write_s << " s\n"
              << "______________________________________________________________\n";

    return total_triples;
}


int main(int argc, char* argv[]) {
    cxxopts::Options opts("gtfs2rdf", "GTFS->RDF converter");
    opts.add_options()
        ("d,dataset", "Path to GTFS directory", cxxopts::value<std::string>())
        ("o,output",  "Output file or directory", cxxopts::value<std::string>()->default_value("out.ttl"))
        ("b,batch-size", "Rows per batch", cxxopts::value<unsigned long long>()->default_value("100000"))
        ("t,triple", "Store as fully resolved triples, without prefixes or other .ttl syntax", 
         cxxopts::value<bool>()->default_value("false"))  // TODO
        ("s,syntactic-sugar", "Enable syntactic .ttl sugar for a more compact file output", 
         cxxopts::value<bool>()->default_value("false"))  // TODO
        ("w,debug", "Show non-fatal warnings", cxxopts::value<bool>()->default_value("true"))  // TODO
        ("h,help", "Show help");

    auto result = opts.parse(argc, argv);
    if (result.count("help")) { std::cout << opts.help() << '\n'; return 0; }
    if (!result.count("dataset")) { 
        std::cerr << "Please specify the GTFS dataset to be converted or type --help for "\
                     "more information!\n"; return 1; 
    }

    std::filesystem::path inputDir = result["dataset"].as<std::string>();
    std::filesystem::path outputPath = result["output"].as<std::string>();
    size_t batch_size = result["batch-size"].as<unsigned long long>();

    if (!std::filesystem::exists(inputDir)) {
        std::cerr << "❌  Error: Input directory '" << inputDir.string() << "' does not exist.\n";
        return 1;
    }

    if (std::filesystem::exists(outputPath)) {
        std::cout << "Output path '" << outputPath.string() << "' already exists. Overwrite? [y/N]\n";
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
        std::cerr << "❌  Error: cannot open '" << outputPath.string() << "' for writing.\n";
        return 1;
    }

    // TODO: Here dependencies could be accounted for or ordering of conversion

    auto merged_prefixes = schema::merge_prefixes(used_schemas, true);
    long long total_triples = 0;
    for (size_t i = 0; i < files_in_dir.size(); i++) {
        if (i == 0) {
            used_schemas[i].setPrefixes(merged_prefixes);
            total_triples += convertFileToStream(files_in_dir[i], used_schemas[i], out, batch_size, true);    
        } else {
        total_triples += convertFileToStream(files_in_dir[i], used_schemas[i], out, batch_size, false);
        }
    }

    std::cout << "🎉  Done. Wrote " << total_triples << " triples to " << outputPath << "\n";
    return 0;
}