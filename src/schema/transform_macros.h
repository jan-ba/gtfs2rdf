// SPDX-FileCopyrightText: 2026 Jan Babin
// SPDX-License-Identifier: AGPL-3.0-only
//
// Part of gtfs2rdf. See the LICENSE file for details.

#pragma once

#include "../util/diagnostics.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

// For user convenience when registering their own transforms inside build<Schema>(rtc):

// information for developers: the STORAGE variable is created automatically inside the lambda
// by getting it from the runtime container passed to build<Schema> such that the user does not
// have to care about it.
// For information regarding the type definitions of Transform2One and Transform2Many, see
// field_transforms.cppm

// to be used inside transform bodies (after TRANSFORM2ONE/2MANY opened the lambda)
#define TRANSFORM_ERROR(MSG) throw diagnostics::Error("Custom user error: " + std::string(MSG));

#define TRANSFORM2ONE(NAME, ARGS, OUT_VAL, STORAGE)                                                \
	rtc.getTransformRegistry().registerTransform(                                               \
        #NAME,                                                                                  \
        field_transforms::Transform2One{                                                        \
            [&](field_transforms::Args(ARGS), field_transforms::Out1& (OUT_VAL)) -> void {      \
              [[maybe_unused]] constexpr std::string_view __gtfs2rdf_transform_name = #NAME;    \
              [[maybe_unused]] auto& (STORAGE) = rtc.getStorage();                              \
              try {
#define TRANSFORM2MANY(NAME, ARGS, OUT_VALS, STORAGE)                                              \
	rtc.getTransformRegistry().registerTransform(                                               \
        #NAME,                                                                                  \
        field_transforms::Transform2Many{                                                       \
            [&](field_transforms::Args(ARGS), field_transforms::OutN& (OUT_VALS)) -> void {     \
              [[maybe_unused]] constexpr std::string_view __gtfs2rdf_transform_name = #NAME;    \
              [[maybe_unused]] auto& (STORAGE) = rtc.getStorage();                              \
              try {
#define TRANSFORM_END                                                                              \
	}                                                                                              \
	catch (...) {                                                                                  \
		diagnostics::wrapAndRethrow("while executing transform '" +                                \
		                            std::string(__gtfs2rdf_transform_name) + "'");                 \
	}                                                                                              \
	}                                                                                              \
	}                                                                                              \
	);
