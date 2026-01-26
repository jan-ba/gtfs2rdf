// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2025 Jan Babin
// Chair of Algorithms and Data Structures, University of Freiburg
//
// This file is part of the GTFS2RDF project.
// It is licensed under the GNU General Public License version 3.
// See the LICENSE file in the project root for the full license text.


module;

#include <string>
#include <unordered_map>
#include <stdexcept>
#include <vector>
#include <string_view>

export module rdf_components;
import runtime;

namespace rdf {

// this determines how to escape strings that fill the placeholders in a Instruction during
// writing RDF output
export enum class RenderKind {
    Raw,            // no escaping / fallback
    IriRef,         // placeholder is inside <...> (e.g. for ntriples)
    PrefixedLocal,  // placeholder is a prefixed name (e.g. gtfs:{LocalName})
    Literal,         // placeholder is inside "..." (e.g. for literals)
    LangTag         // placeholder is a language tag (e.g. @en)
};

// template string for one instruction / triple -> one RenderKind per placeholder from left to right
export struct TemplateString {
    std::string raw;
    std::vector<RenderKind> render_kinds;
};

// count number of placeholders ("{") in a string_view, expects correct syntax
static size_t _count_placeholders(std::string_view s) {
    size_t n = 0;
    for (size_t pos = 0; (pos = s.find("{", pos)) != std::string_view::npos; ++pos) ++n;
    return n;
}


// lookup table for unreserved characters in IRIREF per RFC3986
constexpr std::array<bool, 256> is_unreserved_table_iridef = []{
    std::array<bool, 256> table = {};
    for (unsigned char c = 'A'; c <= 'Z'; ++c) table[c] = true;
    for (unsigned char c = 'a'; c <= 'z'; ++c) table[c] = true;
    for (unsigned char c = '0'; c <= '9'; ++c) table[c] = true;
    table[static_cast<unsigned char>('-')] = true;
    table[static_cast<unsigned char>('.')] = true;
    table[static_cast<unsigned char>('_')] = true;
    table[static_cast<unsigned char>('~')] = true;
    return table;
}();

// percent-encode bytes not in RFC3986 "unreserved" (A-Z, a-z, 0-9, '-', '.', '_', '~'). 
// This keeps IRIREF safe, such as in <http://example.com/{value}>
export void percent_encode_iriref(std::string& out, std::string_view value) {
    static constexpr char H[] = "0123456789ABCDEF";
    for (unsigned char c : value) {
        if (is_unreserved_table_iridef[c] && c != '%') {
            out.push_back(static_cast<char>(c));
        } else {
            // percent-encode by hex representation 
            out.push_back('%');
            out.push_back(H[(c >> 4) & 0xF]);
            out.push_back(H[c & 0xF]);
        }
    }
}

// lookup table for safe characters in prefixed name local part
constexpr std::array<bool, 256> is_safe_table_prefixed_local = []{
    std::array<bool, 256> table = {};
    for (unsigned char c = 'A'; c <= 'Z'; ++c) table[c] = true;
    for (unsigned char c = 'a'; c <= 'z'; ++c) table[c] = true;
    for (unsigned char c = '0'; c <= '9'; ++c) table[c] = true;
    table[static_cast<unsigned char>('_')] = true;
    table[static_cast<unsigned char>('-')] = true;
    return table;
}();

// percent-encode bytes not in PN_LOCAL per Turtle spec (letters, digits, '_', '-'),
// e.g. for prefixed names such as gtfs:{Local Name} where space must be encoded
export void percent_encode_prefixed_local(std::string& out, std::string_view value) {
    static constexpr char H[] = "0123456789ABCDEF";
    for (unsigned char c : value) {
        if (is_safe_table_prefixed_local[c] && c != '%') {
          out.push_back(static_cast<char>(c));
        } else {
          out.push_back('%');
          out.push_back(H[(c >> 4) & 0xF]);
          out.push_back(H[c & 0xF]);
        }
    }
}

// percent-encode special characters in literals per RDF spec 
// (e.g. \n, \r, \t, \", \\, and control characters)
export void percent_encode_literal(std::string& out, std::string_view value) {
    bool needsEscape = false;
    for (unsigned char c : value) {
        if (c < 0x20 || c == 0x7F || c == '\\' || c == '"') { needsEscape = true; break; }
    }
    if (!needsEscape) {
        out.append(value);
        return;
    }

    static constexpr char H[] = "0123456789ABCDEF";

    for (unsigned char c : value) {
        switch (c) {
            case '\\': out.append("\\\\"); break;
            case '"' : out.append("\\\""); break;
            case '\n': out.append("\\n");  break;
            case '\r': out.append("\\r");  break;
            case '\t': out.append("\\t");  break;
            case '\b': out.append("\\b");  break;
            case '\f': out.append("\\f");  break;
            default:
                if (c < 0x20 || c == 0x7F) {
                    out.push_back('\\'); out.push_back('u');
                    out.push_back('0');  out.push_back('0');
                    out.push_back(H[(c >> 4) & 0xF]);
                    out.push_back(H[c & 0xF]);
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
}

export class IRI {
  private:
    const std::string prefix_;
    const std::string local_name_;

  public:
    IRI(const std::string& prefix, const std::string& local_name)
      : prefix_(prefix), local_name_(local_name) {
          // empty IRIs not allowed
          if (local_name_.empty()) {
              throw std::runtime_error("❌  Error: empty IRI is not allowed");
          }
      }

    IRI() : prefix_(""), local_name_("") {}
      
    // convert to template, which combines raw string with RenderKinds for its placeholders
    TemplateString toTemplate(const std::unordered_map<std::string, std::string>& prefixes,
                              const runtime::RuntimeContainer& rt) const
    {
        TemplateString t;

        const bool ntriples = rt.getSettings().isNTriplesOutput();

        // N-Triples: always use IRIREF <...>
        if (ntriples) {
            if (!prefix_.empty()) {
                t.raw = "<" + prefixes.at(prefix_) + local_name_ + ">";
                const size_t num_ph = _count_placeholders(local_name_);
                if (num_ph) t.render_kinds.assign(num_ph, RenderKind::IriRef);
                return t;
            }
            // prefix_ empty: assume already serialized token (<...> or _:...)
            t.raw = local_name_;
            const size_t num_ph = _count_placeholders(local_name_);
            if (num_ph) t.render_kinds.assign(num_ph, RenderKind::IriRef);
            return t;
        }

        // Turtle: keep prefixed names if possible
        if (!prefix_.empty()) {
            t.raw = prefix_ + ":" + local_name_;

            const size_t num_ph = _count_placeholders(local_name_);
            if (num_ph) t.render_kinds.assign(num_ph, RenderKind::PrefixedLocal);

            return t;
        }

        // prefix_ empty: treat as already-serialized token
        t.raw = local_name_;
        const size_t num_ph = _count_placeholders(local_name_);
        if (num_ph) t.render_kinds.assign(num_ph, RenderKind::IriRef);
        return t;
    }

    // convert to string directly (no escaping)
    const std::string toString(const std::unordered_map<std::string, std::string>& prefixes,
                              const runtime::RuntimeContainer& rt) const {
        return toTemplate(prefixes, rt).raw;
    }

};

export class Object {
  private:
    const enum class Type { IRI, Literal, BlankNode } type_;
    const IRI value_;
    const IRI datatype_;
    const std::string lang_;

  public:
    // IRI
    Object(const IRI& value) : type_(Type::IRI), value_(value) {}

    // literal - only language tag (if any)
    Object(const std::string& literal, const std::string& lang = "")
      : type_(Type::Literal), value_(IRI("", literal)), datatype_(IRI()), lang_(lang) {
    }

    // literal - with datatype
    Object(const std::string& literal, const IRI& datatype)
      : type_(Type::Literal), value_(IRI("", literal)), datatype_(datatype) {
    }

    // convert to template, which combines raw string with RenderKinds for its placeholders
    TemplateString toTemplate(const std::unordered_map<std::string, std::string>& prefixes,
                            const runtime::RuntimeContainer& rt) const
    {
        TemplateString t;

        switch (type_) {
            case Type::IRI: {
                return value_.toTemplate(prefixes, rt);
            }
            case Type::Literal: {
                const auto lit_t = value_.toTemplate(prefixes, rt);

                t.raw.reserve(lit_t.raw.size() + 16);
                t.raw.push_back('"');
                t.raw.append(lit_t.raw);
                t.raw.push_back('"');

                // mark placeholders in lexical form as Literal
                const size_t num_lit_ph = _count_placeholders(lit_t.raw);
                if (num_lit_ph) t.render_kinds.assign(num_lit_ph, RenderKind::Literal);

                if (!lang_.empty()) {
                    t.raw += "@";
                    t.raw += lang_;

                    // allow placeholders in language tag (e.g. @{FEED_LANG@feed_info.txt})
                    const size_t num_lang_ph = _count_placeholders(lang_);
                    if (num_lang_ph) t.render_kinds.insert(t.render_kinds.end(), num_lang_ph, RenderKind::LangTag);
                } else {
                    const auto dt_t = datatype_.toTemplate(prefixes, rt);
                    if (!dt_t.raw.empty()) {
                        t.raw += "^^" + dt_t.raw;
                        t.render_kinds.insert(t.render_kinds.end(),
                                              dt_t.render_kinds.begin(), dt_t.render_kinds.end());
                    }
                }
                return t;
            }
            case Type::BlankNode: {
                t.raw = "_:" + value_.toString(prefixes, rt);
                const size_t num_ph = _count_placeholders(t.raw);
                if (num_ph) t.render_kinds.assign(num_ph, RenderKind::Raw);
                return t;
            }
        }
        return t;
    }

    const std::string toString(const std::unordered_map<std::string, std::string>& prefixes,
                            const runtime::RuntimeContainer& rt) const {
        return toTemplate(prefixes, rt).raw;
    }
};

export class Triple {
  private:
    const IRI subject_;
    const IRI predicate_;
    const Object object_;

  public:
    Triple(const IRI& subject, const IRI& predicate, const Object& object)
      : subject_(subject), predicate_(predicate), object_(object) {}

    TemplateString toTemplate(const std::unordered_map<std::string, std::string>& prefixes,
                            const runtime::RuntimeContainer& rt) const
    {
        auto s = subject_.toTemplate(prefixes, rt);
        auto p = predicate_.toTemplate(prefixes, rt);
        auto o = object_.toTemplate(prefixes, rt);

        TemplateString t;
        t.raw = s.raw + " " + p.raw + " " + o.raw + " .";

        t.render_kinds.reserve(s.render_kinds.size() + p.render_kinds.size() + o.render_kinds.size());
        t.render_kinds.insert(t.render_kinds.end(), s.render_kinds.begin(), s.render_kinds.end());
        t.render_kinds.insert(t.render_kinds.end(), p.render_kinds.begin(), p.render_kinds.end());
        t.render_kinds.insert(t.render_kinds.end(), o.render_kinds.begin(), o.render_kinds.end());
        return t;
    }

    const std::string toString(const std::unordered_map<std::string, std::string>& prefixes,
                            const runtime::RuntimeContainer& rt) const {
        return toTemplate(prefixes, rt).raw;
    }
};

} // namespace