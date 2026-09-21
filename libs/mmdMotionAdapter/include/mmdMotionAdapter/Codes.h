// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <mmdControl/Codes.h>

namespace mmd::codes {

inline constexpr Code MotionInvalidSampleRange{"MMD_MOTION_INVALID_SAMPLE_RANGE", Severity::Fatal};
inline constexpr Code MotionMissingRequiredJoint{"MMD_MOTION_MISSING_REQUIRED_JOINT",
                                                 Severity::Warning};

} // namespace mmd::codes
