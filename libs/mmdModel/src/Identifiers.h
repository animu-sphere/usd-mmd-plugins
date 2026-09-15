// SPDX-License-Identifier: Apache-2.0
//
// Stable identifiers, contract v1 (docs/design/TEXT_ENCODING_POLICY.md §6).
#pragma once

#include <mmdPmx/DiagnosticList.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace mmd::detail {

/// The longest identifier an English name yields before a digit prefix or a
/// collision suffix (TEXT_ENCODING_POLICY.md §6.1 step 1; TEXT-O2).
inline constexpr std::size_t kIdentifierCap = 64;

/// §6.1 step 1: every maximal run of characters outside [A-Za-z0-9_] becomes
/// one `_`, leading and trailing `_` are stripped, the result is cut to
/// kIdentifierCap characters, and a leading digit gains a `_`. Empty when the
/// English name holds no identifier character.
std::string IdentifierCandidate(std::string_view english);

/// §6.1 step 2: `<kind>_<NNNN>`, the source index zero-padded to four digits.
std::string FallbackIdentifier(std::string_view kind, std::size_t sourceIndex);

/// Identifiers for every element of one kind, in source order: element i's
/// English name is `englishNames[i]`. Unique ignoring ASCII case; an element
/// that had to be renamed raises MMD_USD_IDENTIFIER_COLLISION at
/// `<table>[i].englishName` (§6.1 step 3).
std::vector<std::string> AssignIdentifiers(std::string_view kind,
    const std::vector<std::string>& englishNames, const std::string& table,
    DiagnosticList& diagnostics);

}  // namespace mmd::detail
