// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the gtfs2rdf project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.

#include "util/diagnostics.h"

#include <iostream>
#include <memory>
#include <unordered_map>

import gtfs2rdf_runner;
import runtime;
import schema;

using Factory = schema::Factory;
const auto& factories = schema::factories(); // implemented in schema:registry at build time

int main(int argc, char* argv[]) {
	std::unique_ptr<runtime::Settings> settings_ptr;
	try {
		settings_ptr = std::make_unique<runtime::Settings>(argc, argv);
	} catch (const std::exception& excpt) {
		diagnostics::printErrorChain(excpt);
		std::cerr << "\nRun aborted due to errors. No output was generated or it may be faulty.\n";
		return 1;
	}

	diagnostics::WarningCollector wcol(settings_ptr->getWarningsVerbosity());

	try {
		return gtfs2rdf_runner::gtfs2rdf(*settings_ptr, wcol, factories);
	} catch (const std::exception& excpt) {
		wcol.printWarningSummary();
		diagnostics::printErrorChain(excpt);
		std::cerr << "\nRun aborted due to errors. No output was generated or it may be faulty.\n";
		return 1;
	}
}