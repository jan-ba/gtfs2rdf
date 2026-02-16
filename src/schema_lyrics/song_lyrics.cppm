// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg

module;

#include "transform_macros.h"

#include <string>
#include <unordered_map>
#include <vector>

export module schema:song_lyrics;

import :core;
import rdf_components;
import field_transforms;
import t_lib;
import runtime;
import util;

using namespace rdf;

namespace schema {

export Schema buildSongLyricsSchema(runtime::RuntimeContainer& rtc) {
	// Columns expected in the CSV
	const std::vector<std::string> POSSIBLE_COLUMNS = {
	    "id",
	    "artist",
	    "title",
	    "tag",
	    "year",
	    "views",
	    "lyrics",
	    "language",
	    "features",
	};

	// Prefixes exactly like your SPARQL snippet (plus nothing else)
	const std::unordered_map<std::string, std::string> PREFIXES = {
	    {"lyr", "https://example.org/lyrics/vocab#"},
	    {"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
	};

	// BNODE() per row (deterministic label based on id; still a blank node token)
	// If your engine rejects blank node tokens, use the Skolem fallback below.
	const IRI SONG = IRI("", "_:song_{id}");

	const std::vector<Triple> TRIPLES = {
	    {SONG, {"rdf", "type"}, {IRI("lyr", "Song")}},

	    // {SONG, {"lyr", "id"}, {"{id}"}},
	    {SONG, {"lyr", "primaryArtist"}, {"{artist}"}},
	    {SONG, {"lyr", "title"}, {"{title}"}},
	    {SONG, {"lyr", "genre"}, {"{tag}"}},
	    {SONG, {"lyr", "year"}, {"{year}"}},
	    {SONG, {"lyr", "views"}, {"{views}"}},
	    {SONG, {"lyr", "lyrics"}, {"{lyrics}"}},
	    {SONG, {"lyr", "language"}, {"{language}"}},
	    {SONG, {"lyr", "features"}, {"{features}"}},
	};

	const std::vector<std::string> NO_WRITE_INSTRUCTIONS = {};

	return Schema(
	    "song_lyrics.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, NO_WRITE_INSTRUCTIONS, rtc);
}

} // namespace schema
