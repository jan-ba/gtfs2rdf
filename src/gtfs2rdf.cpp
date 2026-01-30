// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the GTFS2RDF project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.

#include "third_party/cxxopts/cxxopts.hpp"
#include "zip.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

import gtfs_parser;
import util;
import rdf_writer;
import schema;
import field_transforms;
import t_lib;
import runtime;

using namespace util;

using Factory = schema::Factory;
const auto &factories = schema::factories(); // implemented in schema:registry at build time

int main(int argc, char *argv[]) {
	runtime::Settings settings(argc, argv);
	zip_t *za;
	int err;

	// try opening the zip file
	if ((za = zip_open(settings.InputPath().string().c_str(), ZIP_RDONLY, &err)) == NULL) {
		zip_error_t error;
		zip_error_init_with_code(&error, err);
		fprintf(stderr,
		        "❌  Error: Cannot open zip archive '%s': %s\n",
		        settings.InputPath().string().c_str(),
		        zip_error_strerror(&error));
		zip_error_fini(&error);
		return 1;
	}

	// TODO: if there is an error with the schemas or something, the output file should not even
	// persist
	// TODO: add parameter to deactivate all transform triples
	std::vector<std::string> files_in_dir;
	std::vector<schema::Schema> used_schemas;
	std::unordered_map<std::string, size_t> schema_name_to_index; // map from schema name to index
	field_transforms::TransformRegistry registry;
	t_lib::register_lib_transforms(registry);
	runtime::RuntimeContainer rt(settings, registry);
	writer::Writer writer(settings.OutputPath(), rt);
	gtfs::GTFSParser_Workspace ws(rt, writer);

	// determine which schemas to use based on files present in the zip
	for (const auto &[file, factory] : factories) {
		if (zip_name_locate(za, file.c_str(), ZIP_FL_ENC_GUESS) != -1) {
			files_in_dir.push_back(file);
			used_schemas.emplace_back(factory(rt)); // call factory
			schema_name_to_index[used_schemas.back().getName()] = used_schemas.size() - 1;
		}
	}

	// error if no files found that correspond to known schemas
	if (files_in_dir.empty()) {
		zip_close(za);
		std::cerr << "❌  Error: No GTFS files corresponding to known schemas found in archive '"
		          << settings.InputPath().string() << "'.\n";
		return 1;
	}

	// compile schemas such that they are ready for use and dependency info is available
	for (auto &sc : used_schemas) {
		sc.compile();
	}

	// determine processing order via topological sort of dependencies
	// this whole section is not very efficient, but then again we're looking at GTFS datasets
	// such that n < 50 in practice
	TopologicalSort toposort(files_in_dir.size());
	std::vector<size_t> num_depending_schemas(files_in_dir.size(), 0);
	for (size_t i = 0; i < files_in_dir.size(); i++) {
		toposort.addNode(i);
		std::cout << " File '" << files_in_dir[i]
		          << "' depends on: " << used_schemas[i].getDependencies() << "\n";
		for (const auto &dep : used_schemas[i].getDependencies()) {
			if (!schema_name_to_index.contains(dep)) {
				zip_close(za);
				throw std::runtime_error("❌  Error: schema dependency '" + dep + "' of schema '" +
				                         files_in_dir[i] + "' not found in GTFS dataset.");
			}
			toposort.addEdge(schema_name_to_index[dep], i, true); // dep must come before i
			num_depending_schemas[schema_name_to_index[dep]]++;
		}
	}

	// deactivate all storage writes from schemas that are not needed later on
	// for (size_t i = 0; i < files_in_dir.size(); i++) {
	// 	if (num_depending_schemas[i] == 0) {
	// 		used_schemas[i].forbidStorageWrites();
	// 		std::cout << "🛑  Deactivated storage writes for schema '" << used_schemas[i].getName()
	// 		          << "'\n";
	// 	}
	// }

	// allow loops (schema may read from its own previously written storage)
	auto order = toposort.sort();

	std::cout << "🔀  Processing GTFS files in order: ";
	for (size_t i = 0; i < files_in_dir.size(); i++) {
		std::cout << files_in_dir[order[i]] << " ";
	}
	std::cout << "\n";

	auto merged_prefixes = schema::merge_prefixes(used_schemas, true);
	runtime::Statistics stats;
	for (size_t i = 0; i < files_in_dir.size(); i++) {
		zip_file_t *zf = zip_fopen(za, files_in_dir[order[i]].c_str(), 0);
		if (!zf) {
			zip_close(za);
			throw std::runtime_error("❌  Error: cannot open entry inside ZIP: " +
			                         files_in_dir[order[i]]);
		}
		if (i == 0 && !settings.isNTriplesOutput())
			writer.writePrefixes(merged_prefixes);
		gtfs::GTFSParser parser(zf, used_schemas[order[i]], ws, rt);
		parser.parse();
		// used_schemas[order[i]].finalise();
		for (auto &dep : used_schemas[order[i]].getDependencies()) {
			num_depending_schemas[schema_name_to_index[dep]]--;
			if (num_depending_schemas[schema_name_to_index[dep]] == 0) {
				// rt.getStorage().clear_context(dep);
				std::cout << "🧹  Cleared storage context for schema '" << dep << "' after "
				          << " last dependent schema '" << used_schemas[order[i]].getName()
				          << "' was processed.\n";
			}
		}
		zip_fclose(zf);
		stats = stats + parser.getStats();
	}
	zip_close(za);

	std::cout << "🎉  Done.\n" + stats.briefPrint("Total GTFS Set") + "\n";

	// dump ontology spec if requested
	if (settings.isSpecDump() && !used_schemas.empty()) {
		std::filesystem::path specPath = settings.OutputPath();
		specPath.replace_extension(".spec.txt");
		writer::Writer onth_writer(specPath, rt, 1.0); // small buffer for spec writing
		if (!settings.isNTriplesOutput())
			onth_writer.writePrefixes(merged_prefixes);
		for (auto &schema : used_schemas) {
			for (auto &inst : schema.getInstructions()) {
				onth_writer.append(inst.getRawInstruction() + "\n");
			}
		}
		std::cout << "📄  Wrote ontology spec to " << specPath << "\n";
	}
	return 0;
}