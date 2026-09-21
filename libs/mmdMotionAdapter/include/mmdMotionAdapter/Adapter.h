// SPDX-License-Identifier: Apache-2.0
//
// Converts fully evaluated MMD control poses into the vendor-neutral shared
// motion values. It never retargets and knows no target avatar.
#pragma once

#include <mmdControl/Evaluator.h>
#include <mmdSkeletonAdapter/Adapter.h>
#include <motionCore/MotionPose.h>

namespace mmd::motion {

struct ClipOptions {
    double startTime = 0.0; ///< seconds, inclusive
    double endTime = 0.0;   ///< seconds, inclusive
    double samplesPerSecond = 30.0;
};

/// Evaluates `bound` at the requested rate and emits humanoid rotations, root
/// motion and source-preserving MMD morph channels. A valid range always has
/// a sample at both endpoints (one sample when they are equal). `model`,
/// `evaluator` and `skeleton` must have been built from the same canonical
/// document that `bound` targets.
Result<openstrata::motion::MotionClip> BuildClip(const CanonicalDocument& model,
                                                 const binding::BoundMotion& bound,
                                                 const control::Evaluator& evaluator,
                                                 const skeleton::AdaptedSkeleton& skeleton,
                                                 ClipOptions options = {});

} // namespace mmd::motion
