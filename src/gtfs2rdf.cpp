#include <iostream>
#include <filesystem>
#include <string>
#include <vector>

import gtfs_parser;
import utility;
import rdf_writer;
import rdf_schema;
import stops_schema;
import stop_times_schema;

using namespace util;


int main(int argc, char* argv[]) {
    if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
        std::cout << "GTFS to RDF Converter\n"
              << "Usage: gtfs2rdf <input_file> [output_directory]\n"
              << "  input_file       Path to stops.txt file\n"
              << "  output_directory Optional output directory (default: current directory)\n";
        return argc < 2 ? 1 : 0;
    }

    std::filesystem::path inputPath = argv[1];
    std::filesystem::path outputDir = argc > 2 ? argv[2] : std::filesystem::current_path();

    if (!std::filesystem::exists(inputPath)) {
        std::cerr << "Error: Input file '" << inputPath << "' does not exist.\n";
        return 1;
    }

    if (!std::filesystem::exists(outputDir)) {
        std::cout << "Creating output directory...\n";
        std::filesystem::create_directories(outputDir);
    }


    // rdf_schema::Schema schema = stops_schema::buildStopsSchema();
    rdf_schema::Schema schema = stop_times_schema::buildStopTimesSchema();
    auto start = std::chrono::high_resolution_clock::now();
    std::cout << "⌛  Parsing GTFS data from " << inputPath << "..." << std::endl;
    std::vector<std::vector<std::string>> stops = gtfs::parse_file(inputPath, 
        schema);

    auto time = std::chrono::duration<double>(
        std::chrono::high_resolution_clock::now() - start).count();
    std::cout << "✅  Reading " << stops.size() << " lines took " << time << " seconds." << std::endl;
    
    std::filesystem::path outPath = outputDir / (inputPath.stem().string() + ".ttl");
    start = std::chrono::high_resolution_clock::now();
    std::cout << "⌛  Writing RDF data to " << outPath << "..." << std::endl;
    std::ofstream ofs(outPath, std::ios::binary);
    if (!ofs) {
        std::cerr << "Error: cannot open '" << outPath << "' for writing.\n";
        return 1;
    }

    int out_lines = ttl::write2TTL(schema, stops, ofs);
    time = std::chrono::duration<double>(
        std::chrono::high_resolution_clock::now() - start).count();
    std::cout << "✅  Writing RDF data (" << out_lines << " triples) to " << outPath << " took " << time << " seconds." << std::endl;

    // int n = 10;
    // std::vector<std::vector<std::string>> subvector(stops.begin(), stops.begin() + n);
    // std::cout << "First " << n << " stops:\n"
    //           << subvector << "\n";
    // ttl::write2TTL(schema, subvector, std::cout); std::cout.flush();
    
    return 0;
}