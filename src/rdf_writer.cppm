// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the GTFS2RDF project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.


module;

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <optional>
#include <cctype>

export module rdf_writer;
import schema.core;

using namespace schema;

namespace ttl {

void writePrefixes(std::ostream& os, const Schema& sc) {
  if (!sc.isPrefixes()) return; // ntriples später
  for (const auto& [pfx, iri] : sc.getPrefixes()) {
    os << "@prefix " << pfx << ": <" << iri << "> .\n";
  }
  os << "\n";
}

// TODO: perhaps this should take write options as parameters?
export long long write2TTL(const Schema& sc, const std::vector<std::vector<std::string>>& rows,
               std::ostream& os, bool first_batch_overall) {
  long long counter = 0;
  std::vector<Instruction> instructions = sc.getInstructions();
  if (first_batch_overall) { writePrefixes(os, sc); }

  for (const auto& row : rows) {
    for (auto& inst : instructions) {
      os << inst.render(row);
    }
  }
  for (const auto& inst : instructions) {
    counter += inst.getCount();
  }
  return counter;
}

} // namespace
