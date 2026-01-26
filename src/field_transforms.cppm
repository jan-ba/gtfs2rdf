module;

#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <functional>
#include <algorithm>
#include <span>
#include <ostream>

#include <iostream>

export module field_transforms;
import util;
using namespace util;

namespace field_transforms {

export const int MaxArgs = 10;  // expected maximum number of arguments for field transforms
                               // 10 since this is required by calendar.txt generate_dates transform

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

        // convenience print function for debugging
        friend std::ostream& operator<<(std::ostream& os, const ArgSpan& span) {
            os << "ArgSpan[";
            for (size_t i = 0; i < span.size_; ++i) {
                if (i > 0) os << ", ";
                os << *span.data_[i];
            }
            os << "]";
            return os;
        }
};

export using Args = field_transforms::ArgSpan;
export using Out1 = std::string;
export using OutN = std::vector<std::string>;

export using Transform2One = std::function<void(const Args&, Out1&)>;
export using Transform2Many = std::function<void(const Args&, OutN&)>;

export enum class TransformKind { Single, Multi };

export struct Transform {
    TransformKind kind;
    Transform2One single;  // valid if kind == Single
    Transform2Many   multi;  // valid if kind == Multi
};

export class TransformRegistry {
  public:   
    void registerTransform(const std::string& name, Transform2One fn) {
        if (registry_.contains(name)) {
            throw std::runtime_error("❌  Transform error: field transform already registered: " + name);
        }
        if (!is_permitted_name_(name)) {
            throw std::runtime_error("❌  Transform error: invalid characters in transform name: " + name);
        }
        registry_[name] = Transform{TransformKind::Single, fn, {}};
    }

    void registerTransform(const std::string& name, Transform2Many fn) {
        if (registry_.contains(name)) {
            throw std::runtime_error("❌  Transform error: field transform already registered: " + name);
        }
        if (!is_permitted_name_(name)) {
            throw std::runtime_error("❌  Transform error: invalid characters in transform name: " + name);
        }
        registry_[name] = Transform{TransformKind::Multi, {}, fn};
    }

    const Transform& getTransform(const std::string& name) const {
        if (!registry_.contains(name)) {
            // TODO: add context information perhaps
            throw std::runtime_error("❌  Error: unknown field transform: " + name);
        }
        return registry_.at(name);
    }

  private:
    std::unordered_map<std::string, Transform> registry_;

    // function that finds permitted characters in transform names
    // these include: a-z, A-Z, 0-9, _
    bool is_permitted_name_(const std::string& s) const {
        for (char c : s) {
            if (!(c == '_' || ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || ('0' <= c && c <= '9'))) {
                return false;
            }
        }
        return true;
    }
};

}  // namespace field_transforms