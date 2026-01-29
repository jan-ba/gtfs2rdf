#pragma once

#ifndef GTFS2RDF_STORAGE_TIMING
#ifdef NDEBUG
#define GTFS2RDF_STORAGE_TIMING 0
#else
#define GTFS2RDF_STORAGE_TIMING 1
#endif
#endif

#if GTFS2RDF_STORAGE_TIMING
#define SCOPED_TIMER_NS(acc) ::util::misc::ScopedTimerNS _t_##__LINE__(acc)
#else
#define SCOPED_TIMER_NS(acc) ((void)0)
#endif
