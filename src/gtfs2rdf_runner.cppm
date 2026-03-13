// SPDX-FileCopyrightText: 2026 Jan Babin
// SPDX-License-Identifier: AGPL-3.0-only
//
// Part of gtfs2rdf. See the LICENSE file for details.

module;

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

export module gtfs2rdf_runner;

namespace gtfs2rdf_runner {

using namespace util;

// type for schema factories
export using Factories = std::unordered_map<std::string, schema::Factory>;

export int gtfs2rdf(const runtime::Settings& settings,
                    diagnostics::WarningCollector& wcol,
                    const Factories& factories) {
	zip_t* z_arch;
	int err;

	// try opening the zip file
	if ((z_arch = zip_open(settings.getInputPath().string().c_str(), ZIP_RDONLY, &err)) ==
	    nullptr) {
		zip_error_t error;
		zip_error_init_with_code(&error, err);
		zip_error_fini(&error);
		throw diagnostics::Error("IO error: cannot open zip archive '" +
		                         settings.getInputPath().string() + "' (error '" +
		                         std::string(zip_error_strerror(&error)) + "')");
	}

	std::vector<std::string> files_in_dir;
	std::vector<schema::Schema> used_schemas;
	std::unordered_map<std::string, size_t> schema_name_to_index; // map from schema name to index
	field_transforms::TransformRegistry registry;
	t_lib::registerLibTransforms(registry);
	runtime::RuntimeContainer rtc(settings, wcol, registry);

	// determine which schemas to use based on files present in the zip
	for (const auto& [file, factory] : factories) {
		if (zip_name_locate(z_arch, file.c_str(), ZIP_FL_ENC_GUESS) != -1) {
			files_in_dir.push_back(file);
			try {
				used_schemas.emplace_back(factory(rtc)); // call factory
				schema_name_to_index[used_schemas.back().getName()] = used_schemas.size() - 1;
				rtc.getWarningCollector().addNode("while building schema for file '" + file + "'",
				                                  1);
			} catch (const std::exception& excpt) {
				zip_close(z_arch);
				diagnostics::wrapAndRethrow("while building schema for file '" + file + "'");
			}
		}
	}

	// error if no files found that correspond to known schemas
	if (files_in_dir.empty()) {
		zip_close(z_arch);
		throw diagnostics::Error(
		    "IO error: no Gtfs files corresponding to known schemas found in zip archive '" +
		    settings.getInputPath().string() + "'.\n");
	}

	// compile schemas such that they are ready for use and dependency info is available
	for (auto& sch : used_schemas) {
		try {
			sch.compile();
			rtc.getWarningCollector().addNode("while compiling schema '" + sch.getName() + "'", 2);
		} catch (const std::exception& excpt) {
			zip_close(z_arch);
			diagnostics::wrapAndRethrow("while compiling schema '" + sch.getName() + "'");
		}
	}

	// determine processing order via topological sort of dependencies
	// this whole section is not very efficient, but then again we're looking at Gtfs feeds
	// such that n < 50 in practice
	TopologicalSort toposort(files_in_dir.size());
	std::vector<size_t> num_depending_schemas(files_in_dir.size(), 0);
	for (size_t i = 0; i < files_in_dir.size(); i++) {
		toposort.addNode(i);
		const auto& deps = used_schemas[i].getDependencies();
		if (deps.empty()) {
			continue;
		}
		std::string dep_list;
		for (const auto& dep : deps) {
			if (!schema_name_to_index.contains(dep)) {
				zip_close(z_arch);
				throw diagnostics::Error("Schema error: dependency '" + dep + "' of schema '" +
				                         used_schemas[i].getName() + "' not found in Gtfs feed");
			}
			toposort.addEdge(schema_name_to_index[dep], i, true); // dep must come before i
			num_depending_schemas[schema_name_to_index[dep]]++;
			dep_list += dep + " ";
		}
		rtc.getWarningCollector().addLeaf("Schema '" + used_schemas[i].getName() + "' depends on " +
		                                      dep_list,
		                                  diagnostics::WarningLevel::DEBUG,
		                                  true);
	}

	// deactivate all storage writes from schemas that are not needed later on
	for (size_t i = 0; i < files_in_dir.size(); i++) {
		if (num_depending_schemas[i] == 0) {
			used_schemas[i].forbidStorageWrites();
			rtc.getWarningCollector().addLeaf("Deactivated storage writes for schema '" +
			                                      used_schemas[i].getName() +
			                                      "' (not required by another schema)",
			                                  diagnostics::WarningLevel::DEBUG,
			                                  true);
		}
	}

	// allow loops (schema may read from its own previously written storage)
	std::vector<size_t> order(files_in_dir.size());
	try {
		order = toposort.sort();
		rtc.getWarningCollector().addNode("while determining processing order of Gtfs files", 2);
	} catch (const std::runtime_error& err) {
		zip_close(z_arch);
		size_t error_node = toposort.getErrorNode();
		throw diagnostics::Error("TopologicalSort error: While determining processing order of "
		                         "Gtfs files. Problematic schema: '" +
		                         used_schemas[error_node].getName() + "'");
	}

	rtc.getWarningCollector().addLeaf(
	    "Processing Gtfs files in order: ", diagnostics::WarningLevel::DEBUG, true);
	for (size_t i = 0; i < files_in_dir.size(); i++) {
		rtc.getWarningCollector().appendToLeaf(files_in_dir[order[i]],
		                                       diagnostics::WarningLevel::DEBUG);
	}

	writer::Writer writer =
	    settings.isPreRun()
	        ? writer::Writer(std::cout,
	                         rtc,
	                         1.0,
	                         false) // pre-run: don't write actual output, only small alibi buffer
	    : settings.isOutputToStdout()
	        ? writer::Writer(std::cout, rtc, true)                 // normal run to stdout
	        : writer::Writer(settings.getOutputPath(), rtc, true); // normal run to file output

	gtfs::GtfsParserWorkspace wsp(rtc, writer);
	auto merged_prefixes = schema::mergePrefixes(used_schemas, rtc.getWarningCollector(), true);
	std::vector<diagnostics::Statistics> per_file_stats(files_in_dir.size());
	for (size_t i = 0; i < files_in_dir.size(); i++) {
		zip_file_t* z_file = nullptr;
		try {
			z_file = zip_fopen(z_arch, files_in_dir[order[i]].c_str(), 0);
			if (!z_file) {
				zip_close(z_arch);
				throw diagnostics::Error("IO error: cannot open entry inside ZIP archive");
			}
			if (i == 0 && !settings.isNTriplesOutput() && !settings.isPreRun()) {
				writer.writePrefixes(merged_prefixes);
			}
			gtfs::GtfsParser parser(z_file, used_schemas[order[i]], wsp, rtc);
			parser.parse();
			for (const auto& dep : used_schemas[order[i]].getDependencies()) {
#ifdef NDEBUG
				num_depending_schemas[schema_name_to_index[dep]]--;
#endif
				if (num_depending_schemas[schema_name_to_index[dep]] == 0) {
					rtc.getStorage().clearContext(dep);
					rtc.getWarningCollector().addLeaf(
					    "Cleared storage of schema '" + dep + "' after last dependent schema '" +
					        used_schemas[order[i]].getName() + "' was processed.",
					    diagnostics::WarningLevel::DEBUG,
					    true);
				}
			}
			zip_fclose(z_file);
			if (settings.isPreRun()) {
				diagnostics::Statistics stats;
				stats.name = files_in_dir[order[i]];
				// NOLINT(bugprone-narrowing-conversions)
				const double QUOT = static_cast<double>(parser.getStats().rows) /
				                    static_cast<double>(settings.getPreRunSampleSize());
				// NOLINT(bugprone-narrowing-conversions)
				stats.triples =
				    QUOT < 1.0 ? parser.getStats().triples : QUOT * parser.getStats().triples;
				stats.rows = parser.getStats().rows;
				stats.header = used_schemas[order[i]].formatHeaderWithUnused();
				// NOLINT(bugprone-narrowing-conversions)
				stats.num_chars =
				    QUOT < 1.0 ? parser.getStats().num_chars : QUOT * parser.getStats().num_chars;
#if GTFS2RDF_FULL_STATS
				// NOLINT(bugprone-narrowing-conversions)
				stats.write_ns =
				    QUOT < 1.0 ? parser.getStats().write_ns : QUOT * parser.getStats().write_ns;
				// NOLINT(bugprone-narrowing-conversions)
				stats.parse_ns = parser.getStats().parse_ns;
				// NOLINT(bugprone-narrowing-conversions)
				stats.conversion_ns = QUOT < 1.0 ? parser.getStats().conversion_ns
				                                 : QUOT * parser.getStats().conversion_ns;
#endif
				per_file_stats[order[i]] = stats;
			} else {
				per_file_stats[order[i]] = parser.getStats();
			}

			rtc.getWarningCollector().addNode(
			    "while processing Gtfs file '" + files_in_dir[order[i]] + "'", 2);
		} catch (const std::exception& excpt) {
			zip_close(z_arch);
			zip_fclose(z_file);
			writer.deleteFile(); // output will be faulty
			diagnostics::wrapAndRethrow("while processing Gtfs file '" + files_in_dir[order[i]] +
			                            "'");
		}
	}
	zip_close(z_arch);

	diagnostics::Statistics total_stats;
	total_stats.name = "Total Gtfs Feed";
	for (size_t i = 0; i < files_in_dir.size(); i++) {
		total_stats = total_stats + per_file_stats[i];
	}

	// dump ontology spec if requested
	if (settings.isSpecDump() && !used_schemas.empty()) {
		std::filesystem::path spec_path = settings.getOutputPath();
		spec_path.replace_extension(".spec.txt");
		writer::Writer onth_writer(spec_path, rtc, 1.0); // small buffer for spec writing
		if (!settings.isNTriplesOutput()) {
			onth_writer.writePrefixes(merged_prefixes);
		}
		for (auto& schema : used_schemas) {
			for (auto& inst : schema.getInstructions()) {
				onth_writer.writeRaw(inst.getRawInstruction() + "\n");
			}
		}
		rtc.getWarningCollector().addLeaf(
		    "Wrote ontology spec to " + spec_path.string(), diagnostics::WarningLevel::DEBUG, true);
	}

	rtc.getWarningCollector().printWarningSummary();

	if (settings.isPreRun() &&
	    !(settings.getStatsVerbosity() == diagnostics::VerbosityLevelStats::QUIET)) {
		// std::cerr <<
		// "\n--------------------------------------------------------------------\n\n";
		std::cerr << "🧮 PRE-RUN SUMMARY for feed: " << settings.getInputPath().filename().string()
		          << " (sample=" << settings.getPreRunSampleSize() << " rows/file)\n\n";

		if (settings.getStatsVerbosity() == diagnostics::VerbosityLevelStats::VERBOSE) {
			for (const auto& f_stats : per_file_stats) {
				if (f_stats.name.empty()) {
					continue; // if some entries unused
				}
				std::cerr << "• " << f_stats.name << "  rows=" << f_stats.rows << "  est≈"
				          << formatValueWithPaddedUnits(f_stats.triples, UnitType::COUNT)
				          << " triples" << "  output size≈"
				          << formatValueWithPaddedUnits(f_stats.num_chars, UnitType::SIZE) << "\n"
				          << "  header: " << f_stats.header << "\n\n";
			}
			std::cerr
			    << "Note:\n"
			    << "  - Header fields in (parentheses) were not used in any triple generation.\n"
			    << "  - If a file shows 0 generated triples, it might still have contributed to "
			       "other files' output via cross-file references\n\n";
		}

		// total summary
		std::cerr
		    << "Summary\n"
		    << "  total rows:   " << total_stats.rows << "\n"
		    << "  est triples:  "
		    << formatValueWithPaddedUnits(total_stats.triples, UnitType::COUNT) << " triples\n"
#if GTFS2RDF_FULL_STATS
		    << "  est parse time: "
		    << formatValueWithPaddedUnits(total_stats.parse_ns, UnitType::TIME) << "\n"
		    << "  est conversion time: "
		    << formatValueWithPaddedUnits(total_stats.conversion_ns, UnitType::TIME) << "\n"
		    << "  est run time: "
		    << formatValueWithPaddedUnits(total_stats.parse_ns + total_stats.write_ns +
		                                      total_stats.conversion_ns,
		                                  UnitType::TIME)
		    << "\n"
#endif
		    << "  est peak RAM usage: "
		    // NOLINT(bugprone-narrowing-conversions, readability-magic-numbers)
		    << formatValueWithPaddedUnits(settings.getEstimatedPeakRAM_MB() * 1024 * 1024,
		                                  UnitType::SIZE)
		    << "\n"
		    << "  est output size: "
		    << formatValueWithPaddedUnits(total_stats.num_chars, UnitType::SIZE) << "\n\n"
		    << " ⚠️  Note: These are only estimates based on a sample data run. Actual output "
		       "may vary significantly depending on the data present in the Gtfs feed as "
		       "well as Transform2Many and filtering."
		    //   << "\n--------------------------------------------------------------------\n";
		    << "\n\n";
	} else if (settings.getStatsVerbosity() == diagnostics::VerbosityLevelStats::VERBOSE) {
		for (const auto& f_stats : per_file_stats) {
			std::cerr << f_stats.fancyPrint() << "\n";
		}
		rtc.getStorage().stats();
		std::cerr << total_stats.fancyPrint() << "\n";
	} else if (settings.getStatsVerbosity() == diagnostics::VerbosityLevelStats::BRIEF) {
		std::cerr << total_stats.fancyPrint() << "\n";
	}
	if (!settings.isPreRun()) {
		std::cerr << "✅  Successful conversion to " << settings.getOutputPath() << ".\n";
	} else {
		std::cerr << "✅  No errors found during pre-run analysis of Gtfs feed "
		          << settings.getInputPath() << ".\n";
	}

	return 0;
}

} // namespace gtfs2rdf_runner