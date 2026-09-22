// SPDX-License-Identifier: Apache-2.0

#include "generic_retarget.h"

openstrata::motion::RetargetedAnimation
RetargetGeneric(const openstrata::motion::MotionClip& clip,
                const openstrata::motion::SkeletonDescriptor& skeleton,
                const openstrata::motion::RetargetMap& map,
                const openstrata::motion::SourceRestPose& sourceRest,
                const std::vector<openstrata::motion::HumanJoint>& required,
                openstrata::motion::RetargetDiagnostics* diagnostics)
{
    openstrata::motion::RetargetOptions options;
    options.requiredBones = required;
    return openstrata::motion::PoseRetargeter(skeleton, map, sourceRest, options)
        .Retarget(clip, diagnostics);
}
