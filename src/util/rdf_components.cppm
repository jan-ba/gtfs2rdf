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

export module rdf_components;
import runtime;

export namespace rdf {

class IRI {
  private:
    const std::string prefix_;
    const std::string local_name_;

  public:
    IRI(const std::string& prefix, const std::string& local_name)
      : prefix_(prefix), local_name_(local_name) {}

    IRI() : prefix_(""), local_name_("") {}
      
  const std::string toString(const std::unordered_map<std::string, std::string>& prefixes,
                             const runtime::RuntimeContainer& rt) const {
    if (!rt.getSettings().isNTriplesOutput() && !prefix_.empty()) {
      return prefix_ + ":" + local_name_;
    } else if (!prefix_.empty()) {
      return "<" + prefixes.at(prefix_) + local_name_ + ">";
      // might add other functionality later
    } else {
      return local_name_;
    }
  }
};

class Object {
  private:
    const enum class Type { IRI, Literal, BlankNode } type_;
    const IRI name_;
    const IRI datatype_;
    const std::string lang_;

  public:
    // IRI
    Object(const IRI& name) : type_(Type::IRI), name_(name) {}

    // literal - only language tag (if any)
    Object(const std::string& literal, const std::string& lang = "")
      : type_(Type::Literal), name_(IRI("", literal)), datatype_(IRI("", "")), lang_(lang) {}

    // literal - with datatype
    Object(const std::string& literal, const IRI& datatype)
      : type_(Type::Literal), name_(IRI("", literal)), datatype_(datatype) {}

    const std::string toString(const std::unordered_map<std::string, std::string>& prefixes,
                              const runtime::RuntimeContainer& rt) const {
      switch (type_) {
        case Type::IRI:
          return name_.toString(prefixes, rt);
        case Type::Literal: {
          std::string lit = "\"" 
                + name_.toString(prefixes, rt) + "\"";
          if (!lang_.empty()) {
            lit += "@" + lang_;
          } else if (!datatype_.toString(prefixes, rt).empty()) {
            lit += "^^" + datatype_.toString(prefixes, rt);
          }
          return lit;
        }
        case Type::BlankNode:  // required?
          return "_:" + name_.toString(prefixes, rt);
      }
      return ""; // should not reach here
    }
};

class Triple {
  private:
    const IRI subject_;
    const IRI predicate_;
    const Object object_;

  public:
    Triple(const IRI& subject, const IRI& predicate, const Object& object)
      : subject_(subject), predicate_(predicate), object_(object) {}

    const std::string toString(const std::unordered_map<std::string, std::string>& prefixes,
                             const runtime::RuntimeContainer& rt) const {
      return subject_.toString(prefixes, rt) + " " +
             predicate_.toString(prefixes, rt) + " " +
             object_.toString(prefixes, rt) + " .";
    }
};

} // namespace