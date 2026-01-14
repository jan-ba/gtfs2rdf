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
    runtime::Settings settings(argc, argv);

    zip_t *za;
    int err;

    // try opening the zip file
    if ((za = zip_open(settings.InputPath().string().c_str(), ZIP_RDONLY, &err)) == NULL) {
        zip_error_t error;
        zip_error_init_with_code(&error, err);
        fprintf(stderr, "❌  Error: Cannot open zip archive '%s': %s\n",
	        settings.InputPath().string().c_str(), zip_error_strerror(&error));
        zip_error_fini(&error);
        return 1;
    }


    std::vector<std::string> files_in_dir;
    std::vector<schema::Schema> used_schemas;
    field_transforms::TransformRegistry registry;
    t_lib::register_lib_transforms(registry);
    writer::Writer writer(settings.OutputPath(), settings.WriteChunkSizeMB() * 1024 * 1024);
    runtime::RuntimeContainer rt(settings, registry);
    gtfs::GTFSParser_Workspace ws(rt, writer);

    for ( const auto& [ file, factory ] : factories ) {
        if (zip_name_locate(za, file.c_str(), ZIP_FL_ENC_GUESS) != -1) {
            files_in_dir.push_back(file);
            used_schemas.emplace_back(factory(rt));  // call factory
        }
    }

    if (files_in_dir.empty()) {
        zip_close(za);
        std::cerr << "❌  Error: No GTFS files corresponding to known schemas found in archive '" 
                  << settings.InputPath().string() << "'.\n";
        return 1;
    }

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
    if (settings.isSpecDump() && !used_schemas.empty()) {
        std::filesystem::path specPath = settings.OutputPath();
        specPath.replace_extension(".spec.txt");
        writer::Writer onth_writer(specPath, 1ull<<20);
        if (!settings.isNTriplesOutput()) onth_writer.writePrefixes(used_schemas[0]);
        for (auto& schema : used_schemas) {
            for (auto& inst : schema.getInstructions()) {
                onth_writer.append(inst.getRawInstruction() + "\n");
            }
        }
        std::cout << "📄  Wrote ontology spec to " << specPath << "\n";
    }

    return 0;
}