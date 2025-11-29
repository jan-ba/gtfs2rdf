module;

#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <functional>
#include <algorithm>

export module field_transforms;

// library of field transforms that could be useful for multiple schemas
// functions will need to be registered in the constructor of TransformRegistry in order to be available
namespace transforms {


    // transforms a time string "HH:MM:SS" into total seconds from midnight as string
    void time_to_seconds(std::string& s) {
        // s is "HH:MM:SS" (or H:MM:SS)
        int h = 0, m = 0, sec = 0;
        if (std::sscanf(s.c_str(), "%d:%d:%d", &h, &m, &sec) != 3) {
            throw std::runtime_error("❌ Transform error: invalid time '" + s + "'");
        }
        int total = h * 3600 + m * 60 + sec;
        s = std::to_string(total);
    }
}




using fn = std::function<void(std::string&)>;

namespace field_transforms {

    // return type: field name + list of functors.
export struct ParsedPlaceholder {
    std::string field_name;
    std::vector<fn> transforms;
};

export class TransformRegistry {
  public:
    TransformRegistry() {
        // register built-in transforms
        registerTransform("time_to_seconds", transforms::time_to_seconds);
    }
   
    void registerTransform(const std::string& name, fn fn) {
        if (registry_.contains(name)) {
            throw std::runtime_error("❌  Transform error: field transform already registered: " + name);
        }

        registry_[name] = fn;
    }

    ParsedPlaceholder parse_placeholder_with_functors(const std::string& raw) const
    {
        ParsedPlaceholder result;

        // remove whitespace
        std::string s = raw;
        s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) { return c == ' '; }), 
                               s.end());

        // find first '|'
        std::size_t pos = s.find('|');
        if (pos == std::string::npos) {  // no '|', only field name
            result.field_name = s;
            return result;
        }

        // field name = part before first '|'
        result.field_name = s.substr(0, pos);

        // Parse transform names after the first '|'
        std::size_t start = pos + 1;
        while (start < s.size()) {
            std::size_t next = s.find('|', start);
            std::string name;
            if (next == std::string::npos) {
                name = s.substr(start);
                start = s.size();
            } else {
                name = s.substr(start, next - start);
                start = next + 1;
            }

            if (name.empty()) {
                // skip empty transform names
                continue;
            }

            if (!registry_.contains(name)) {
                throw std::runtime_error("❌  Error: unknown field transform: " + name);
            }

            result.transforms.push_back(registry_.at(name));
        }

        return result;
    }

    fn getTransform(const std::string& name) const {
        if (!registry_.contains(name)) {
            throw std::runtime_error("❌  Error: unknown field transform: " + name);
        }
        return registry_.at(name);
    }

  private:
    std::unordered_map<std::string, fn> registry_;
};

}  // namespace