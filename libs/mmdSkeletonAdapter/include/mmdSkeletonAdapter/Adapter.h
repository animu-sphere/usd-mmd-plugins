// SPDX-License-Identifier: Apache-2.0
//
// The narrow PMX-skeleton edge into usd-motion-plugins. It owns MMD's
// versioned source-name role table, but no generic retarget algorithm.
#pragma once

#include <mmdModel/CanonicalDocument.h>
#include <motionRetarget/PoseRetargeter.h>
#include <motionRetarget/RestPose.h>
#include <motionRetarget/RetargetMap.h>
#include <motionRetarget/SkeletonDescriptor.h>

#include <array>
#include <bitset>
#include <vector>

namespace mmd::skeleton {

inline constexpr int kRoleTableVersion = 2;

/// An arm joint whose roll about its own bone belongs on the twist bone below
/// it (MOTION_CONTRACT.md §10.10): `upperArm` onto `腕捩`, `lowerArm` onto
/// `手捩`. Joints are skeleton joint indices.
struct ArmTwist {
    int joint = -1;
    int twist = -1;
    /// The joint's unit bone direction at rest, toward its follower.
    pxr::GfVec3f axis{0.0f};
};

/// Everything the shared motion core needs from one canonical PMX skeleton.
/// Source joints build VMD-derived clips; the target map drives the PMX stage.
/// `sourceRest` and `targetRest` state the arm chain's T-pose aim
/// (MOTION_CONTRACT.md §12.6); a retarget onto this skeleton passes
/// `targetRest` as `RetargetOptions::targetRest`, or its arms land off by the
/// model's own rest angle. `armTwists` and `heldJoints` finish a pose
/// retargeted onto this skeleton (MOTION_CONTRACT.md §10.10).
struct AdaptedSkeleton {
    openstrata::motion::SkeletonDescriptor skeleton;
    openstrata::motion::RetargetMap targetMap;
    openstrata::motion::SourceRestPose sourceRest;
    openstrata::motion::TargetRestPose targetRest;
    std::array<int, openstrata::motion::HumanJointCount> sourceJoints{};
    std::bitset<openstrata::motion::HumanJointCount> sourcePresent;
    std::vector<openstrata::motion::HumanJoint> requiredJoints;
    int roleTableVersion = kRoleTableVersion;
    /// Each arm joint with a twist bone between it and its follower.
    std::vector<ArmTwist> armTwists;
    /// One per skeleton joint: the bound joints, the twist bones of
    /// `armTwists`, and every ancestor of either. A finish keeps them as the
    /// retarget left them (mmdControl's `Evaluator::Complete`).
    std::vector<bool> heldJoints;

    int SourceJoint(openstrata::motion::HumanJoint role) const noexcept;
};

/// Applies role-table version 2 to `model`, and builds target data whose joint
/// tokens and rest transforms exactly match /Asset/skel/Skeleton; the arm
/// chain's reference rest is stated beside them, never in them.
AdaptedSkeleton Adapt(const CanonicalDocument& model);

/// Moves each arm joint's roll about its bone onto its twist bone
/// (MOTION_CONTRACT.md §10.10, step 1): the joint keeps the swing, and the
/// twist is composed onto the twist bone, its offset turned with it, so the
/// twist bone and every joint below keep their world transforms. `pose` is a
/// pose retargeted onto `adapted.skeleton`.
void CarryArmRoll(const AdaptedSkeleton& adapted, openstrata::motion::RetargetedPose& pose);

/// The same, for every sample.
void CarryArmRoll(const AdaptedSkeleton& adapted, openstrata::motion::RetargetedAnimation& animation);

} // namespace mmd::skeleton
