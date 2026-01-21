module;

#include <string>
#include <vector>
#include <cctype>
#include <string_view>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <sstream>

export module util:strings;

export namespace util {

// parse string in format "YYYYMMDD" into chrono::sys_days
std::chrono::sys_days parseYYYYMMDD(const std::string& s) {
    std::istringstream ss(s);
    std::chrono::sys_days dp{};
    ss >> std::chrono::parse("%Y%m%d", dp);
    return dp;
}

// splits a string by a given delimiter character
std::vector<std::string> split(const std::string& str, const char delimiter) {
    std::vector<std::string> tokens;
    std::string current;

    for (char c : str) {
        if (c == delimiter) {
            tokens.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }

    // add the last token (even if it's empty)
    tokens.push_back(current);

    return tokens;
}

// removes whitespace inplace from a string
void remove_ws(std::string& str) {
    str.erase(std::remove_if(str.begin(), str.end(), 
                             [](unsigned char c) { return std::isspace(c); }), 
                             str.end());
}

// removes whitespace outside of quoted substrings
std::string remove_ws_outside_quotes(std::string_view str) {
  std::string out;
  out.reserve(str.size());
  bool in_q = false;
  for (size_t i = 0; i < str.size(); ++i) {
    char c = str[i];
    if (c == '"' && (i == 0 || str[i-1] != '\\')) in_q = !in_q;
    if (!in_q && std::isspace((unsigned char)c)) continue;
    out.push_back(c);
  }
  return out;
}

// returns a concatenation string of a vector of strings
std::string concat(const std::vector<std::string>& vec, const std::string& delimiter = "") {
    std::string result;
    for (size_t i = 0; i < vec.size(); ++i) {
        result += vec[i];
        if (i < vec.size() - 1) {
            result += delimiter;
        }
    }
    return result;
}

// returns all occurences of substrings that are enclosed between 'start_delim' and 'end_delim'
// throws an error if delimiters are unbalanced
// invariant: nested delimiters are not supported
std::vector<std::string> extract_enclosed_substrings(const std::string& str,
    const std::string& start_delim, const std::string& end_delim) 
{
    std::vector<std::string> results;

    // if there is an end delimiter but no start delimiter at all -> unbalanced
    if (str.find(end_delim) != std::string::npos &&
        str.find(start_delim) == std::string::npos) {
        throw std::runtime_error("❌  Error: unbalanced delimiters in string: " + str);
    }

    size_t start_search = 0;
    size_t last_consumed = 0;

    while (true) {
        size_t pos = str.find(start_delim, start_search);
        if (pos == std::string::npos) break;

        size_t end = str.find(end_delim, pos + start_delim.size());
        if (end == std::string::npos) {
            throw std::runtime_error("❌  Error: unbalanced delimiters in string: " + str);
        }

        results.push_back(str.substr(pos + start_delim.size(),
                                     end - (pos + start_delim.size())));

        last_consumed = end + end_delim.size();
        start_search = last_consumed;
    }

    // any stray end delimiter after the last consumed block -> unbalanced
    if (str.find(end_delim, last_consumed) != std::string::npos) {
        throw std::runtime_error("❌  Error: unbalanced delimiters in string: " + str);
    }

    return results;
}

// finds the position of a delimiter character at the top level (not inside quotes or parentheses)
size_t find_top_level(std::string_view s, char delimiter, size_t from = 0) {
  bool in_q = false;
  int paren = 0;
  for (size_t i = from; i < s.size(); ++i) {
    char c = s[i];
    if (c == '"' && (i == 0 || s[i-1] != '\\')) in_q = !in_q;
    if (in_q) continue;
    if (c == '(') ++paren;
    else if (c == ')') --paren;
    else if (paren == 0 && c == delimiter) return i;
  }
  return std::string_view::npos;
}

// splits a string by a given delimiter character only at the top level
std::vector<std::string_view> split_top_level(std::string_view s, char delimiter) {
  std::vector<std::string_view> out;
  size_t start = 0;
  while (start <= s.size()) {
    size_t pos = find_top_level(s, delimiter, start);
    if (pos == std::string_view::npos) {
      auto tok = s.substr(start);
      if (!tok.empty()) out.push_back(tok);
      break;
    }
    auto tok = s.substr(start, pos - start);
    if (!tok.empty()) out.push_back(tok);
    start = pos + 1;
  }
  return out;
}

// splits a string once at the top level by a given delimiter character into a pair
std::pair<std::string_view,std::string_view> split_once_top_level(std::string_view s, 
                                                                         char delimiter) {
  size_t pos = find_top_level(s, delimiter, 0);
  if (pos == std::string_view::npos) return {s, std::string_view{}};
  return {s.substr(0,pos), s.substr(pos+1)};
}

// removes surrounding quotes and unescapes minimal escape sequences
std::string unquote(std::string_view tok) {
  if (tok.size() >= 2 && tok.front() == '"' && tok.back() == '"') {
    std::string out;
    out.reserve(tok.size()-2);
    for (size_t i = 1; i + 1 < tok.size(); ++i) {
      char c = tok[i];
      if (c == '\\' && i + 1 < tok.size()-1) {
        char n = tok[i+1];
        // minimal escapes
        if (n == '"' || n == '\\') { out.push_back(n); ++i; continue; }
      }
      out.push_back(c);
    }
    return out;
  }
  return std::string(tok);
}

// splits a string at the first occurrence of a delimiter character
std::pair<std::string_view,std::string_view> split_at(std::string_view s, char delimiter) {
size_t pos = s.find(delimiter);
  if (pos == std::string_view::npos) return {s, {}};
  return {s.substr(0,pos), s.substr(pos+1)};
}

}  // namespace util::strings