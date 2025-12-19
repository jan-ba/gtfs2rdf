// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the GTFS2RDF project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.


#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>
#include "util/cxxopts.hpp"
#include "zip.h"

import gtfs_parser;
import utility;
import rdf_writer;
import schema;
import field_transforms;
import t_lib;
import runtime;

using namespace util;



using Factory = schema::Factory;
const auto& factories = schema::factories();  // implemented in schema:registry at build time


int main(int argc, char* argv[]) {
    cxxopts::Options opts("gtfs2rdf", "GTFS->RDF converter");
    opts.add_options()
        ("d,dataset", "Path to GTFS .zip archive", cxxopts::value<std::string>())
        ("o,output",  "Output directory", cxxopts::value<std::string>()->default_value("."))
        ("c,chunk-size", "Batch size in mb", cxxopts::value<double>()->default_value("10.0"))
        ("t,triple", "Store as fully resolved triples, without prefixes or other .ttl syntax", 
         cxxopts::value<bool>()->default_value("false"))  // TODO
        ("s,syntactic-sugar", "Enable syntactic .ttl sugar for a more compact file output", 
         cxxopts::value<bool>()->default_value("false"))  // TODO
        ("L,spec-dump", "Dump onthology spec to disk",
        cxxopts::value<bool>()->default_value("false")->implicit_value("true"))  
        ("w,debug", "Show non-fatal warnings", cxxopts::value<bool>()->default_value("true"))  // TODO
        ("h,help", "Show help");

    opts.positional_help("GTFS_ZIP");
    opts.parse_positional({"dataset"});

    auto result = opts.parse(argc, argv);
    if (result.count("help")) { std::cout << opts.help() << '\n'; return 0; }
    if (!result.count("dataset")) { 
        std::cerr << "Input GTFS dataset required. Type --help for "\
                     "more information!\n"; return 1; 
    }

    runtime::Settings settings(
        result["triple"].as<bool>(),
        result["syntactic-sugar"].as<bool>(),
        result["debug"].as<bool>(),
        result["spec-dump"].as<bool>(),
        result["chunk-size"].as<double>());

    std::filesystem::path inputZIP = result["dataset"].as<std::string>();
    std::string file_ext = settings.isNTriplesOutput() ? ".nt" : ".ttl";
    std::filesystem::path outputPath = result["output"].as<std::string>() + "/" + inputZIP.stem().string() + file_ext;
    double batch_size_mb = result["chunk-size"].as<double>();
    if (batch_size_mb <= 0.0) {
        std::cerr << "❌  Error: batch size must be positive.\n";
        return 1;
    }

    // check that file exists and is a zip file
    if (!std::filesystem::is_regular_file(inputZIP) || inputZIP.extension() != ".zip") {
        std::cerr << "❌  Error: Input '" << inputZIP.string() << "' doesn't exist or "\
                                                                   "is not a zip file.\n";
        return 1;
    }

    zip_t *za;
    int err;

    // try opening the zip file
    if ((za = zip_open(inputZIP.string().c_str(), ZIP_RDONLY, &err)) == NULL) {
        zip_error_t error;
        zip_error_init_with_code(&error, err);
        fprintf(stderr, "❌  Error: Cannot open zip archive '%s': %s\n",
	        inputZIP.string().c_str(), zip_error_strerror(&error));
        zip_error_fini(&error);
        return 1;
    }

    // validate output path (and avoid unwanted overwriting)
    if (std::filesystem::exists(outputPath)) {
        std::cout << "Output path '" << outputPath.string() << "' already exists. Overwrite? [y/N]\n";
        std::string a;
        std::getline(std::cin, a);
        if (!(a == "y" || a == "Y" || a == "yes" || a == "YES")) return 1;
    }

    std::vector<std::string> files_in_dir;
    std::vector<schema::Schema> used_schemas;
    field_transforms::TransformRegistry registry;
    t_lib::register_lib_transforms(registry);
    writer::Writer writer(outputPath);
    runtime::RuntimeContainer rt(settings, registry);
    gtfs::GTFSParser_Workspace ws(rt, writer);

    for ( const auto& [ file, factory ] : factories ) {
        if (zip_name_locate(za, file.c_str(), ZIP_FL_ENC_GUESS) != -1) {
            files_in_dir.push_back(file);
            used_schemas.emplace_back(factory(rt));  // call factory
        }
    }


    // std::ofstream out("outputPath", std::ios::binary);
    // if (!out) {
    //     std::cerr << "❌  Error: cannot open '" << outputPath.string() << "' for writing.\n";
    //     return 1;
    // }

    // TODO: Here dependencies could be accounted for or ordering of conversion

    auto merged_prefixes = schema::merge_prefixes(used_schemas, true);
    runtime::Statistics stats;
    for (size_t i = 0; i < files_in_dir.size(); i++) {
        zip_file_t* zf = zip_fopen(za, files_in_dir[i].c_str(), 0);
        if (!zf) {
            zip_close(za);
            throw std::runtime_error("❌  Error: cannot open entry inside ZIP: " + files_in_dir[i]);
        }
        if (i == 0) used_schemas[i].setPrefixes(merged_prefixes);
        gtfs::GTFSParser parser(zf, used_schemas[i], i == 0, ws, rt);
        parser.parse();
        zip_fclose(zf);
        stats = stats + parser.getStats();
    }
    zip_close(za);

    std::cout << "🎉  Done.\n" + stats.briefPrint("Total GTFS Set") + "\n";

    // dump ontology spec if requested
    // if (result["spec-dump"].as<bool>()) {
    //     std::filesystem::path specPath = outputPath;
    //     specPath.replace_extension(".spec.txt");
    //     std::ofstream specOut(specPath, std::ios::binary);
    //     if (!specOut) {
    //         std::cerr << "❌  Error: cannot open '" << specPath.string() << "' for writing.\n";
    //         return 1;
    //     }
    //     writer::writePrefixes(specOut, used_schemas[0], rt); // prefixes only once
    //     for (const auto& schema : used_schemas) {
    //         for (const auto& inst : schema.getInstructions()) {
    //             specOut << inst.getRawInstruction() << "\n";
    //         }
    //     }
    //     specOut.close();
    //     std::cout << "📄  Wrote ontology spec to " << specPath << "\n";
    // }

    return 0;
}