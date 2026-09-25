// SPDX-License-Identifier: Apache-2.0

#include "mmdSkeletonAdapter/Adapter.h"

#include <algorithm>
#include <array>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mmd::skeleton {
namespace {

using openstrata::motion::HumanJoint;
using openstrata::motion::HumanJointCount;

constexpr int kUnmapped = openstrata::motion::RetargetMap::kUnmapped;

struct RoleSpec {
    HumanJoint role;
    std::initializer_list<std::string_view> candidates;
};

const std::array<RoleSpec, 54>&
RoleSpecs()
{
    static const std::array<RoleSpec, 54> specs{{
        {HumanJoint::Hips, {"下半身"}},
        {HumanJoint::Spine, {"上半身"}},
        {HumanJoint::Chest, {"上半身2"}},
        {HumanJoint::UpperChest, {"上半身3"}},
        {HumanJoint::Neck, {"首"}},
        {HumanJoint::Head, {"頭"}},
        {HumanJoint::LeftEye, {"左目"}},
        {HumanJoint::RightEye, {"右目"}},
        {HumanJoint::LeftUpperLeg, {"左足D", "左足"}},
        {HumanJoint::LeftLowerLeg, {"左ひざD", "左ひざ"}},
        {HumanJoint::LeftFoot, {"左足首D", "左足首"}},
        {HumanJoint::LeftToes, {"左足先EX"}},
        {HumanJoint::RightUpperLeg, {"右足D", "右足"}},
        {HumanJoint::RightLowerLeg, {"右ひざD", "右ひざ"}},
        {HumanJoint::RightFoot, {"右足首D", "右足首"}},
        {HumanJoint::RightToes, {"右足先EX"}},
        {HumanJoint::LeftShoulder, {"左肩"}},
        {HumanJoint::LeftUpperArm, {"左腕"}},
        {HumanJoint::LeftLowerArm, {"左ひじ"}},
        {HumanJoint::LeftHand, {"左手首"}},
        {HumanJoint::RightShoulder, {"右肩"}},
        {HumanJoint::RightUpperArm, {"右腕"}},
        {HumanJoint::RightLowerArm, {"右ひじ"}},
        {HumanJoint::RightHand, {"右手首"}},
        {HumanJoint::LeftThumbMetacarpal, {"左親指０"}},
        {HumanJoint::LeftThumbProximal, {"左親指１"}},
        {HumanJoint::LeftThumbDistal, {"左親指２"}},
        {HumanJoint::LeftIndexProximal, {"左人指１"}},
        {HumanJoint::LeftIndexIntermediate, {"左人指２"}},
        {HumanJoint::LeftIndexDistal, {"左人指３"}},
        {HumanJoint::LeftMiddleProximal, {"左中指１"}},
        {HumanJoint::LeftMiddleIntermediate, {"左中指２"}},
        {HumanJoint::LeftMiddleDistal, {"左中指３"}},
        {HumanJoint::LeftRingProximal, {"左薬指１"}},
        {HumanJoint::LeftRingIntermediate, {"左薬指２"}},
        {HumanJoint::LeftRingDistal, {"左薬指３"}},
        {HumanJoint::LeftLittleProximal, {"左小指１"}},
        {HumanJoint::LeftLittleIntermediate, {"左小指２"}},
        {HumanJoint::LeftLittleDistal, {"左小指３"}},
        {HumanJoint::RightThumbMetacarpal, {"右親指０"}},
        {HumanJoint::RightThumbProximal, {"右親指１"}},
        {HumanJoint::RightThumbDistal, {"右親指２"}},
        {HumanJoint::RightIndexProximal, {"右人指１"}},
        {HumanJoint::RightIndexIntermediate, {"右人指２"}},
        {HumanJoint::RightIndexDistal, {"右人指３"}},
        {HumanJoint::RightMiddleProximal, {"右中指１"}},
        {HumanJoint::RightMiddleIntermediate, {"右中指２"}},
        {HumanJoint::RightMiddleDistal, {"右中指３"}},
        {HumanJoint::RightRingProximal, {"右薬指１"}},
        {HumanJoint::RightRingIntermediate, {"右薬指２"}},
        {HumanJoint::RightRingDistal, {"右薬指３"}},
        {HumanJoint::RightLittleProximal, {"右小指１"}},
        {HumanJoint::RightLittleIntermediate, {"右小指２"}},
        {HumanJoint::RightLittleDistal, {"右小指３"}},
    }};
    return specs;
}

const std::vector<HumanJoint>&
RequiredJoints()
{
    static const std::vector<HumanJoint> required{
        HumanJoint::Hips,
        HumanJoint::Spine,
        HumanJoint::Head,
        HumanJoint::LeftUpperLeg,
        HumanJoint::LeftLowerLeg,
        HumanJoint::LeftFoot,
        HumanJoint::RightUpperLeg,
        HumanJoint::RightLowerLeg,
        HumanJoint::RightFoot,
        HumanJoint::LeftUpperArm,
        HumanJoint::LeftLowerArm,
        HumanJoint::LeftHand,
        HumanJoint::RightUpperArm,
        HumanJoint::RightLowerArm,
        HumanJoint::RightHand,
    };
    return required;
}

int
FindBone(const CanonicalDocument& model, std::initializer_list<std::string_view> candidates)
{
    for (const std::string_view candidate : candidates) {
        int found = kUnmapped;
        std::size_t sourceIndex = std::numeric_limits<std::size_t>::max();
        for (std::size_t i = 0; i < model.skeleton.bones.size(); ++i) {
            const Bone& bone = model.skeleton.bones[i];
            if (bone.name.source == candidate && bone.sourceIndex < sourceIndex) {
                found = static_cast<int>(i);
                sourceIndex = bone.sourceIndex;
            }
        }
        if (found != kUnmapped) {
            return found;
        }
    }
    return kUnmapped;
}

bool
IsAncestor(const std::vector<Bone>& bones, int ancestor, int joint)
{
    while (joint != kNone && joint >= 0 && static_cast<std::size_t>(joint) < bones.size()) {
        if (joint == ancestor) {
            return true;
        }
        joint = bones[static_cast<std::size_t>(joint)].parent;
    }
    return false;
}

// Version 2 (MOTION_CONTRACT.md §12.2, MOT-O12). As a target, `上半身2` and
// `上半身3` take `chest` and `upperChest` in the order the model chains them,
// ancestor first; where neither is the other's ancestor, `上半身3` is off the
// neck's chain and `upperChest` is left unbound.
void
OrderUpperBody(const CanonicalDocument& model, std::array<int, HumanJointCount>& joints)
{
    int& chest = joints[static_cast<std::size_t>(HumanJoint::Chest)];
    int& upperChest = joints[static_cast<std::size_t>(HumanJoint::UpperChest)];
    if (chest == kUnmapped || upperChest == kUnmapped) {
        return;
    }
    if (IsAncestor(model.skeleton.bones, chest, upperChest)) {
        return;
    }
    if (IsAncestor(model.skeleton.bones, upperChest, chest)) {
        std::swap(chest, upperChest);
        return;
    }
    upperChest = kUnmapped;
}

// As a source, `upperChest` is never emitted: `chest` is the upper-body bone
// the neck and shoulders hang from, whose evaluated world rotation already
// holds every torso bone below it. The shared retarget drops an intermediate
// joint a target lacks rather than folding it into its child (its
// RETARGETING_POLICY §4.1, case 6), so an emitted `upperChest` would move the
// neck and arms of every target without one.
void
SourceUpperBody(std::array<int, HumanJointCount>& joints)
{
    int& chest = joints[static_cast<std::size_t>(HumanJoint::Chest)];
    int& upperChest = joints[static_cast<std::size_t>(HumanJoint::UpperChest)];
    if (upperChest != kUnmapped) {
        chest = upperChest;
        upperChest = kUnmapped;
    }
}

int
TargetHips(const CanonicalDocument& model, const std::array<int, HumanJointCount>& source)
{
    const int spine = source[static_cast<std::size_t>(HumanJoint::Spine)];
    const int left = source[static_cast<std::size_t>(HumanJoint::LeftUpperLeg)];
    const int right = source[static_cast<std::size_t>(HumanJoint::RightUpperLeg)];
    if (spine == kUnmapped || left == kUnmapped || right == kUnmapped) {
        return kUnmapped;
    }
    int candidate = spine;
    while (candidate != kNone && candidate >= 0 &&
           static_cast<std::size_t>(candidate) < model.skeleton.bones.size()) {
        if (IsAncestor(model.skeleton.bones, candidate, left) &&
            IsAncestor(model.skeleton.bones, candidate, right)) {
            return candidate;
        }
        candidate = model.skeleton.bones[static_cast<std::size_t>(candidate)].parent;
    }
    return kUnmapped;
}

} // namespace

int
AdaptedSkeleton::SourceJoint(openstrata::motion::HumanJoint role) const noexcept
{
    const std::size_t index = static_cast<std::size_t>(role);
    return index < sourceJoints.size() ? sourceJoints[index] : kUnmapped;
}

AdaptedSkeleton
Adapt(const CanonicalDocument& model)
{
    AdaptedSkeleton adapted;
    adapted.sourceJoints.fill(kUnmapped);
    adapted.requiredJoints = RequiredJoints();

    std::array<int, HumanJointCount> targetJoints{};
    targetJoints.fill(kUnmapped);
    for (const RoleSpec& spec : RoleSpecs()) {
        targetJoints[static_cast<std::size_t>(spec.role)] = FindBone(model, spec.candidates);
    }
    OrderUpperBody(model, targetJoints);
    adapted.sourceJoints = targetJoints;
    SourceUpperBody(adapted.sourceJoints);
    for (std::size_t role = 0; role < HumanJointCount; ++role) {
        adapted.sourcePresent.set(role, adapted.sourceJoints[role] != kUnmapped);
    }

    std::vector<std::string> tokens;
    std::vector<pxr::GfMatrix4d> rests;
    tokens.reserve(model.skeleton.bones.size());
    rests.reserve(model.skeleton.bones.size());
    for (const Bone& bone : model.skeleton.bones) {
        tokens.push_back(bone.jointPath);
        pxr::GfMatrix4d rest(1.0);
        rest.SetTranslateOnly(pxr::GfVec3d(
            bone.localTranslation[0], bone.localTranslation[1], bone.localTranslation[2]));
        rests.push_back(rest);
    }
    const openstrata::motion::SkeletonDescriptorResult built =
        openstrata::motion::BuildSkeletonDescriptor(tokens, rests);
    if (built.skeleton) {
        adapted.skeleton = *built.skeleton;
    }

    for (const RoleSpec& spec : RoleSpecs()) {
        int joint = targetJoints[static_cast<std::size_t>(spec.role)];
        if (spec.role == HumanJoint::Hips) {
            joint = TargetHips(model, adapted.sourceJoints);
        }
        if (joint != kUnmapped) {
            adapted.targetMap.SetJointIndex(spec.role, joint, model.skeleton.bones.size());
        }
    }

    for (std::size_t i = 0; i < HumanJointCount; ++i) {
        if (!adapted.sourcePresent.test(i)) {
            continue;
        }
        const HumanJoint role = static_cast<HumanJoint>(i);
        const int joint = adapted.sourceJoints[i];
        const auto parent = openstrata::motion::NearestPresentAncestor(role, adapted.sourcePresent);
        if (parent) {
            adapted.sourceRest.SetParent(role, *parent);
        }

        const Double3& position = model.skeleton.bones[static_cast<std::size_t>(joint)].position;
        pxr::GfVec3f translation(static_cast<float>(position[0]),
                                 static_cast<float>(position[1]),
                                 static_cast<float>(position[2]));
        if (parent) {
            const int parentJoint = adapted.SourceJoint(*parent);
            const Double3& parentPosition =
                model.skeleton.bones[static_cast<std::size_t>(parentJoint)].position;
            translation -= pxr::GfVec3f(static_cast<float>(parentPosition[0]),
                                        static_cast<float>(parentPosition[1]),
                                        static_cast<float>(parentPosition[2]));
        }
        adapted.sourceRest.localTranslations[i] = translation;
    }

    return adapted;
}

} // namespace mmd::skeleton
