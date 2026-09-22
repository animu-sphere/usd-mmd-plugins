// SPDX-License-Identifier: Apache-2.0
//
// Deliberately generic side of the Phase 9 acceptance path. This translation
// unit's interface names only shared-motion values: no MMD type crosses it.
#pragma once

#include <motionRetarget/PoseRetargeter.h>

#include <vector>

openstrata::motion::RetargetedAnimation
RetargetGeneric(const openstrata::motion::MotionClip& clip,
                const openstrata::motion::SkeletonDescriptor& skeleton,
                const openstrata::motion::RetargetMap& map,
                const openstrata::motion::SourceRestPose& sourceRest,
                const std::vector<openstrata::motion::HumanJoint>& required,
                openstrata::motion::RetargetDiagnostics* diagnostics);
