// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <mmdPmx/Diagnostic.h>
#include <mmdPmx/Document.h>

#include <string>
#include <vector>

namespace usdmmd {

/// The stage-contract version this authorer writes
/// (docs/design/STAGE_CONTRACT.md §2). The bundle's CMakeLists.txt reads this
/// line to stamp buildInfo.json, so it stays a single integer literal.
constexpr int kStageContractVersion = 1;

/// Authors the stage for one PMX as `.usda` text (DESIGN_POLICY.md §4).
///
/// Until Phase 2 it takes the parser's document directly and authors the
/// stage metadata and `/Asset` only. From Phase 2 it takes mmdModel's
/// CanonicalDocument instead, and never sees PMX syntax again.
class UsdMmdAuthorer {
public:
    /// `diagnostics` is in/out: it arrives holding every recoverable diagnostic
    /// raised so far, the authorer appends its own, and the whole list is
    /// recorded on `/Asset` (DIAGNOSTICS.md §4). Returns false only on an
    /// internal failure to author, never for anything in the source.
    bool WriteToString(
        const mmd::pmx::Document& document,
        std::vector<mmd::Diagnostic>* diagnostics,
        std::string* outUsda) const;
};

}  // namespace usdmmd
