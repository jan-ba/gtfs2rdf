// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the GTFS2RDF project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.

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