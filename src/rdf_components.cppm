// SPDX-FileCopyrightText: 2026 Jan Babin
// SPDX-License-Identifier: AGPL-3.0-only
//
// Part of gtfs2rdf. See the LICENSE file for details.

module;

#include "./util/diagnostics.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

export module rdf_components;
import runtime;

namespace rdf {

// this determines how to escape strings that fill the placeholders in a Instruction during
// writing Rdf output
export enum class RenderKind : uint8_t {
	RAW,            // no escaping / fallback
	IRI_REF,        // placeholder is inside <...> (e.g. for ntriples)
	PREFIXED_LOCAL, // placeholder is a prefixed name (e.g. gtfs:{LocalName})
	LITERAL,        // placeholder is inside "..." (e.g. for literals)
	LANG_TAG        // placeholder is a language tag (e.g. @en)
};

// template string for one instruction / triple -> one RenderKind per placeholder from left to right
export struct TemplateString {
	std::string raw;
	std::vector<RenderKind> render_kinds;
};

// count number of placeholders ("{") in a string_view, expects correct syntax
static size_t countPlaceholders(std::string_view svw) {
	size_t count = 0;
	for (size_t pos = 0; (pos = svw.find('{', pos)) != std::string_view::npos; ++pos) {
		++count;
	}
	return count;
}

// lookup table for unreserved characters in IRIREF per RFC3986
// NOLINTBEGIN: very specific usage, packaged in lookup table -> comprehensible as is
constexpr std::array<bool, 256> LT_UNRESERVED_IRIDEF = [] {
	std::array<bool, 256> table = {};
	for (unsigned char c = 'A'; c <= 'Z'; ++c) {
		table[c] = true;
	}
	for (unsigned char c = 'a'; c <= 'z'; ++c) {
		table[c] = true;
	}
	for (unsigned char c = '0'; c <= '9'; ++c) {
		table[c] = true;
	}
	table[static_cast<unsigned char>('-')] = true;
	table[static_cast<unsigned char>('.')] = true;
	table[static_cast<unsigned char>('_')] = true;
	table[static_cast<unsigned char>('~')] = true;
	return table;
}();
// NOLINTEND

// percent-encode bytes not in RFC3986 "unreserved" (A-Z, a-z, 0-9, '-', '.', '_', '~').
// This keeps IRIREF safe, such as in <http://example.com/{value}>
// NOLINTBEGIN: very specific usage, packaged in single function -> comprehensible as is
export void percentEncodeIRIREF(std::string& out, std::string_view value) {
	static constexpr char H[] = "0123456789ABCDEF";
	for (unsigned char c : value) {
		if (LT_UNRESERVED_IRIDEF[c] && c != '%') {
			out.push_back(static_cast<char>(c));
		} else {
			// percent-encode by hex representation
			out.push_back('%');
			out.push_back(H[(c >> 4) & 0xF]);
			out.push_back(H[c & 0xF]);
		}
	}
}
// NOLINTEND

// lookup table for safe characters in prefixed name local part
// NOLINTBEGIN: very specific usage, packaged in lookup table -> comprehensible as is
constexpr std::array<bool, 256> LT_SAFE_PREFIXED_LOCAL = [] {
	std::array<bool, 256> table = {};
	for (unsigned char c = 'A'; c <= 'Z'; ++c) {
		table[c] = true;
	}
	for (unsigned char c = 'a'; c <= 'z'; ++c) {
		table[c] = true;
	}
	for (unsigned char c = '0'; c <= '9'; ++c) {
		table[c] = true;
	}
	table[static_cast<unsigned char>('_')] = true;
	table[static_cast<unsigned char>('-')] = true;
	return table;
}();
// NOLINTEND

// percent-encode bytes not in (letters, digits, '_', '-'),
// rdf1.2 allows more characters (e.g. non trailing periods / non leading colons), but we're being
// more rigorous here for simplicity
// e.g. for prefixed names such as gtfs:{Local Name} where space must be encoded
// NOLINTBEGIN: very specific usage, packaged in single function -> comprehensible as is
export void percentEncodePrefixedLocal(std::string& out, std::string_view value) {
	static constexpr char H[] = "0123456789ABCDEF";
	for (unsigned char c : value) {
		if (LT_SAFE_PREFIXED_LOCAL[c] && c != '%') {
			out.push_back(static_cast<char>(c));
		} else {
			out.push_back('%');
			out.push_back(H[(c >> 4) & 0xF]);
			out.push_back(H[c & 0xF]);
		}
	}
}
// NOLINTEND

// percent-encode special characters in literals per Rdf spec
// (e.g. \n, \r, \t, \", \\, and control characters)
// NOLINTBEGIN: very specific usage, packaged in single function -> comprehensible as is
export void percentEncodeLiteral(std::string& out, std::string_view value) {
	bool needsEscape = false;
	for (unsigned char c : value) {
		if (c < 0x20 || c == 0x7F || c == '\\' || c == '"') {
			needsEscape = true;
			break;
		}
	}
	if (!needsEscape) {
		out.append(value);
		return;
	}

	static constexpr char H[] = "0123456789ABCDEF";

	for (unsigned char c : value) {
		switch (c) {
			case '\\':
				out.append("\\\\");
				break;
			case '"':
				out.append("\\\"");
				break;
			case '\n':
				out.append("\\n");
				break;
			case '\r':
				out.append("\\r");
				break;
			case '\t':
				out.append("\\t");
				break;
			case '\b':
				out.append("\\b");
				break;
			case '\f':
				out.append("\\f");
				break;
			default:
				if (c < 0x20 || c == 0x7F) {
					out.push_back('\\');
					out.push_back('u');
					out.push_back('0');
					out.push_back('0');
					out.push_back(H[(c >> 4) & 0xF]);
					out.push_back(H[c & 0xF]);
				} else {
					out.push_back(static_cast<char>(c));
				}
		}
	}
}
// NOLINTEND

export class IRI {
  private:
	const std::string PREFIX_;
	const std::string LOCAL_NAME_;

  public:
	IRI(std::string prefix, std::string local_name)
	    : PREFIX_(std::move(prefix))
	    , LOCAL_NAME_(std::move(local_name)) {
		// empty IRIs not allowed
		if (LOCAL_NAME_.empty()) {
			throw diagnostics::Error("Schema error: IRI must always have a non-empty local name");
		}
		if (PREFIX_ == "_") {
			throw diagnostics::Error(
			    "Schema error: this converter doesn't support blank nodes yet");
		}
	}

	IRI() = default; // empty IRI

	// convert to template, which combines raw string with RenderKinds for its placeholders
	[[nodiscard]] TemplateString
	toTemplate(const std::unordered_map<std::string, std::string>& prefixes,
	           const runtime::RuntimeContainer& rtc) const {
		TemplateString tmpl;

		const bool NTRIPLES = rtc.getSettings().isNTriplesOutput();

		// N-Triples: always use IRIREF <...>
		if (NTRIPLES) {
			if (!PREFIX_.empty()) {
				if (!prefixes.contains(PREFIX_)) {
					throw diagnostics::Error("Schema error: unknown prefix '" + PREFIX_ +
					                         "' in IRI");
				}
				tmpl.raw = "<" + prefixes.at(PREFIX_) + LOCAL_NAME_ + ">";
				const size_t NUM_PHLS = countPlaceholders(LOCAL_NAME_);
				if (NUM_PHLS) {
					tmpl.render_kinds.assign(NUM_PHLS, RenderKind::IRI_REF);
				}
				return tmpl;
			}
			// PREFIX_ empty: assume already serialized token (<...> or _:...)
			tmpl.raw = LOCAL_NAME_;
			const size_t NUM_PHLS = countPlaceholders(LOCAL_NAME_);
			if (NUM_PHLS) {
				tmpl.render_kinds.assign(NUM_PHLS, RenderKind::IRI_REF);
			}
			return tmpl;
		}

		// Turtle: keep prefixed names if possible
		if (!PREFIX_.empty()) {
			tmpl.raw = PREFIX_ + ":" + LOCAL_NAME_;

			const size_t NUM_PHLS = countPlaceholders(LOCAL_NAME_);
			if (NUM_PHLS) {
				tmpl.render_kinds.assign(NUM_PHLS, RenderKind::PREFIXED_LOCAL);
			}

			return tmpl;
		}

		// PREFIX_ empty: treat as already-serialized token
		tmpl.raw = LOCAL_NAME_;
		const size_t NUM_PHLS = countPlaceholders(LOCAL_NAME_);
		if (NUM_PHLS) {
			tmpl.render_kinds.assign(NUM_PHLS, RenderKind::IRI_REF);
		}
		return tmpl;
	}

	// convert to string directly (no escaping)
	[[nodiscard]] std::string toString(const std::unordered_map<std::string, std::string>& prefixes,
	                                   const runtime::RuntimeContainer& rtc) const {
		return toTemplate(prefixes, rtc).raw;
	}
};

export class Object {
  private:
	const enum class Type : uint8_t { IRI, LITERAL /*, BLANK_NODE */ } TYPE_;
	const IRI VALUE_;
	const IRI DATATYPE_;
	const std::string LANG_;

  public:
	// IRI
	Object(IRI value)
	    : TYPE_(Type::IRI)
	    , VALUE_(std::move(value)) {
	}

	// literal - only language tag (if any)
	Object(std::string literal, std::string lang = "")
	    : TYPE_(Type::LITERAL)
	    , VALUE_(IRI("", std::move(literal)))
	    , DATATYPE_(IRI())
	    , LANG_(std::move(lang)) {
	}

	// literal - with datatype
	Object(std::string literal, IRI datatype)
	    : TYPE_(Type::LITERAL)
	    , VALUE_(IRI("", std::move(literal)))
	    , DATATYPE_(std::move(datatype)) {
	}

	// convert to template, which combines raw string with RenderKinds for its placeholders
	[[nodiscard]] TemplateString
	toTemplate(const std::unordered_map<std::string, std::string>& prefixes,
	           const runtime::RuntimeContainer& rtc) const {
		TemplateString tmpl;

		switch (TYPE_) {
			case Type::IRI:
				{
					return VALUE_.toTemplate(prefixes, rtc);
				}
			case Type::LITERAL:
				{
					const auto LIT_TMPL = VALUE_.toTemplate(prefixes, rtc);

					tmpl.raw.reserve(LIT_TMPL.raw.size());
					tmpl.raw.push_back('"');
					tmpl.raw.append(LIT_TMPL.raw);
					tmpl.raw.push_back('"');

					// mark placeholders in lexical form as Literal
					const size_t NUM_LIT_PHLS = countPlaceholders(LIT_TMPL.raw);
					if (NUM_LIT_PHLS) {
						tmpl.render_kinds.assign(NUM_LIT_PHLS, RenderKind::LITERAL);
					}

					if (!LANG_.empty()) {
						tmpl.raw += "@";
						tmpl.raw += LANG_;

						// allow placeholders in language tag (e.g. @{FEED_LANG@feed_info.txt})
						const size_t NUM_LANG_PHLS = countPlaceholders(LANG_);
						if (NUM_LANG_PHLS) {
							tmpl.render_kinds.insert(
							    tmpl.render_kinds.end(), NUM_LANG_PHLS, RenderKind::LANG_TAG);
						}
					} else {
						const auto DT_TEMPL = DATATYPE_.toTemplate(prefixes, rtc);
						if (!DT_TEMPL.raw.empty()) {
							tmpl.raw += "^^" + DT_TEMPL.raw;
							tmpl.render_kinds.insert(tmpl.render_kinds.end(),
							                         DT_TEMPL.render_kinds.begin(),
							                         DT_TEMPL.render_kinds.end());
						}
					}
					return tmpl;
				}
				// case Type::BLANK_NODE: {
				// 	tmpl.raw = "_:" + VALUE_.toString(prefixes, rtc);
				// 	const size_t NUM_PHLS = countPlaceholders(tmpl.raw);
				// 	if (NUM_PHLS) {
				// 		tmpl.render_kinds.assign(NUM_PHLS, RenderKind::RAW);
				// 	}
				// 	return tmpl;
				// }
		}
		return tmpl;
	}

	[[nodiscard]] std::string toString(const std::unordered_map<std::string, std::string>& prefixes,
	                                   const runtime::RuntimeContainer& rtc) const {
		return toTemplate(prefixes, rtc).raw;
	}
};

export class Triple {
  private:
	const IRI SUBJECT_;
	const IRI PREDICATE_;
	const Object OBJECT_;

  public:
	Triple(IRI subject, IRI predicate, Object object)
	    : SUBJECT_(std::move(subject))
	    , PREDICATE_(std::move(predicate))
	    , OBJECT_(std::move(object)) {
	}

	[[nodiscard]] TemplateString
	toTemplate(const std::unordered_map<std::string, std::string>& prefixes,
	           const runtime::RuntimeContainer& rtc) const {
		auto subj = SUBJECT_.toTemplate(prefixes, rtc);
		auto pred = PREDICATE_.toTemplate(prefixes, rtc);
		auto obj = OBJECT_.toTemplate(prefixes, rtc);

		TemplateString tmpl;
		tmpl.raw = subj.raw + " " + pred.raw + " " + obj.raw + " .";

		tmpl.render_kinds.reserve(subj.render_kinds.size() + pred.render_kinds.size() +
		                          obj.render_kinds.size());
		tmpl.render_kinds.insert(
		    tmpl.render_kinds.end(), subj.render_kinds.begin(), subj.render_kinds.end());
		tmpl.render_kinds.insert(
		    tmpl.render_kinds.end(), pred.render_kinds.begin(), pred.render_kinds.end());
		tmpl.render_kinds.insert(
		    tmpl.render_kinds.end(), obj.render_kinds.begin(), obj.render_kinds.end());
		return tmpl;
	}

	[[nodiscard]] std::string toString(const std::unordered_map<std::string, std::string>& prefixes,
	                                   const runtime::RuntimeContainer& rtc) const {
		return toTemplate(prefixes, rtc).raw;
	}
};

} // namespace rdf