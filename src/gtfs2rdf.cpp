#include <iostream>
#include <filesystem>
#include <string>
#include <vector>

import gtfs;
import utility;

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
    auto start = std::chrono::high_resolution_clock::now();
    std::cout << "Parsing GTFS data from " << inputPath << "...\n";
    std::vector<std::vector<std::string>> stops = gtfs::parse_file(inputPath, 
        gtfs::ColumnInfo(gtfs::possible_columns_stop_times));
    std::cout << "Converting " << inputPath << " to RDF...\n";
    auto time = std::chrono::duration<double>(
        std::chrono::high_resolution_clock::now() - start).count();
    std::cout << "DONE  - Reading " << stops.size() << " lines took " << time << " seconds.\n";
    int n = 10;
    std::cout << "First " << n << " stops:\n"
              << std::vector<std::vector<std::string>>(stops.begin(), stops.begin() + n) << "\n";
    return 0;
}