module;

#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <functional>
#include <algorithm>
#include <span>

#include <iostream>

export module field_transforms;
import utility;
using namespace util;


namespace field_transforms {

export const int MaxArgs = 5;  // expected maximum number of arguments for field transforms

// expected maximum number of chained transforms per placeholder - if more are needed, consider 
// chaining transforms inside a single transform function for performance
export const int MaxTransforms = 3;

export class ArgSpan {
    private:
        const std::string* const* const data_;
        const size_t size_ = 0;
        
    public:
        ArgSpan(const std::string* const* data, size_t size) : data_(data), size_(size) {}

        size_t size() const { return size_; }

        const std::string& operator[](size_t index) const {
            if (index >= size_) {
                throw std::out_of_range("Invalid index access to ArgSpan");
            }
            return *data_[index];
        }

        const std::string* const* data() const { return data_; }

        // iterators
        const std::string* const* begin() const { return data_; }
        const std::string* const* end()   const { return data_ + size_; }
};

export using Transform = std::function<void(const ArgSpan&, std::string&)>;

    // return type: field name + list of functors.
export struct ParsedPlaceholder {
    std::vector<std::string> field_names;
    std::vector<Transform> transforms;
};

export class TransformRegistry {
  public:
    TransformRegistry() {

    }
   
    void registerTransform(const std::string& name, Transform fn) {
        if (registry_.contains(name)) {
            throw std::runtime_error("❌  Transform error: field transform already registered: " + name);
        }

        registry_[name] = fn;
    }

    const ParsedPlaceholder parse_placeholder_with_functors(const std::string& raw) const
    {
        ParsedPlaceholder result;

        // remove whitespace
        std::string s = raw;
        s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) { return c == ' '; }), 
                               s.end());

        // find first '|'
        std::size_t pos = s.find('|');
        if (pos == std::string::npos) {  // no '|', only field name
            result.field_names.push_back(s);
            return result;
        }

        // field name = part before first '|'
        result.field_names = util::split(s.substr(0, pos), ',');

        auto transforms = util::split(s.substr(pos + 1), '|');

        for (const auto& name : transforms) {
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

    Transform getTransform(const std::string& name) const {
        if (!registry_.contains(name)) {
            throw std::runtime_error("❌  Error: unknown field transform: " + name);
        }
        return registry_.at(name);
    }

  private:
    std::unordered_map<std::string, Transform> registry_;
};

}  // namespace