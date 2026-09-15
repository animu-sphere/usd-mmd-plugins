// SPDX-License-Identifier: Apache-2.0
//
// The diagnostic codes usdMmdFileFormat itself raises -- the events it, and
// not the parser or the canonical model, detects (docs/reference/
// DIAGNOSTICS.md §5). Same rules as mmdPmx/Codes.h: a code is never renamed,
// reused, or given another severity, and scripts/check_docs.py fails when
// this list and DIAGNOSTICS.md §5 disagree.
#pragma once

#include <mmdPmx/Diagnostic.h>

namespace usdmmd::codes {

// Skinning -- docs/design/STAGE_CONTRACT.md §9.4.
inline constexpr mmd::Code SkelSdefApproximated{
    "MMD_SKEL_SDEF_APPROXIMATED", mmd::Severity::Warning};
inline constexpr mmd::Code SkelQdefApproximated{
    "MMD_SKEL_QDEF_APPROXIMATED", mmd::Severity::Warning};

// Physics -- docs/design/PMX_CONTRACT.md §12.
inline constexpr mmd::Code PhysicsSoftBodyUnsupported{
    "MMD_PHYSICS_SOFT_BODY_UNSUPPORTED", mmd::Severity::Warning};

}  // namespace usdmmd::codes
