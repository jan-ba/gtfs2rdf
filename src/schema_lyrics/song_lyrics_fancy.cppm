// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg

module;

#include "transform_macros.h"

#include <string>
#include <unordered_map>
#include <vector>

export module schema:song_lyrics_fancy;

import :core;
import rdf_components;
import field_transforms;
import t_lib;
import runtime;
import util;

using namespace rdf;

namespace schema {

export Schema buildSongLyricsFancySchema(runtime::RuntimeContainer& rtc) {
	// -------------------------------------------------------------------------
	// Small helpers
	// -------------------------------------------------------------------------

	auto isValidLanguage = [](std::string_view lang) {
		if (lang == "NaN" || lang == "nan" || lang == "null" || lang.empty()) {
			return false;
		}
		// 2- or 3-letter codes (en, de, eng, ...)
		return lang.size() == 2 || lang.size() == 3;
	};

	auto normaliseArtistLabel = [](std::string_view s) -> std::string {
		// dataset artifact: sometimes backslashes appear (e.g. Cam\\'ron)
		// we normalise by removing backslashes, matching parseFeatures.
		return util::strings::replaceAll(std::string(s), "\\", "");
	};

	// -------------------------------------------------------------------------
	// Language handling
	// -------------------------------------------------------------------------

	TRANSFORM2ONE(existsNoLanguage, ARGS, OUT_VAL, STORAGE) {
		// ARGS[0]: lyrics, ARGS[1]: language
		if (ARGS.size() != 2) {
			TRANSFORM_ERROR("Expected lyrics and language, but got: " +
			                std::to_string(ARGS.size()) + " arguments");
		}
		if (isValidLanguage(ARGS[1])) {
			return; // language exists, previous triple did the job, so skip this one
		}
		OUT_VAL = ARGS[0]; // return lyrics as-is, language does not exist
	}
	TRANSFORM_END

	TRANSFORM2ONE(validateLanguage, ARGS, OUT_VAL, STORAGE) {
		if (ARGS.size() != 1) {
			TRANSFORM_ERROR("Expected exactly one argument for language, but got: " +
			                std::to_string(ARGS.size()));
		}
		auto lang = ARGS[0];
		if (!isValidLanguage(lang)) {
			return; // treat these as missing language
		}
		OUT_VAL = lang;
	}
	TRANSFORM_END

	// -------------------------------------------------------------------------
	// Feature parsing
	// -------------------------------------------------------------------------

	TRANSFORM2MANY(parseFeatures, ARGS, OUT_VALS, STORAGE) {
		// ARGS[0]: feature list in format '{"feat1", "feat2", ...}'
		// ARGS[1]: main artist to be removed from feature list (if present)
		if (ARGS.size() != 2) {
			TRANSFORM_ERROR("Expected exactly two arguments for parseFeatures (feature list and "
			                "main artist), but got: " +
			                std::to_string(ARGS.size()));
		}

		auto svw = ARGS[0];
		if (svw.size() < 2 || svw.front() != '{' || svw.back() != '}') {
			return; // empty or not in expected format
		}

		svw = svw.substr(1, svw.size() - 2); // remove surrounding braces
		if (svw.empty()) {
			return;
		}

		auto features = util::strings::splitAtTopLevel(svw, ',');
		if (features.empty()) {
			return;
		}

		std::string main_artist = normaliseArtistLabel(ARGS[1]);

		for (auto& feat : features) {
			auto feat_clean = normaliseArtistLabel(util::strings::unquote(feat));
			if (feat_clean.empty()) {
				continue;
			}
			if (feat_clean == main_artist) {
				continue; // skip main artist if present
			}
			OUT_VALS.push_back(std::move(feat_clean));
		}
	}
	TRANSFORM_END

	// -------------------------------------------------------------------------
	// Artist ID assignment + "print once" logic (MULTI_MAP, not VARIABLES)
	//
	// Storage layout (ctx = "song_lyrics.txt"):
	//   - VARIABLE: NEXT_ARTIST_ID
	//   - MULTI_MAP: artist_ids[label]    -> id (single value)
	//   - MULTI_MAP: artist_label_printed[id] -> "1"
	// -------------------------------------------------------------------------

	TRANSFORM2ONE(ensureArtistId, ARGS, OUT_VAL, STORAGE) {
		// ARGS[0]: artist label
		if (ARGS.size() != 1) {
			TRANSFORM_ERROR("Expected exactly one argument for ensureArtistId, but got: " +
			                std::to_string(ARGS.size()));
		}

		const std::string ctx = "song_lyrics.txt";

		std::string label = normaliseArtistLabel(ARGS[0]);
		if (label.empty()) {
			return;
		}

		// already have an id?
		auto ids = STORAGE.getValues(ctx, "artist_ids", {label});
		if (!ids.empty()) {
			OUT_VAL = ids[0];
			return;
		}

		// allocate new
		std::string next_s = STORAGE.getVariable(ctx, "NEXT_ARTIST_ID");
		unsigned long long next = 1;
		if (!next_s.empty()) {
			try {
				next = std::stoull(next_s);
			} catch (...) {
				next = 1;
			}
		}

		std::string id = std::to_string(next);

		STORAGE.storeValue(ctx, "artist_ids", {label}, id);
		STORAGE.storeVariable(ctx, "NEXT_ARTIST_ID", std::to_string(next + 1));

		OUT_VAL = id;
	}
	TRANSFORM_END

	TRANSFORM2ONE(getArtistId, ARGS, OUT_VAL, STORAGE) {
		// ARGS[0]: artist label
		if (ARGS.size() != 1) {
			TRANSFORM_ERROR("Expected exactly one argument for getArtistId, but got: " +
			                std::to_string(ARGS.size()));
		}

		const std::string ctx = "song_lyrics.txt";

		std::string label = normaliseArtistLabel(ARGS[0]);
		if (label.empty()) {
			return;
		}

		auto ids = STORAGE.getValues(ctx, "artist_ids", {label});
		if (ids.empty()) {
			TRANSFORM_ERROR("Artist label '" + label +
			                "' not found in storage. "
			                "Ensure ensureArtistId runs before getArtistId.");
		}

		OUT_VAL = ids[0];
	}
	TRANSFORM_END

	TRANSFORM2ONE(labelOnceById, ARGS, OUT_VAL, STORAGE) {
		// ARGS[0]: artist label (we use it to find the id; output is the label once)
		if (ARGS.size() != 1) {
			TRANSFORM_ERROR("Expected exactly one argument for labelOnceById, but got: " +
			                std::to_string(ARGS.size()));
		}

		const std::string ctx = "song_lyrics.txt";

		std::string label = normaliseArtistLabel(ARGS[0]);
		if (label.empty()) {
			return;
		}

		auto ids = STORAGE.getValues(ctx, "artist_ids", {label});
		if (ids.empty()) {
			// if it wasn't ensured yet, do nothing (storage-only should handle it)
			return;
		}

		const std::string& id = ids[0];

		if (STORAGE.containsValue(ctx, "artist_label_printed", {id}, "1")) {
			return; // already printed
		}

		STORAGE.storeValue(ctx, "artist_label_printed", {id}, "1");
		OUT_VAL = label; // print label exactly once per artist-id
	}
	TRANSFORM_END

	TRANSFORM2ONE(typeOnceById, ARGS, OUT_VAL, STORAGE) {
		if (ARGS.size() != 1) {
			TRANSFORM_ERROR("Expected exactly one argument for typeOnceById, but got: " +
			                std::to_string(ARGS.size()));
		}

		const std::string ctx = "song_lyrics.txt";

		std::string label = normaliseArtistLabel(ARGS[0]);
		if (label.empty())
			return;

		auto ids = STORAGE.getValues(ctx, "artist_ids", {label});
		if (ids.empty())
			return; // should not happen if ensured in storage-only

		const std::string& id = ids[0];

		if (STORAGE.containsValue(ctx, "artist_type_printed", {id}, "1")) {
			return; // already printed type
		}
		STORAGE.storeValue(ctx, "artist_type_printed", {id}, "1");
		OUT_VAL = id; // return the ID once
	}
	TRANSFORM_END

	// -------------------------------------------------------------------------
	// Columns
	// -------------------------------------------------------------------------

	const std::vector<std::string> POSSIBLE_COLUMNS = {
	    "title",
	    "tag",
	    "artist",
	    "year",
	    "views",
	    "features",
	    "lyrics",
	    "id",
	    "language_cld3",
	    "language_ft",
	    "language",
	};

	// -------------------------------------------------------------------------
	// Prefixes
	// -------------------------------------------------------------------------

	const std::unordered_map<std::string, std::string> PREFIXES = {
	    {"song", "https://example.org/lyrics/song/"},
	    {"lyr", "https://example.org/lyrics/vocab#"},
	    {"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"},
	    {"rdfs", "http://www.w3.org/2000/01/rdf-schema#"},
	    {"xsd", "http://www.w3.org/2001/XMLSchema#"},
	    {"artistId", "https://example.org/lyrics/artist/id/"},
	    {"genre", "https://example.org/lyrics/genre/"},
	};

	// -------------------------------------------------------------------------
	// Nodes
	// -------------------------------------------------------------------------

	const IRI SONG = IRI("song", "{id}");

	// Artist nodes are ID-based (short + stable, good for indexing/autocomplete).
	const IRI MAIN_ARTIST = IRI("artistId", "{artist | getArtistId@song_lyrics.txt}");
	const IRI FEAT_ARTIST =
	    IRI("artistId", "{features, artist | parseFeatures | getArtistId@song_lyrics.txt}");
	const IRI MAIN_ARTIST_TYPE_NODE = IRI("artistId", "{artist | typeOnceById@song_lyrics.txt}");

	// -------------------------------------------------------------------------
	// Triples
	// -------------------------------------------------------------------------

	const std::vector<Triple> TRIPLES = {
	    // Song node
	    {SONG, {"rdf", "type"}, {IRI("lyr", "Song")}},

	    // Links
	    {SONG, {"lyr", "primaryArtist"}, {MAIN_ARTIST}},
	    {SONG, {"lyr", "contributorArtist"}, {FEAT_ARTIST}},
	    {SONG, {"lyr", "genre"}, {IRI("genre", "{tag}")}},

	    // Song attributes
	    {SONG, {"lyr", "title"}, {"{title}"}},
	    {SONG,
	     {"lyr", "year"},
	     {"{year | isValidInt | isUnsigned}", IRI("xsd", "nonNegativeInteger")}},
	    {SONG,
	     {"lyr", "views"},
	     {"{views | isValidInt | isUnsigned}", IRI("xsd", "nonNegativeInteger")}},

	    // Lyrics with optional language tag
	    {SONG, {"lyr", "lyrics"}, {"{lyrics}", "{language | validateLanguage}"}},
	    {SONG, {"lyr", "lyrics"}, {"{lyrics, language | existsNoLanguage}"}},

	    // Artist typing + labels (printed once per artist-id)
	    {MAIN_ARTIST_TYPE_NODE, {"rdf", "type"}, {IRI("lyr", "Artist")}},
	    {MAIN_ARTIST, {"rdfs", "label"}, {"{artist | labelOnceById@song_lyrics.txt}"}},
	};

	// -------------------------------------------------------------------------
	// Storage-only: ensure IDs exist before triples that use getArtistId()
	// -------------------------------------------------------------------------

	const std::vector<std::string> NO_WRITE_INSTRUCTIONS = {
	    // Ensure main artist has an ID
	    "{artist | ensureArtistId@song_lyrics.txt}",

	    // Ensure all feature artists have IDs
	    "{features, artist | parseFeatures | ensureArtistId@song_lyrics.txt}",
	};

	return Schema(
	    "song_lyrics.txt", POSSIBLE_COLUMNS, PREFIXES, TRIPLES, NO_WRITE_INSTRUCTIONS, rtc);
}

} // namespace schema
