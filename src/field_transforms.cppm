// SPDX-FileCopyrightText: 2026 Jan Babin
// SPDX-License-Identifier: AGPL-3.0-only
//
// Part of gtfs2rdf. See the LICENSE file for details.

module;

#include "util/diagnostics.h"

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

// defines the field_transforms module, which provides a registry for field transform functions that
// can be applied inside of placeholders (e.g. {stop_name | toUpper})
namespace field_transforms {

export const int MAX_ARGS =
    10; // expected maximum number of arguments for field transforms
        // 10 since this is required by calendar.txt generate_dates transform

// expected maximum number of chained transforms per placeholder - if more are needed, consider
// chaining transforms inside a single transform function for performance
export const int MAX_TRANSFORMS = 3;

export using ArgSpan = std::span<const std::string_view>;

export using Args = field_transforms::ArgSpan;
export using Out1 = std::string;
export using OutN = std::vector<std::string>;

export using Transform2One = std::function<void(Args, Out1&)>;
export using Transform2Many = std::function<void(Args, OutN&)>;

export enum class TransformKind { ONE, MANY };

export struct Transform {
	std::string name;
	TransformKind kind;
	Transform2One single; // valid if kind == ONE
	Transform2Many many;  // valid if kind == MANY
};

// manages the transforms. Ensures no duplicate names and only valid characters for names
export class TransformRegistry {
  public:
	void registerTransform(const std::string& name, Transform2One transform) {
		if (registry_.contains(name)) {
			throw diagnostics::Error("Transform error: field transform '" + name +
			                         "' already registered");
		}
		if (!isPermittedName_(name)) {
			throw diagnostics::Error("Transform error: invalid characters in transform name '" +
			                         name + "'");
		}
		registry_[name] = Transform{name, TransformKind::ONE, std::move(transform), {}};
	}

	void registerTransform(const std::string& name, Transform2Many transform) {
		if (registry_.contains(name)) {
			throw diagnostics::Error("Transform error: field transform '" + name +
			                         "' already registered");
		}
		if (!isPermittedName_(name)) {
			throw diagnostics::Error("Transform error: invalid characters in transform name '" +
			                         name + "'");
		}
		registry_[name] = Transform{name, TransformKind::MANY, {}, std::move(transform)};
	}

	// [TODO]: this will still be copied in code I think. Check if that can be avoided
	const Transform& getTransform(const std::string& name) const {
		if (!registry_.contains(name)) {
			throw diagnostics::Error("Transform error: unknown transform '" + name + "'");
		}
		return registry_.at(name);
	}

  private:
	std::unordered_map<std::string, Transform> registry_;

	// function that enforces only permitted characters in transform names
	// these include: a-z, A-Z, 0-9, _
	// NOLINTBEGIN : no need to simplify boolean expression since this is more readable
	static bool isPermittedName_(const std::string& str) {
		for (char c : str) {
			if (!(c == '_' || ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') ||
			      ('0' <= c && c <= '9'))) {
				return false;
			}
		}
		return true;
	}
	// NOLINTEND
};

} // namespace field_transforms