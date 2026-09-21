// SPDX-License-Identifier: Apache-2.0

#include <mmdSkeletonAdapter/Adapter.h>

#include <cassert>
#include <cmath>
#include <string>

namespace {

using openstrata::motion::HumanJoint;

int
AddBone(mmd::CanonicalDocument& model, std::string source, std::string english, std::string stable,
        int parent, mmd::Double3 position, std::size_t sourceIndex)
{
    mmd::Bone bone;
    bone.name = {std::move(source), std::move(english), stable};
    bone.sourceIndex = sourceIndex;
    bone.parent = parent;
    bone.position = position;
    if (parent == mmd::kNone) {
        bone.jointPath = stable;
        bone.localTranslation = position;
    } else {
        const mmd::Bone& p = model.skeleton.bones[static_cast<std::size_t>(parent)];
        bone.jointPath = p.jointPath + "/" + stable;
        bone.localTranslation = {
            position[0] - p.position[0], position[1] - p.position[1], position[2] - p.position[2]};
    }
    model.skeleton.bones.push_back(std::move(bone));
    return static_cast<int>(model.skeleton.bones.size() - 1);
}

void
TestRolesAndSkeleton()
{
    mmd::CanonicalDocument model;
    const int root = AddBone(model, "全ての親", "", "Root", mmd::kNone, {0.0, 0.0, 0.0}, 0);
    const int waist = AddBone(model, "腰", "", "Waist", root, {0.0, 1.0, 0.0}, 1);
    const int lower = AddBone(model, "下半身", "", "Lower", waist, {0.0, 1.1, 0.0}, 2);
    const int spine = AddBone(model, "上半身", "", "Spine", waist, {0.0, 1.4, 0.0}, 3);
    const int leftRegular = AddBone(model, "左足", "", "LeftLeg", waist, {0.1, 0.9, 0.0}, 4);
    const int leftD = AddBone(model, "左足D", "", "LeftLegD", waist, {0.1, 0.9, 0.0}, 40);
    const int right = AddBone(model, "右足", "", "RightLeg", waist, {-0.1, 0.9, 0.0}, 5);
    const int head = AddBone(model, "頭", "", "Head", spine, {0.0, 1.8, 0.0}, 6);
    AddBone(model, "別名", "左腕", "WrongEnglish", spine, {0.2, 1.5, 0.0}, 7);
    const int arm = AddBone(model, "左腕", "Bip001", "LeftArm", spine, {0.3, 1.5, 0.0}, 8);

    const mmd::skeleton::AdaptedSkeleton adapted = mmd::skeleton::Adapt(model);
    assert(adapted.roleTableVersion == 1);
    assert(adapted.SourceJoint(HumanJoint::Hips) == lower);
    assert(adapted.SourceJoint(HumanJoint::Spine) == spine);
    assert(adapted.SourceJoint(HumanJoint::LeftUpperLeg) == leftD);
    assert(adapted.SourceJoint(HumanJoint::RightUpperLeg) == right);
    assert(adapted.SourceJoint(HumanJoint::Head) == head);
    assert(adapted.SourceJoint(HumanJoint::LeftUpperArm) == arm);
    assert(adapted.SourceJoint(HumanJoint::Jaw) == -1);
    assert(adapted.targetMap.GetJointIndex(HumanJoint::Hips) == waist);
    assert(adapted.targetMap.GetJointIndex(HumanJoint::Spine) == spine);
    assert(adapted.targetMap.GetJointIndex(HumanJoint::LeftUpperLeg) == leftD);
    assert(leftRegular != leftD);

    assert(adapted.skeleton.GetSize() == model.skeleton.bones.size());
    assert(adapted.skeleton.GetJoints()[static_cast<std::size_t>(arm)].token ==
           "Root/Waist/Spine/LeftArm");
    assert(adapted.skeleton.GetJoints()[static_cast<std::size_t>(spine)].parent == waist);
    assert(
        std::abs(adapted.skeleton.GetJoints()[static_cast<std::size_t>(spine)].restTranslation[1] -
                 0.4f) < 1.0e-6f);

    const std::size_t spineRole = static_cast<std::size_t>(HumanJoint::Spine);
    assert(adapted.sourceRest.parents[spineRole] == static_cast<std::size_t>(HumanJoint::Hips));
    assert(std::abs(adapted.sourceRest.localTranslations[spineRole][1] - 0.3f) < 1.0e-6f);
    assert(adapted.requiredJoints.size() == 15);
}

void
TestTargetHipsRequiresBothLegs()
{
    mmd::CanonicalDocument model;
    const int root = AddBone(model, "腰", "", "Waist", mmd::kNone, {0.0, 0.0, 0.0}, 0);
    AddBone(model, "下半身", "", "Lower", root, {0.0, 0.1, 0.0}, 1);
    AddBone(model, "上半身", "", "Spine", root, {0.0, 0.2, 0.0}, 2);
    AddBone(model, "左足", "", "LeftLeg", root, {0.1, -0.1, 0.0}, 3);
    const mmd::skeleton::AdaptedSkeleton adapted = mmd::skeleton::Adapt(model);
    assert(!adapted.targetMap.IsMapped(HumanJoint::Hips));
}

} // namespace

int
main()
{
    TestRolesAndSkeleton();
    TestTargetHipsRequiresBothLegs();
    return 0;
}
