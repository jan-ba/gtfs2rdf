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
