module;

#include <algorithm>
#include <functional>
#include <iostream>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

export module field_transforms;
import util;
using namespace util;

namespace field_transforms {

export const int MaxArgs = 10; // expected maximum number of arguments for field transforms
                               // 10 since this is required by calendar.txt generate_dates transform

// expected maximum number of chained transforms per placeholder - if more are needed, consider
// chaining transforms inside a single transform function for performance
export const int MaxTransforms = 3;

export using ArgSpan = std::span<const std::string_view>;

export using Args = field_transforms::ArgSpan;
export using Out1 = std::string;
export using OutN = std::vector<std::string>;

export using Transform2One = std::function<void(Args, Out1 &)>;
export using Transform2Many = std::function<void(Args, OutN &)>;

export enum class TransformKind { Single, Multi };

export struct Transform {
	TransformKind kind;
	Transform2One single; // valid if kind == Single
	Transform2Many multi; // valid if kind == Multi
};

export class TransformRegistry {
  public:
	void registerTransform(const std::string &name, Transform2One fn) {
		if (registry_.contains(name)) {
			throw std::runtime_error("❌  Transform error: field transform already registered: " +
			                         name);
		}
		if (!is_permitted_name_(name)) {
			throw std::runtime_error("❌  Transform error: invalid characters in transform name: " +
			                         name);
		}
		registry_[name] = Transform{TransformKind::Single, fn, {}};
	}

	void registerTransform(const std::string &name, Transform2Many fn) {
		if (registry_.contains(name)) {
			throw std::runtime_error("❌  Transform error: field transform already registered: " +
			                         name);
		}
		if (!is_permitted_name_(name)) {
			throw std::runtime_error("❌  Transform error: invalid characters in transform name: " +
			                         name);
		}
		registry_[name] = Transform{TransformKind::Multi, {}, fn};
	}

	const Transform &getTransform(const std::string &name) const {
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
	bool is_permitted_name_(const std::string &s) const {
		for (char c : s) {
			if (!(c == '_' || ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') ||
			      ('0' <= c && c <= '9'))) {
				return false;
			}
		}
		return true;
	}
};

} // namespace field_transforms