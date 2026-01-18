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
import util;
using namespace util;

namespace field_transforms {

export const int MaxArgs = 9;  // expected maximum number of arguments for field transforms
                               // 9 since this accounts for all days of the week in calendar.txt

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
                throw std::out_of_range("❌ Transform error: Invalid index access to ArgSpan");
            }
            return *data_[index];
        }

        const std::string* const* data() const { return data_; }

        // iterators
        const std::string* const* begin() const { return data_; }
        const std::string* const* end()   const { return data_ + size_; }
};

export using Transform2One = std::function<void(const ArgSpan&, std::string&)>;
export using Transform2N = std::function<void(const ArgSpan&, std::vector<std::string>&)>;

export enum class TransformKind { Single, Multi };

export struct Transform {
    TransformKind kind;
    Transform2One single;  // valid if kind == Single
    Transform2N   multi;  // valid if kind == Multi
};

// this denotes how the output of a placeholder is to be handled
// store to FILE in a rdf triple (standard) or store internally for later use:
// CONSTANT: single constant value
// MULTIMAP: multiple values per input row (e.g. for calendar_dates.txt exceptions)
// TUPLEMAP: multiple tuples of values per input row (e.g. for translations.txt)
export enum class OutputDestination { CONSTANT, MULTIMAP, TUPLEMAP, FILE };

export class StorageInstruction {
  private:
    const std::string* context_name_ = nullptr;  // context (schema/file) name
    std::string target_name_;  // name of constant/multimap/tuplemap
    OutputDestination output_destination_ = OutputDestination::FILE;
    bool write_ = false;  // whether to write to storage (else, just read) (only if OutputDestination != FILE)

  public:
    StorageInstruction(const std::string& target_name) 
        : target_name_(target_name) {
        output_destination_ = OutputDestination::CONSTANT;
    }

    StorageInstruction() 
        : target_name_(""), output_destination_(OutputDestination::FILE) {}

    void setContextName(const std::string& context_name) {
        context_name_ = &context_name;
    }

    const std::string& getContextName() const {
        if (context_name_ == nullptr) {
            throw std::runtime_error("❌  Error: StorageInstruction context name not set");
        }
        return *context_name_;
    }

    const std::string& getTargetName() const {
        return target_name_;
    }

    OutputDestination getOutputDestination() const {
        return output_destination_;
    }
};

// return type: field name + list of functors.
export struct ParsedPlaceholder {
    std::vector<std::string> field_names;
    std::vector<Transform> transforms;
    StorageInstruction storage_instruction;
};


export class TransformRegistry {
  private:
    // function that finds permitted characters in transform names
    // these include: ':', ' ',', '|', '>', '{}', '}' (TODO: to be extended in the future?)
    bool is_permitted_name(const std::string& s) const {
        for (char c : s) {
            if (!(c == '_' || ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || ('0' <= c && c <= '9'))) {
                return false;
            }
        }
        return true;
    }

  public:   
    void registerTransform(const std::string& name, Transform2One fn) {
        if (registry_.contains(name)) {
            throw std::runtime_error("❌  Transform error: field transform already registered: " + name);
        }
        if (!is_permitted_name(name)) {
            throw std::runtime_error("❌  Transform error: invalid characters in transform name: " + name);
        }
        registry_[name] = Transform{TransformKind::Single, fn, {}};
    }

    void registerTransform(const std::string& name, Transform2N fn) {
        if (registry_.contains(name)) {
            throw std::runtime_error("❌  Transform error: field transform already registered: " + name);
        }
        if (!is_permitted_name(name)) {
            throw std::runtime_error("❌  Transform error: invalid characters in transform name: " + name);
        }
        registry_[name] = Transform{TransformKind::Multi, {}, fn};
    }

    const Transform& getTransform(const std::string& name) const {
        if (!registry_.contains(name)) {
            throw std::runtime_error("❌  Error: unknown field transform: " + name);
        }
        return registry_.at(name);
    }

    const ParsedPlaceholder parse_placeholder_with_functors(const std::string& raw) const
    {
        ParsedPlaceholder result;

        // remove whitespace
        std::string s = raw;

        util::remove_whitespace(s);

        // find first '>' if any (then, this placeholder is meant for persistent storage)
        size_t dest_pos = s.find('>');
        std::vector<std::string> tmp;
        if (dest_pos != std::string::npos) {
            tmp = util::split(s, '>');
            if (tmp.size() != 2) {
                throw std::runtime_error("❌  Transform error: invalid placeholder syntax (multiple '>'): " + raw);
            }
            result.storage_instruction = StorageInstruction(tmp[1]);
            s = tmp[0];
        }


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

            result.transforms.push_back(getTransform(name));
        }
        return result;
    }

  private:
    std::unordered_map<std::string, Transform> registry_;
};

}  // namespace