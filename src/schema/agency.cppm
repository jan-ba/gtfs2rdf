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
#include <vector>
#include <optional>
#include <stdexcept>

export module schema.agency;
import schema.core;
import rdf_components;
import field_transforms;
import t_lib;

using namespace rdf;

namespace schema {

// GTFS -> RDF schema for agency.txt
export Schema buildAgencySchema(field_transforms::TransformRegistry& registry) {
  const std::vector<std::string> possible_columns = {
    "agency_id",
    "agency_name",
    "agency_url",
    "agency_timezone",
    "agency_lang",
    "agency_phone",
    "agency_fare_url",
    "agency_email",
    "cemv_support"
  };

  const std::unordered_map<std::string, std::string> prefixes = {
    { "agencies", "https://gtfs.de/agencies/" },
    { "rdf",      "http://www.w3.org/1999/02/22-rdf-syntax-ns#" },
    { "xsd",      "http://www.w3.org/2001/XMLSchema#" },
    { "gtfs",     "https://w3id.org/gtfs2rdf#" }
  };

  const std::vector<Triple> triples = {
    // SUBJECT                         PREDICATE           OBJECT
    // Type
    { {"agencies","{agency_id}"},      {"rdf","type"},
                                       { IRI("gtfs","Agency") } },

    // Core fields
    { {"agencies","{agency_id}"},      {"gtfs","agencyName"},
                                       { "{agency_name}" } },
    { {"agencies","{agency_id}"},      {"gtfs","agencyUrl"},
                                       { "{agency_url}", IRI("xsd","anyURI") } },
    { {"agencies","{agency_id}"},      {"gtfs","agencyTimezone"},
                                       { "{agency_timezone}" } },

    // Optional fields
    { {"agencies","{agency_id}"},      {"gtfs","agencyLang"},
                                       { "{agency_lang}", IRI("xsd","language") } },
    { {"agencies","{agency_id}"},      {"gtfs","agencyPhone"},
                                       { "{agency_phone}" } },
    { {"agencies","{agency_id}"},      {"gtfs","agencyFareUrl"},
                                       { "{agency_fare_url}", IRI("xsd","anyURI") } },
    { {"agencies","{agency_id}"},      {"gtfs","agencyEmail"},
                                       { "{agency_email}" } },

    // cEMV support enum (0/1/2)
    { {"agencies","{agency_id}"},      {"gtfs","cemvSupport"},
                                       { "{cemv_support}", IRI("xsd","integer") } }
  };

  Schema sc("agencies.txt", possible_columns, prefixes, triples, registry);
  return sc;
}

} // namespace
