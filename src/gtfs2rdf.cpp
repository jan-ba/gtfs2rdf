// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the GTFS2RDF project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.

#include "third_party/cxxopts/cxxopts.hpp"
#include "util/diagnostics.h"
#include "zip.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

import gtfs_parser;
import util;
import writer;
import schema;
import field_transforms;
import t_lib;
import runtime;

using namespace util;

using Factory = schema::Factory;
const auto& factories = schema::factories(); // implemented in schema:registry at build time

int main(int argc, char* argv[]) {
	try {
		runtime::Settings settings(argc, argv);
		zip_t* za;
		int err;

		// try opening the zip file
		if ((za = zip_open(settings.InputPath().string().c_str(), ZIP_RDONLY, &err)) == NULL) {
			zip_error_t error;
			zip_error_init_with_code(&error, err);
			zip_error_fini(&error);
			throw diagnostics::Error("IO error: cannot open zip archive '" +
			                         settings.InputPath().string() + "' (error '" +
			                         std::string(zip_error_strerror(&error)) + "')");
		}

		std::vector<std::string> files_in_dir;
		std::vector<schema::Schema> used_schemas;
		std::unordered_map<std::string, size_t>
		    schema_name_to_index; // map from schema name to index
		field_transforms::TransformRegistry registry;
		t_lib::register_lib_transforms(registry);
		runtime::RuntimeContainer rt(settings, registry);

		// determine which schemas to use based on files present in the zip
		for (const auto& [file, factory] : factories) {
			if (zip_name_locate(za, file.c_str(), ZIP_FL_ENC_GUESS) != -1) {
				files_in_dir.push_back(file);
				try {
					used_schemas.emplace_back(factory(rt)); // call factory
					schema_name_to_index[used_schemas.back().getName()] = used_schemas.size() - 1;
					rt.getWarningCollector().addNode(
					    "while building schema for file '" + file + "'", 1);
				} catch (const diagnostics::Error& e) {
					zip_close(za);
					diagnostics::wrap_and_rethrow(e,
					                              "while building schema for file '" + file + "'");
				}
			}
		}

		// error if no files found that correspond to known schemas
		if (files_in_dir.empty()) {
			zip_close(za);
			throw diagnostics::Error(
			    "IO error: no GTFS files corresponding to known schemas found in archive '" +
			    settings.InputPath().string() + "'.\n");
		}

		// compile schemas such that they are ready for use and dependency info is available
		for (auto& sc : used_schemas) {
			try {
				sc.compile();
				rt.getWarningCollector().addNode("while compiling schema '" + sc.getName() + "'",
				                                 2);
			} catch (const diagnostics::Error& e) {
				zip_close(za);
				diagnostics::wrap_and_rethrow(e, "while compiling schema '" + sc.getName() + "'");
			}
		}

		// determine processing order via topological sort of dependencies
		// this whole section is not very efficient, but then again we're looking at GTFS feeds
		// such that n < 50 in practice
		TopologicalSort toposort(files_in_dir.size());
		std::vector<size_t> num_depending_schemas(files_in_dir.size(), 0);
		for (size_t i = 0; i < files_in_dir.size(); i++) {
			toposort.addNode(i);
			for (const auto& dep : used_schemas[i].getDependencies()) {
				// TODO: debug info
				if (!schema_name_to_index.contains(dep)) {
					zip_close(za);
					throw diagnostics::Error("Schema error: dependency '" + dep + "' of schema '" +
					                         used_schemas[i].getName() +
					                         "' not found in GTFS feed");
				}
				toposort.addEdge(schema_name_to_index[dep], i, true); // dep must come before i
				num_depending_schemas[schema_name_to_index[dep]]++;
			}
		}

		// deactivate all storage writes from schemas that are not needed later on
		for (size_t i = 0; i < files_in_dir.size(); i++) {
			if (num_depending_schemas[i] == 0) {
				used_schemas[i].forbidStorageWrites();
				rt.getWarningCollector().addLeaf("Deactivated storage writes for schema '" +
				                                     used_schemas[i].getName() +
				                                     "' (not required by another schema)",
				                                 diagnostics::WarningLevel::Info,
				                                 true);
			}
		}

		// allow loops (schema may read from its own previously written storage)
		std::vector<size_t> order(files_in_dir.size());
		try {
			order = toposort.sort();
			rt.getWarningCollector().addNode("while determining processing order of GTFS files", 2);
		} catch (const std::runtime_error& e) {
			zip_close(za);
			size_t error_node = toposort.getErrorNode();
			throw diagnostics::Error("TopologicalSort error: While determining processing order of "
			                         "GTFS files. Problematic schema: '" +
			                         used_schemas[error_node].getName() + "'");
		}

		rt.getWarningCollector().addLeaf(
		    "Processing GTFS files in order: ", diagnostics::WarningLevel::Info, true);
		for (size_t i = 0; i < files_in_dir.size(); i++) {
			rt.getWarningCollector().appendToLeaf(files_in_dir[order[i]],
			                                      diagnostics::WarningLevel::Info);
		}

		writer::Writer writer(settings.OutputPath(), rt, !settings.isPreRun());
		gtfs::GTFSParser_Workspace ws(rt, writer);
		auto merged_prefixes = schema::merge_prefixes(used_schemas, rt.getWarningCollector(), true);
		std::vector<diagnostics::Statistics> per_file_stats(files_in_dir.size());
		for (size_t i = 0; i < files_in_dir.size(); i++) {
			zip_file_t* zf = nullptr;
			try {
				zf = zip_fopen(za, files_in_dir[order[i]].c_str(), 0);
				if (!zf) {
					zip_close(za);
					throw diagnostics::Error("IO error: cannot open entry inside ZIP archive");
				}
				if (i == 0 && !settings.isNTriplesOutput() && !settings.isPreRun())
					writer.writePrefixes(merged_prefixes);
				gtfs::GTFSParser parser(zf, used_schemas[order[i]], ws, rt);
				parser.parse();
				for (auto& dep : used_schemas[order[i]].getDependencies()) {
#ifdef NDEBUG
					num_depending_schemas[schema_name_to_index[dep]]--;
#endif
					if (num_depending_schemas[schema_name_to_index[dep]] == 0) {
						rt.getStorage().clearContext(dep);
						rt.getWarningCollector().addLeaf("Cleared storage for schema '" + dep +
						                                     "' after last dependent schema '" +
						                                     used_schemas[order[i]].getName() +
						                                     "' was processed.",
						                                 diagnostics::WarningLevel::Info,
						                                 true);
					}
				}
				zip_fclose(zf);
				if (settings.isPreRun()) {
					diagnostics::Statistics stats;
					stats.name = files_in_dir[order[i]];
					const double quot = static_cast<double>(parser.getStats().rows) /
					                    static_cast<double>(settings.getPreRunSampleSize());
					stats.triples =
					    quot < 1.0 ? parser.getStats().triples : quot * parser.getStats().triples;
					stats.rows = parser.getStats().rows;
					stats.header = used_schemas[order[i]].formatHeaderWithUnused();
					stats.num_chars = quot < 1.0 ? parser.getStats().num_chars
					                             : quot * parser.getStats().num_chars;
#if GTFS2RDF_FULL_STATS
					stats.write_ns =
					    quot < 1.0 ? parser.getStats().write_ns : quot * parser.getStats().write_ns;
					stats.parse_ns = parser.getStats().parse_ns;
					stats.conversion_ns = quot < 1.0 ? parser.getStats().conversion_ns
					                                 : quot * parser.getStats().conversion_ns;
#endif
					per_file_stats[order[i]] = stats;
				} else {
					per_file_stats[order[i]] = parser.getStats();
				}

				rt.getWarningCollector().addNode(
				    "while processing GTFS file '" + files_in_dir[order[i]] + "'", 2);
			} catch (const diagnostics::Error& e) {
				zip_close(za);
				zip_fclose(zf);
				writer.deleteFile(); // output will be faulty
				diagnostics::wrap_and_rethrow(
				    e, "while processing GTFS file '" + files_in_dir[order[i]] + "'");
			}
		}
		zip_close(za);

		diagnostics::Statistics total_stats;
		total_stats.name = "Total GTFS Feed";
		for (size_t i = 0; i < files_in_dir.size(); i++) {
			total_stats = total_stats + per_file_stats[i];
		}

		// dump ontology spec if requested
		if (settings.isSpecDump() && !used_schemas.empty()) {
			std::filesystem::path specPath = settings.OutputPath();
			specPath.replace_extension(".spec.txt");
			writer::Writer onth_writer(specPath, rt, 1.0); // small buffer for spec writing
			if (!settings.isNTriplesOutput())
				onth_writer.writePrefixes(merged_prefixes);
			for (auto& schema : used_schemas) {
				for (auto& inst : schema.getInstructions()) {
					onth_writer.writeRaw(inst.getRawInstruction() + "\n");
				}
			}
			rt.getWarningCollector().addLeaf("Wrote ontology spec to " + specPath.string(),
			                                 diagnostics::WarningLevel::Info,
			                                 true);
		}

		rt.getWarningCollector().printWarningSummary();

		if (settings.isPreRun() &&
		    !(settings.getStatsVerbosity() == diagnostics::VerbosityLevelStats::Quiet)) {
			std::cerr << "\n--------------------------------------------------------------------\n";
			std::cerr << "🧮 PRE-RUN SUMMARY for feed: " << settings.InputPath().filename().string()
			          << " (sample=" << settings.getPreRunSampleSize() << " rows/file)\n\n";

			if (settings.getStatsVerbosity() == diagnostics::VerbosityLevelStats::Verbose) {
				for (const auto& st : per_file_stats) {
					if (st.name.empty())
						continue; // if some entries unused

					std::cerr << "• " << st.name << "  rows=" << st.rows << "  est≈"
					          << fmt_suffix_padded(st.triples, UnitType::Counts) << " triples"
					          << "  output size≈"
					          << fmt_suffix_padded(st.num_chars, UnitType::Sizes) << "\n"
					          << "  header: " << st.header << "\n\n";
				}
				std::cerr << "Note: Header fields in (parentheses) were not used in any triple "
				             "generation.\n\n";
			}

			// total summary
			std::cerr
			    << "Summary\n"
			    << "  total rows:   " << total_stats.rows << "\n"
			    << "  est triples:  " << fmt_suffix_padded(total_stats.triples, UnitType::Counts)
			    << " triples\n"
#if GTFS2RDF_FULL_STATS
			    << "  est parse time: " << fmt_suffix_padded(total_stats.parse_ns, UnitType::Time)
			    << "\n"
			    << "  est conversion time: "
			    << fmt_suffix_padded(total_stats.conversion_ns, UnitType::Time) << "\n"
			    << "  est run time: "
			    << fmt_suffix_padded(total_stats.parse_ns + total_stats.write_ns +
			                             total_stats.conversion_ns,
			                         UnitType::Time)
			    << "\n"
#endif
			    << "  est peak RAM usage: "
			    << fmt_suffix_padded(settings.estimatedPeakRAMMB() * 1024 * 1024, UnitType::Sizes)
			    << "\n"
			    << "  est. output size: "
			    << fmt_suffix_padded(total_stats.num_chars, UnitType::Sizes) << "\n\n"
			    << " ⚠️  Note: These are only estimates based on a sample data run. Actual output "
			       "may vary significantly depending on the data present in the GTFS feed as "
			       "well as Transform2Many and filtering."
			    << "\n--------------------------------------------------------------------\n";
		} else if (settings.getStatsVerbosity() == diagnostics::VerbosityLevelStats::Verbose) {
			for (const auto& st : per_file_stats) {
				std::cerr << st.fancyPrint() << "\n";
			}
			rt.getStorage().stats();
			std::cerr << total_stats.fancyPrint() << "\n";
		} else if (settings.getStatsVerbosity() == diagnostics::VerbosityLevelStats::Brief) {
			std::cerr << total_stats.fancyPrint() << "\n";
		}
		if (!settings.isPreRun()) {
			std::cerr << "✅  Successful conversion to output file " << settings.OutputPath()
			          << ".\n";
		} else {
			std::cerr << "✅  No errors found during pre-run analysis of GTFS feed "
			          << settings.InputPath() << ".\n";
		}
	} catch (const diagnostics::Error& e) {
		diagnostics::print_error_chain(e);
		std::cerr << "\nRun aborted due to errors. No output was generated or it may be faulty.\n";
		return 1;
	} catch (const std::exception& e) {
		std::cerr << "❌  Unhandled exception: " << e.what() << "\n";
		return 1;
	}

	return 0;
}