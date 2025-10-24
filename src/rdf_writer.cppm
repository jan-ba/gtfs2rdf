module;

#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <optional>
#include <cctype>

export module rdf_writer;
import rdf_schema;

using namespace rdf_schema;

namespace util {

inline bool isUnreserved(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c=='-' || c=='.' || c=='_' || c=='~';
}

std::string percentEncode(const std::string& s) {
  static const char* hex = "0123456789ABCDEF";
  std::string out;
  out.reserve(s.size()*3);
  for (unsigned char c : s) {
    if (isUnreserved(static_cast<char>(c))) {
      out.push_back(static_cast<char>(c));
    } else {
      out.push_back('%');
      out.push_back(hex[c >> 4]);
      out.push_back(hex[c & 15]);
    }
  }
  return out;
}

std::string escapeLiteral(const std::string& s) {
  std::string out; out.reserve(s.size()+8);
  for (char c : s) {
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"':  out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += c; break;
    }
  }
  return out;
}

void writePrefixes(std::ostream& os, const Schema& sc) {
  if (!sc.isPrefixes()) return; // ntriples später
  for (const auto& [pfx, iri] : sc.getPrefixes()) {
    os << "@prefix " << pfx << ": <" << iri << "> .\n";
  }
  os << "\n";
}

} // namespace

export namespace ttl {

int write2TTL(const Schema& sc, const std::vector<std::vector<std::string>>& rows,
               std::ostream& os) {
  using namespace util;
  int counter = 0;
  std::vector<Instruction> instructions = sc.getInstructions();
  writePrefixes(os, sc);

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
