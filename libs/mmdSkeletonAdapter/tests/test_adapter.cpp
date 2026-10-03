// SPDX-License-Identifier: Apache-2.0

#include <mmdSkeletonAdapter/Adapter.h>

#include <motionRetarget/PoseRetargeter.h>

#include <pxr/base/gf/rotation.h>

#include <algorithm>
#include <bitset>
#include <cassert>
#include <cmath>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

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
    assert(adapted.roleTableVersion == 2);
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

// MOT-O12: as a target, `上半身2` and `上半身3` bind `chest` and `upperChest`
// in the model's own order, ancestor first; as a source, `chest` is the bone
// the neck hangs from and `upperChest` is never emitted.
void
TestUpperBodyFollowsTheChain()
{
    // Conventional: 上半身 → 上半身2 → 上半身3 → 首.
    {
        mmd::CanonicalDocument model;
        const int spine = AddBone(model, "上半身", "", "Spine", mmd::kNone, {0.0, 1.0, 0.0}, 0);
        const int two = AddBone(model, "上半身2", "", "Spine2", spine, {0.0, 1.1, 0.0}, 1);
        const int three = AddBone(model, "上半身3", "", "Spine3", two, {0.0, 1.2, 0.0}, 2);
        AddBone(model, "首", "", "Neck", three, {0.0, 1.4, 0.0}, 3);
        const mmd::skeleton::AdaptedSkeleton adapted = mmd::skeleton::Adapt(model);
        assert(adapted.targetMap.GetJointIndex(HumanJoint::Chest) == two);
        assert(adapted.targetMap.GetJointIndex(HumanJoint::UpperChest) == three);
        assert(adapted.SourceJoint(HumanJoint::Chest) == three);
        assert(adapted.SourceJoint(HumanJoint::UpperChest) == -1);
        assert(!adapted.sourcePresent.test(static_cast<std::size_t>(HumanJoint::UpperChest)));
        assert(adapted.sourceRest.parents[static_cast<std::size_t>(HumanJoint::Neck)] ==
               static_cast<std::size_t>(HumanJoint::Chest));
    }
    // Inserted: 上半身 → 上半身3 → 上半身2 → 首, as both local models with 上半身3 are.
    {
        mmd::CanonicalDocument model;
        const int spine = AddBone(model, "上半身", "", "Spine", mmd::kNone, {0.0, 1.0, 0.0}, 0);
        const int three = AddBone(model, "上半身3", "", "Spine3", spine, {0.0, 1.1, 0.0}, 1);
        const int two = AddBone(model, "上半身2", "", "Spine2", three, {0.0, 1.2, 0.0}, 2);
        AddBone(model, "首", "", "Neck", two, {0.0, 1.4, 0.0}, 3);
        const mmd::skeleton::AdaptedSkeleton adapted = mmd::skeleton::Adapt(model);
        assert(adapted.targetMap.GetJointIndex(HumanJoint::Chest) == three);
        assert(adapted.targetMap.GetJointIndex(HumanJoint::UpperChest) == two);
        assert(adapted.SourceJoint(HumanJoint::Chest) == two);
        assert(adapted.SourceJoint(HumanJoint::UpperChest) == -1);
        const std::size_t chest = static_cast<std::size_t>(HumanJoint::Chest);
        assert(adapted.sourceRest.parents[chest] == static_cast<std::size_t>(HumanJoint::Spine));
        assert(std::abs(adapted.sourceRest.localTranslations[chest][1] - 0.2f) < 1.0e-6f);
    }
    // Off the chain: 上半身3 is a sibling of 上半身2, so it binds nothing.
    {
        mmd::CanonicalDocument model;
        const int spine = AddBone(model, "上半身", "", "Spine", mmd::kNone, {0.0, 1.0, 0.0}, 0);
        const int two = AddBone(model, "上半身2", "", "Spine2", spine, {0.0, 1.2, 0.0}, 1);
        AddBone(model, "上半身3", "", "Spine3", spine, {0.0, 1.1, 0.0}, 2);
        const mmd::skeleton::AdaptedSkeleton adapted = mmd::skeleton::Adapt(model);
        assert(adapted.SourceJoint(HumanJoint::Chest) == two);
        assert(adapted.SourceJoint(HumanJoint::UpperChest) == -1);
        assert(adapted.targetMap.GetJointIndex(HumanJoint::Chest) == two);
        assert(!adapted.targetMap.IsMapped(HumanJoint::UpperChest));
    }
}


// MOT-O10, MOTION_CONTRACT.md §12.6. Arms whose upper and lower segments hang
// `armDegrees` below horizontal, shoulders `shoulderDegrees`, with 肩C and a
// twist bone between chain joints, as distributed models have them, and two
// fingers under each hand.
struct ArmModel {
    mmd::CanonicalDocument model;
    int chest = -1;
    int shoulderC[2]{-1, -1};
    int twist[2]{-1, -1};
    int finger[2]{-1, -1};
};

pxr::GfVec3f
Down(float side, float degrees)
{
    const float radians = degrees * 3.14159265f / 180.0f;
    return pxr::GfVec3f(side * std::cos(radians), -std::sin(radians), 0.0f);
}

mmd::Double3
At(const pxr::GfVec3f& p)
{
    return {p[0], p[1], p[2]};
}

ArmModel
MakeArmModel(float armDegrees, float shoulderDegrees, float scale)
{
    ArmModel arms;
    mmd::CanonicalDocument& model = arms.model;
    std::size_t index = 0;
    const int spine = AddBone(model, "上半身", "", "Spine", mmd::kNone, {0.0, scale * 1.0, 0.0}, index++);
    arms.chest = AddBone(model, "上半身2", "", "Chest", spine, {0.0, scale * 1.2, 0.0}, index++);
    AddBone(model, "首", "", "Neck", arms.chest, {0.0, scale * 1.4, 0.0}, index++);
    for (int s = 0; s < 2; ++s) {
        const float side = s == 0 ? 1.0f : -1.0f;
        const std::string jp = s == 0 ? "左" : "右";
        const std::string en = s == 0 ? "Left" : "Right";
        const pxr::GfVec3f shoulderAt(side * 0.02f * scale, 1.35f * scale, 0.0f);
        const pxr::GfVec3f upperAt = shoulderAt + Down(side, shoulderDegrees) * 0.1f * scale;
        const pxr::GfVec3f lowerAt = upperAt + Down(side, armDegrees) * 0.25f * scale;
        const pxr::GfVec3f handAt = lowerAt + Down(side, armDegrees) * 0.25f * scale;
        const int shoulder = AddBone(model, jp + "肩", "", en + "Shoulder", arms.chest, At(shoulderAt), index++);
        arms.shoulderC[s] = AddBone(model, jp + "肩C", "", en + "ShoulderC", shoulder, At(upperAt), index++);
        const int upper = AddBone(model, jp + "腕", "", en + "Arm", arms.shoulderC[s], At(upperAt), index++);
        arms.twist[s] = AddBone(model, jp + "腕捩", "", en + "ArmTwist", upper,
                                At(upperAt + Down(side, armDegrees) * 0.12f * scale), index++);
        const int lower = AddBone(model, jp + "ひじ", "", en + "Elbow", arms.twist[s], At(lowerAt), index++);
        const int hand = AddBone(model, jp + "手首", "", en + "Wrist", lower, At(handAt), index++);
        arms.finger[s] = AddBone(model, jp + "人指１", "", en + "Index1", hand,
                                 At(handAt + Down(side, armDegrees) * 0.08f * scale + pxr::GfVec3f(0.0f, 0.0f, 0.02f)),
                                 index++);
        AddBone(model, jp + "中指１", "", en + "Middle1", hand,
                At(handAt + Down(side, armDegrees) * 0.08f * scale), index++);
    }
    return arms;
}

pxr::GfVec3f
Position(const mmd::CanonicalDocument& model, int joint)
{
    const mmd::Double3& p = model.skeleton.bones[static_cast<std::size_t>(joint)].position;
    return pxr::GfVec3f(static_cast<float>(p[0]), static_cast<float>(p[1]), static_cast<float>(p[2]));
}

double
AngleDegrees(const pxr::GfVec3f& a, const pxr::GfVec3f& b)
{
    // atan2 rather than acos: near zero, acos of a float dot loses ~0.02°.
    const pxr::GfVec3d x(a), y(b);
    return std::atan2(pxr::GfCross(x, y).GetLength(), pxr::GfDot(x, y)) * 180.0 /
           3.14159265358979323846;
}

bool
SameRotation(const pxr::GfQuatf& a, const pxr::GfQuatf& b)
{
    return std::fabs(pxr::GfDot(a.GetNormalized(), b.GetNormalized())) > 1.0f - 1.0e-6f;
}

// The segments §12.6 aims, each from a role to its follower, with its side.
struct Segment {
    HumanJoint from;
    HumanJoint to;
    float side;
};
const Segment kSegments[] = {
    {HumanJoint::LeftShoulder, HumanJoint::LeftUpperArm, 1.0f},
    {HumanJoint::LeftUpperArm, HumanJoint::LeftLowerArm, 1.0f},
    {HumanJoint::LeftLowerArm, HumanJoint::LeftHand, 1.0f},
    {HumanJoint::RightShoulder, HumanJoint::RightUpperArm, -1.0f},
    {HumanJoint::RightUpperArm, HumanJoint::RightLowerArm, -1.0f},
    {HumanJoint::RightLowerArm, HumanJoint::RightHand, -1.0f},
};

void
TestArmChainReferenceRest()
{
    const ArmModel arms = MakeArmModel(40.0f, 10.0f, 1.0f);
    const mmd::skeleton::AdaptedSkeleton adapted = mmd::skeleton::Adapt(arms.model);

    // As a source: each chain role's world rest turns its bone onto the lateral
    // axis; the hand, with two fingers and no follower, inherits the lower arm.
    for (const Segment& segment : kSegments) {
        const pxr::GfVec3f bone = Position(arms.model, adapted.SourceJoint(segment.to)) -
                                  Position(arms.model, adapted.SourceJoint(segment.from));
        const pxr::GfQuatf world = adapted.sourceRest.GetWorldRestRotation(segment.from);
        assert(AngleDegrees(world.Transform(bone), pxr::GfVec3f(segment.side, 0.0f, 0.0f)) < 1.0e-3);
    }
    assert(SameRotation(adapted.sourceRest.GetWorldRestRotation(HumanJoint::LeftHand),
                        adapted.sourceRest.GetWorldRestRotation(HumanJoint::LeftLowerArm)));
    for (const HumanJoint role : {HumanJoint::Spine, HumanJoint::Chest, HumanJoint::Neck,
                                  HumanJoint::LeftHand, HumanJoint::LeftIndexProximal,
                                  HumanJoint::RightMiddleProximal}) {
        assert(SameRotation(adapted.sourceRest.localRotations[static_cast<std::size_t>(role)],
                            pxr::GfQuatf(1.0f)));
    }
    assert(!SameRotation(
        adapted.sourceRest.localRotations[static_cast<std::size_t>(HumanJoint::LeftUpperArm)],
        pxr::GfQuatf(1.0f)));

    // As a target: the same world rests over the stage's own joints. 肩C, the
    // twist bone and the fingers stay unset and pass the aim on.
    const openstrata::motion::TargetRestPose& rest = adapted.targetRest;
    assert(rest.localRotations.size() == adapted.skeleton.GetSize());
    for (const Segment& segment : kSegments) {
        const int joint = adapted.targetMap.GetJointIndex(segment.from);
        assert(rest.localRotations[static_cast<std::size_t>(joint)].has_value());
        assert(SameRotation(rest.GetWorldRestRotation(adapted.skeleton, joint),
                            adapted.sourceRest.GetWorldRestRotation(segment.from)));
    }
    for (int s = 0; s < 2; ++s) {
        assert(!rest.localRotations[static_cast<std::size_t>(arms.shoulderC[s])].has_value());
        assert(!rest.localRotations[static_cast<std::size_t>(arms.twist[s])].has_value());
        assert(!rest.localRotations[static_cast<std::size_t>(arms.finger[s])].has_value());
    }
    assert(!rest.localRotations[static_cast<std::size_t>(arms.chest)].has_value());
    const int leftUpper = adapted.targetMap.GetJointIndex(HumanJoint::LeftUpperArm);
    const int leftHand = adapted.targetMap.GetJointIndex(HumanJoint::LeftHand);
    assert(SameRotation(rest.GetWorldRestRotation(adapted.skeleton, arms.twist[0]),
                        rest.GetWorldRestRotation(adapted.skeleton, leftUpper)));
    assert(SameRotation(rest.GetWorldRestRotation(adapted.skeleton, arms.finger[0]),
                        rest.GetWorldRestRotation(adapted.skeleton, leftHand)));

    // Not the stage: the descriptor still states identity rests.
    for (const openstrata::motion::SkeletonJoint& joint : adapted.skeleton.GetJoints()) {
        assert(SameRotation(joint.restRotation, pxr::GfQuatf(1.0f)));
    }
}

// A level-arm humanoid with identity rests, as a normalized VRM rests.
struct LevelRig {
    openstrata::motion::SkeletonDescriptor skeleton;
    openstrata::motion::RetargetMap map;
};

LevelRig
MakeLevelRig()
{
    std::vector<std::string> tokens;
    std::vector<pxr::GfMatrix4d> rests;
    const auto add = [&](std::string token, const pxr::GfVec3f& offset) {
        pxr::GfMatrix4d rest(1.0);
        rest.SetTranslateOnly(pxr::GfVec3d(offset));
        tokens.push_back(std::move(token));
        rests.push_back(rest);
        return static_cast<int>(tokens.size() - 1);
    };
    std::vector<std::pair<HumanJoint, int>> bound;
    bound.emplace_back(HumanJoint::Chest, add("Torso", {0.0f, 1.2f, 0.0f}));
    for (int s = 0; s < 2; ++s) {
        const bool left = s == 0;
        const float side = left ? 1.0f : -1.0f;
        const std::string p = left ? "Torso/L" : "Torso/R";
        bound.emplace_back(left ? HumanJoint::LeftShoulder : HumanJoint::RightShoulder,
                           add(p + "Clavicle", {side * 0.03f, 0.2f, 0.0f}));
        bound.emplace_back(left ? HumanJoint::LeftUpperArm : HumanJoint::RightUpperArm,
                           add(p + "Clavicle/Arm", {side * 0.12f, 0.0f, 0.0f}));
        bound.emplace_back(left ? HumanJoint::LeftLowerArm : HumanJoint::RightLowerArm,
                           add(p + "Clavicle/Arm/Forearm", {side * 0.3f, 0.0f, 0.0f}));
        bound.emplace_back(left ? HumanJoint::LeftHand : HumanJoint::RightHand,
                           add(p + "Clavicle/Arm/Forearm/Palm", {side * 0.3f, 0.0f, 0.0f}));
    }
    const auto built = openstrata::motion::BuildSkeletonDescriptor(tokens, rests);
    assert(built.skeleton);
    LevelRig rig;
    rig.skeleton = *built.skeleton;
    for (const auto& [role, joint] : bound) {
        assert(rig.map.SetJointIndex(role, joint, rig.skeleton.GetSize()));
    }
    return rig;
}

pxr::GfRotation
Raise()
{
    return pxr::GfRotation(pxr::GfVec3d(0.0, 0.0, 1.0), 20.0);
}

// Two samples: every role at its rest, then the left upper arm turned 20°
// about +Z in the source's own frame.
openstrata::motion::MotionClip
TwoSamples(const std::bitset<openstrata::motion::HumanJointCount>& roles)
{
    openstrata::motion::MotionClip clip;
    openstrata::motion::MotionPose pose;
    pose.validRotations = roles;
    clip.samples.push_back(pose);
    pose.timestamp = 1.0 / 30.0;
    const pxr::GfQuatd raise = Raise().GetQuat();
    pose.localRotations[static_cast<std::size_t>(HumanJoint::LeftUpperArm)] =
        pxr::GfQuatf(static_cast<float>(raise.GetReal()), pxr::GfVec3f(raise.GetImaginary()));
    clip.samples.push_back(pose);
    return clip;
}

// Where a source segment points in a sample: its rest direction, turned in the
// second sample for the left upper and lower arm.
pxr::GfVec3f
Expected(const pxr::GfVec3f& restDirection, const Segment& segment, std::size_t sample)
{
    const bool turned = sample == 1 && (segment.from == HumanJoint::LeftUpperArm ||
                                        segment.from == HumanJoint::LeftLowerArm);
    return turned ? pxr::GfVec3f(Raise().TransformDir(pxr::GfVec3d(restDirection))) : restDirection;
}

double
WorstSegment(const openstrata::motion::SkeletonDescriptor& skeleton,
             const openstrata::motion::RetargetMap& map,
             const openstrata::motion::RetargetedAnimation& animation,
             const std::vector<pxr::GfVec3f>& restDirections)
{
    double worst = 0.0;
    for (std::size_t sample = 0; sample < animation.samples.size(); ++sample) {
        for (std::size_t k = 0; k < std::size(kSegments); ++k) {
            pxr::GfQuatf qa, qb;
            pxr::GfVec3f pa, pb;
            const bool a = openstrata::motion::GetJointWorldTransform(
                skeleton, animation.samples[sample], map.GetJointIndex(kSegments[k].from), &qa, &pa);
            const bool b = openstrata::motion::GetJointWorldTransform(
                skeleton, animation.samples[sample], map.GetJointIndex(kSegments[k].to), &qb, &pb);
            assert(a && b);
            worst = std::max(worst,
                             AngleDegrees(pb - pa, Expected(restDirections[k], kSegments[k], sample)));
        }
    }
    return worst;
}

std::vector<pxr::GfVec3f>
RestDirections(const mmd::CanonicalDocument& model, const mmd::skeleton::AdaptedSkeleton& adapted)
{
    std::vector<pxr::GfVec3f> out;
    for (const Segment& segment : kSegments) {
        out.push_back(Position(model, adapted.SourceJoint(segment.to)) -
                      Position(model, adapted.SourceJoint(segment.from)));
    }
    return out;
}

void
TestRetargetBothWays()
{
    const ArmModel a = MakeArmModel(40.0f, 10.0f, 1.0f);
    const ArmModel b = MakeArmModel(30.0f, 5.0f, 1.2f);
    const mmd::skeleton::AdaptedSkeleton source = mmd::skeleton::Adapt(a.model);
    const mmd::skeleton::AdaptedSkeleton target = mmd::skeleton::Adapt(b.model);
    const LevelRig level = MakeLevelRig();
    const openstrata::motion::MotionClip mmdClip = TwoSamples(source.sourcePresent);
    const std::vector<pxr::GfVec3f> mmdDirections = RestDirections(a.model, source);

    openstrata::motion::RetargetOptions stated;
    stated.targetRest = target.targetRest;

    // An A-pose source onto level arms: the target hangs its arms as MMD does.
    const auto ontoLevel =
        openstrata::motion::PoseRetargeter(level.skeleton, level.map, source.sourceRest).Retarget(mmdClip);
    assert(WorstSegment(level.skeleton, level.map, ontoLevel, mmdDirections) < 0.01);

    // A-pose onto A-pose, both rests stated: exact although the angles differ.
    const auto ontoPmx =
        openstrata::motion::PoseRetargeter(target.skeleton, target.targetMap, source.sourceRest, stated)
            .Retarget(mmdClip);
    assert(WorstSegment(target.skeleton, target.targetMap, ontoPmx, mmdDirections) < 0.01);

    // Both or neither: without the target's rest, the arms are off by its own
    // 30° arm angle.
    const auto unstated =
        openstrata::motion::PoseRetargeter(target.skeleton, target.targetMap, source.sourceRest)
            .Retarget(mmdClip);
    assert(std::fabs(WorstSegment(target.skeleton, target.targetMap, unstated, mmdDirections) - 30.0) <
           0.5);

    // A level-arm source, its rest identity, onto the A-pose PMX: level arms.
    std::bitset<openstrata::motion::HumanJointCount> levelRoles;
    levelRoles.set(static_cast<std::size_t>(HumanJoint::Chest));
    std::vector<pxr::GfVec3f> lateral;
    for (const Segment& segment : kSegments) {
        levelRoles.set(static_cast<std::size_t>(segment.from));
        levelRoles.set(static_cast<std::size_t>(segment.to));
        lateral.emplace_back(segment.side, 0.0f, 0.0f);
    }
    const auto fromLevel =
        openstrata::motion::PoseRetargeter(target.skeleton, target.targetMap,
                                           openstrata::motion::SourceRestPose(), stated)
            .Retarget(TwoSamples(levelRoles));
    assert(WorstSegment(target.skeleton, target.targetMap, fromLevel, lateral) < 0.01);
}

} // namespace

int
main()
{
    TestRolesAndSkeleton();
    TestTargetHipsRequiresBothLegs();
    TestUpperBodyFollowsTheChain();
    TestArmChainReferenceRest();
    TestRetargetBothWays();
    return 0;
}
