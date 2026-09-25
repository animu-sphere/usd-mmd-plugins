// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <mmdModel/CanonicalDocument.h>
#include <mmdPmx/Diagnostic.h>

#include <string>
#include <vector>

namespace usdmmd {

/// The stage-contract version this authorer writes
/// (docs/design/STAGE_CONTRACT.md §2). The bundle's CMakeLists.txt reads this
/// line to stamp buildInfo.json, so it stays a single integer literal.
constexpr int kStageContractVersion = 2;

/// Authors the stage for one model as `.usda` text (DESIGN_POLICY.md §4).
///
/// It reads the canonical document only -- never PMX syntax -- and converts
/// nothing: every value arrives in the USD basis and in meters, named and
/// ordered, and is authored as it is.
class UsdMmdAuthorer {
public:
    /// `diagnostics` is in/out: it arrives holding every recoverable diagnostic
    /// raised so far, the authorer appends its own, and the whole list is
    /// recorded on `/Asset` (DIAGNOSTICS.md §4). Returns false only on an
    /// internal failure to author, never for anything in the source.
    bool WriteToString(const mmd::CanonicalDocument& document,
                       std::vector<mmd::Diagnostic>* diagnostics, std::string* outUsda) const;
};

} // namespace usdmmd
