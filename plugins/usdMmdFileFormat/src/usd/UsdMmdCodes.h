// SPDX-License-Identifier: Apache-2.0
//
// The diagnostic codes usdMmdFileFormat itself raises -- the events it, and
// not the parser, detects (docs/reference/DIAGNOSTICS.md §5). Same rules as
// mmdPmx/Codes.h: a code is never renamed, reused, or given another severity,
// and scripts/check_docs.py fails when this list and DIAGNOSTICS.md §5
// disagree.
#pragma once

#include <mmdPmx/Diagnostic.h>

namespace usdmmd::codes {

// Physics -- docs/design/PMX_CONTRACT.md §12.
inline constexpr mmd::Code PhysicsSoftBodyUnsupported{
    "MMD_PHYSICS_SOFT_BODY_UNSUPPORTED", mmd::Severity::Warning};

}  // namespace usdmmd::codes
