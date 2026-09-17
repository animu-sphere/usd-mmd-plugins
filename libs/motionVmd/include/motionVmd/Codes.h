// SPDX-License-Identifier: Apache-2.0
//
// The diagnostic codes motionVmd raises. Each code's meaning is fixed by the
// design section named beside it, and its severity by
// docs/reference/DIAGNOSTICS.md §5; a code is never renamed, reused, or given
// another severity, and scripts/check_docs.py fails when this list and
// DIAGNOSTICS.md §5 disagree.
#pragma once

#include "motionVmd/Diagnostic.h"

namespace motionVmd::codes {

// VMD syntax -- docs/design/MOTION_CONTRACT.md §3.
inline constexpr Code MotionBadSignature{"MMD_MOTION_BAD_SIGNATURE", Severity::Fatal};
inline constexpr Code MotionTruncatedBuffer{"MMD_MOTION_TRUNCATED_BUFFER", Severity::Fatal};
inline constexpr Code MotionCountExceedsBuffer{"MMD_MOTION_COUNT_EXCEEDS_BUFFER", Severity::Fatal};
inline constexpr Code MotionTrailingBytes{"MMD_MOTION_TRAILING_BYTES", Severity::Warning};
inline constexpr Code MotionFileUnreadable{"MMD_MOTION_FILE_UNREADABLE", Severity::Fatal};

// Text -- docs/design/MOTION_CONTRACT.md §4.
inline constexpr Code TextTruncatedCp932{"MMD_TEXT_TRUNCATED_CP932", Severity::Info};
inline constexpr Code TextInvalidCp932{"MMD_TEXT_INVALID_CP932", Severity::Error};

// Tracks -- docs/design/MOTION_CONTRACT.md §5.
inline constexpr Code MotionDuplicateKeyframe{"MMD_MOTION_DUPLICATE_KEYFRAME", Severity::Warning};

} // namespace motionVmd::codes
