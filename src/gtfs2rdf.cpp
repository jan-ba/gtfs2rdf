// SPDX-FileCopyrightText: 2026 Jan Babin
// SPDX-License-Identifier: AGPL-3.0-only
//
// Part of gtfs2rdf. See the LICENSE file for details.

#include "util/diagnostics.h"

#include <iostream>
#include <memory>
#include <unordered_map>

import gtfs2rdf_runner;
import runtime;
import schema;

using Factory = schema::Factory;
const auto& factories = schema::factories(); // implemented in schema:registry at build time (cmake)

int main(int argc, char* argv[]) {
	std::unique_ptr<runtime::Settings> settings_ptr;

	// this is to be able to use the WarningCollector (that depends on the settings) to report any
	// warnings in case of an error / exception
	// if settings and main runner were in the same try block, any exception thrown would prevent
	// the WarningCollector from being used to report any warnings that may have been collected up
	// since it could not be called from outside the try scope (it has to be constructed between
	// settings and main runner)
	try {
		settings_ptr = std::make_unique<runtime::Settings>(argc, argv);
	} catch (const std::exception& excpt) {
		diagnostics::printErrorChain(excpt);
		std::cerr << "\nRun aborted due to errors. No output was generated or it may be faulty.\n";
		return 1;
	}

	diagnostics::WarningCollector wcol(settings_ptr->getWarningsVerbosity());

	// main program run
	try {
		return gtfs2rdf_runner::gtfs2rdf(*settings_ptr, wcol, factories);
	} catch (const std::exception& excpt) {
		wcol.printWarningSummary();
		diagnostics::printErrorChain(excpt);
		std::cerr << "\nRun aborted due to errors. No output was generated or it may be faulty.\n";
		return 1;
	}
}