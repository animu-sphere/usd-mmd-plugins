// SPDX-License-Identifier: Apache-2.0
//
// The diagnostic codes mmdControl raises -- what preparing a model for
// evaluation detects. Same rules as mmdPmx/Codes.h: a code is never renamed,
// reused, or given another severity, and scripts/check_docs.py fails when this
// list and docs/reference/DIAGNOSTICS.md §5 disagree.
#pragma once

#include <mmdPmx/Diagnostic.h>

namespace mmd::codes {

// Control evaluation -- docs/design/MOTION_CONTRACT.md §11.8.
inline constexpr Code MotionExternalParentIgnored{"MMD_MOTION_EXTERNAL_PARENT_IGNORED",
                                                  Severity::Info};
inline constexpr Code MotionLocalAppendApproximated{"MMD_MOTION_LOCAL_APPEND_APPROXIMATED",
                                                    Severity::Info};
inline constexpr Code MotionIkLoopClamped{"MMD_MOTION_IK_LOOP_CLAMPED", Severity::Warning};

} // namespace mmd::codes
