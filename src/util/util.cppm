// SPDX-FileCopyrightText: 2026 Jan Babin
// SPDX-License-Identifier: AGPL-3.0-only
//
// Part of gtfs2rdf. See the LICENSE file for details.

export module util;

export import :strings;
export import :misc;
export import :topological_sort;

export namespace util {
using namespace strings;
using namespace misc;
using namespace topological_sort;
} // namespace util

// toplevel module for utility functions and classes