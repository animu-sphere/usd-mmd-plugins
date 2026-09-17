// SPDX-License-Identifier: Apache-2.0
//
// The diagnostic codes mmdMotionBinding raises -- the events binding a motion
// to a model detects. Same rules as mmdPmx/Codes.h: a code is never renamed,
// reused, or given another severity, and scripts/check_docs.py fails when this
// list and docs/reference/DIAGNOSTICS.md §5 disagree.
#pragma once

#include <mmdPmx/Diagnostic.h>

namespace mmd::codes {

// Binding -- docs/design/MOTION_CONTRACT.md §8.1.
inline constexpr Code MotionUnmatchedBone{"MMD_MOTION_UNMATCHED_BONE", Severity::Info};
inline constexpr Code MotionUnmatchedMorph{"MMD_MOTION_UNMATCHED_MORPH", Severity::Info};
inline constexpr Code MotionAmbiguousName{"MMD_MOTION_AMBIGUOUS_NAME", Severity::Warning};
inline constexpr Code MotionUnencodableName{"MMD_MOTION_UNENCODABLE_NAME", Severity::Info};

} // namespace mmd::codes
