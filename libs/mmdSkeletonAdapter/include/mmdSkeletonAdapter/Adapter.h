// SPDX-License-Identifier: Apache-2.0
//
// The narrow PMX-skeleton edge into usd-motion-plugins. It owns MMD's
// versioned source-name role table, but no generic retarget algorithm.
#pragma once

#include <mmdModel/CanonicalDocument.h>
#include <motionRetarget/RestPose.h>
#include <motionRetarget/RetargetMap.h>
#include <motionRetarget/SkeletonDescriptor.h>

#include <array>
#include <bitset>
#include <vector>

namespace mmd::skeleton {

inline constexpr int kRoleTableVersion = 1;

/// Everything the shared motion core needs from one canonical PMX skeleton.
/// Source joints build VMD-derived clips; the target map drives the PMX stage.
struct AdaptedSkeleton {
    openstrata::motion::SkeletonDescriptor skeleton;
    openstrata::motion::RetargetMap targetMap;
    openstrata::motion::SourceRestPose sourceRest;
    std::array<int, openstrata::motion::HumanJointCount> sourceJoints{};
    std::bitset<openstrata::motion::HumanJointCount> sourcePresent;
    std::vector<openstrata::motion::HumanJoint> requiredJoints;
    int roleTableVersion = kRoleTableVersion;

    int SourceJoint(openstrata::motion::HumanJoint role) const noexcept;
};

/// Applies role-table version 1 to `model`, and builds target data whose joint
/// tokens and rest transforms exactly match /Asset/skel/Skeleton.
AdaptedSkeleton Adapt(const CanonicalDocument& model);

} // namespace mmd::skeleton
