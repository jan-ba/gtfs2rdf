// SPDX-FileCopyrightText: 2026 Jan Babin
// SPDX-License-Identifier: AGPL-3.0-only
//
// Part of gtfs2rdf. See the LICENSE file for details.

#pragma once

#ifndef GTFS2RDF_FULL_STATS
#ifdef NDEBUG
#define GTFS2RDF_FULL_STATS 0
#else
#define GTFS2RDF_FULL_STATS 1
#endif
#endif

#if GTFS2RDF_FULL_STATS
#define SCOPED_TIMER_NS(acc) ::util::misc::ScopedTimerNS _t_##__LINE__(acc)
#define SCOPED_TIMER_PAUSE_NS(acc) ::util::misc::ScopedTimerPauseNS _tp_##__LINE__(acc)
#else
#define SCOPED_TIMER_NS(acc) ((void)0)
#define SCOPED_TIMER_PAUSE_NS(acc) ((void)0)
#endif
