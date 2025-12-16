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

export module schema:agency;
import :core;
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

  // since subject does not change per triple, we can use placeholders for agency_id for easier readability
  const IRI subject = IRI("agencies", "{agency_id}");

  const std::vector<Triple> triples = {
    // SUBJECT                         PREDICATE           OBJECT
    // Type
    { subject,      {"rdf","type"},                  { IRI("gtfs","Agency") } },

    // Core fields
    { subject,      {"gtfs","agencyName"},           { "{agency_name}" } },
    { subject,      {"gtfs","agencyUrl"},            { "{agency_url}", IRI("xsd","anyURI") } },
    { subject,      {"gtfs","agencyTimezone"},       { "{agency_timezone}" } },

    // Optional fields
    { subject,      {"gtfs","agencyLang"},           { "{agency_lang}", IRI("xsd","language") } },
    { subject,      {"gtfs","agencyPhone"},          { "{agency_phone}" } },
    { subject,      {"gtfs","agencyFareUrl"},        { "{agency_fare_url}", IRI("xsd","anyURI") } },
    { subject,      {"gtfs","agencyEmail"},          { "{agency_email}" } },

    // cEMV support enum (0/1/2)
    { subject,      {"gtfs","cemvSupport"},          { "{cemv_support}", IRI("xsd","integer") } }
  };

  Schema sc("agencies.txt", possible_columns, prefixes, triples, registry);
  return sc;
}

} // namespace
