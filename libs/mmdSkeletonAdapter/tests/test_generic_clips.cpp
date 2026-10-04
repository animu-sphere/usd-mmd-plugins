// SPDX-License-Identifier: Apache-2.0
//
// Phase 8: generic motion onto a PMX. A semantic motion stage written the way
// a format repository writes one -- `usd-vrm-plugins`' `.vrma` stage,
// `motion_convert`'s BVH clip: standard UsdSkel over the shared joint tokens,
// with no `customData.motion` -- is read by `motionUsd` and retargeted onto a
// PMX skeleton through this adapter's map and target rest. Nothing here
// evaluates a VMD. Ground truth is UsdSkel's own evaluation of the source
// stage, so neither side of the comparison is computed by the code under test.

#include <mmdSkeletonAdapter/Adapter.h>

#include <motionRetarget/Diagnostics.h>
#include <motionRetarget/PoseRetargeter.h>
#include <motionRetarget/SkeletonDescriptor.h>
#include <motionUsd/ClipReader.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/quatf.h>
#include <pxr/base/gf/rotation.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdSkel/animation.h>
#include <pxr/usd/usdSkel/bindingAPI.h>
#include <pxr/usd/usdSkel/skeleton.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

using openstrata::motion::HumanJoint;

// ---------------------------------------------------------------- the source

pxr::GfQuatf
Quat(const pxr::GfRotation& rotation)
{
    const pxr::GfQuatd q = rotation.GetQuat();
    return pxr::GfQuatf(static_cast<float>(q.GetReal()), pxr::GfVec3f(q.GetImaginary()));
}

pxr::GfQuatf
About(const pxr::GfVec3d& axis, double degrees)
{
    return Quat(pxr::GfRotation(axis, degrees));
}

const pxr::GfVec3d kX(1.0, 0.0, 0.0), kY(0.0, 1.0, 0.0), kZ(0.0, 0.0, 1.0);

// One joint of the source: its token, where it rests in skeleton space, and
// the world rotation its rest states. Parents precede children.
struct SourceJoint {
    std::string token;
    pxr::GfVec3f position;
    pxr::GfQuatf worldRest;
};

std::string
Parent(const std::string& token)
{
    const std::size_t slash = token.rfind('/');
    return slash == std::string::npos ? std::string() : token.substr(0, slash);
}

// A level-arm humanoid. With `tilted`, the rests state rotations a normalized
// rig would not -- the hips' a 120° turn about (1, 1, 1), the upper chest and
// the neck rolled -- while every joint stays where it was, as one distributed
// VRMA's skeleton does. A reader that dropped a rest rotation, or a retarget
// that ignored one, would show here and not in the identity case.
std::vector<SourceJoint>
SourceJoints(bool tilted)
{
    const pxr::GfQuatf one(1.0f);
    const pxr::GfQuatf turned = tilted ? About(pxr::GfVec3d(1.0, 1.0, 1.0), 120.0) : one;
    const pxr::GfQuatf rolled = tilted ? About(kZ, -35.0) * turned : one;
    const pxr::GfQuatf neck = tilted ? About(kZ, 47.0) : one;
    std::vector<SourceJoint> joints = {
        {"hips", {0.0f, 0.9f, 0.0f}, turned},
        {"hips/spine", {0.0f, 1.0f, 0.0f}, turned},
        {"hips/spine/chest", {0.0f, 1.1f, 0.0f}, turned},
        {"hips/spine/chest/upperChest", {0.0f, 1.2f, 0.0f}, rolled},
        {"hips/spine/chest/upperChest/neck", {0.0f, 1.35f, 0.0f}, neck},
        {"hips/spine/chest/upperChest/neck/head", {0.0f, 1.45f, 0.0f}, neck},
    };
    for (const float side : {1.0f, -1.0f}) {
        const std::string s = side > 0.0f ? "left" : "right";
        const std::string arm = "hips/spine/chest/upperChest/" + s + "Shoulder";
        joints.push_back({arm, {side * 0.02f, 1.35f, 0.0f}, rolled});
        joints.push_back({arm + "/" + s + "UpperArm", {side * 0.12f, 1.35f, 0.0f}, rolled});
        joints.push_back({arm + "/" + s + "UpperArm/" + s + "LowerArm",
                          {side * 0.37f, 1.35f, 0.0f}, one});
        joints.push_back({arm + "/" + s + "UpperArm/" + s + "LowerArm/" + s + "Hand",
                          {side * 0.62f, 1.35f, 0.0f}, one});
        const std::string leg = "hips/" + s + "UpperLeg";
        joints.push_back({leg, {side * 0.1f, 0.85f, 0.0f}, turned});
        joints.push_back({leg + "/" + s + "LowerLeg", {side * 0.1f, 0.45f, 0.0f}, one});
        joints.push_back({leg + "/" + s + "LowerLeg/" + s + "Foot", {side * 0.1f, 0.05f, 0.0f}, one});
    }
    return joints;
}

// Two samples: the rest, then each listed joint turned by its own rotation in
// its own frame -- the hips yawed, the upper chest rolled, the neck nodded and
// the left upper arm raised.
const std::map<std::string, pxr::GfQuatf>&
Motion()
{
    static const std::map<std::string, pxr::GfQuatf> motion = {
        {"hips", About(kY, 30.0)},
        {"upperChest", About(kZ, 15.0)},
        {"neck", About(kX, 10.0)},
        {"leftUpperArm", About(kZ, 20.0)},
    };
    return motion;
}

std::string
Leaf(const std::string& token)
{
    const std::size_t slash = token.rfind('/');
    return slash == std::string::npos ? token : token.substr(slash + 1);
}

// The stage `usdVrmaFileFormat` and `motion_convert` write, reduced to what
// the read needs: `/Animation/HumanoidSkeleton` binding one `UsdSkelAnimation`.
pxr::UsdStageRefPtr
SourceStage(const std::vector<SourceJoint>& joints)
{
    const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
    stage->SetTimeCodesPerSecond(30.0);
    stage->SetStartTimeCode(0.0);
    stage->SetEndTimeCode(1.0);
    stage->DefinePrim(pxr::SdfPath("/Animation"), pxr::TfToken("Scope"));

    std::map<std::string, std::size_t> index;
    pxr::VtTokenArray tokens;
    pxr::VtMatrix4dArray rests;
    std::vector<pxr::GfQuatf> localRests;
    pxr::VtVec3fArray translations;
    for (std::size_t i = 0; i < joints.size(); ++i) {
        const SourceJoint& joint = joints[i];
        index[joint.token] = i;
        const std::string parent = Parent(joint.token);
        pxr::GfQuatf parentRest(1.0f);
        pxr::GfVec3f offset = joint.position;
        if (!parent.empty()) {
            const SourceJoint& p = joints[index.at(parent)];
            parentRest = p.worldRest;
            offset = joint.position - p.position;
        }
        const pxr::GfQuatf local = (parentRest.GetInverse() * joint.worldRest).GetNormalized();
        const pxr::GfVec3f translation = parentRest.GetInverse().Transform(offset);
        pxr::GfMatrix4d rest;
        rest.SetTransform(pxr::GfRotation(pxr::GfQuatd(local)), pxr::GfVec3d(translation));
        tokens.push_back(pxr::TfToken(joint.token));
        rests.push_back(rest);
        localRests.push_back(local);
        translations.push_back(translation);
    }

    const pxr::UsdSkelSkeleton skeleton =
        pxr::UsdSkelSkeleton::Define(stage, pxr::SdfPath("/Animation/HumanoidSkeleton"));
    skeleton.CreateJointsAttr().Set(tokens);
    skeleton.CreateRestTransformsAttr().Set(rests);
    skeleton.CreateBindTransformsAttr().Set(rests);

    const pxr::UsdSkelAnimation animation =
        pxr::UsdSkelAnimation::Define(stage, pxr::SdfPath("/Animation/BodyAnimation"));
    animation.CreateJointsAttr().Set(tokens);
    for (const double time : {0.0, 1.0}) {
        pxr::VtQuatfArray rotations;
        for (std::size_t i = 0; i < joints.size(); ++i) {
            const auto turn = Motion().find(Leaf(joints[i].token));
            rotations.push_back(time > 0.0 && turn != Motion().end()
                                    ? (localRests[i] * turn->second).GetNormalized()
                                    : localRests[i]);
        }
        animation.CreateRotationsAttr().Set(rotations, pxr::UsdTimeCode(time));
        animation.CreateTranslationsAttr().Set(translations, pxr::UsdTimeCode(time));
    }
    pxr::UsdSkelBindingAPI::Apply(skeleton.GetPrim())
        .CreateAnimationSourceRel()
        .SetTargets({animation.GetPath()});
    return stage;
}

// Where the source stage puts each joint at `time`, by leaf: the animation's
// own values read back from the stage, composed down the joint paths with
// Gf alone. A `UsdSkelSkeletonQuery` would not do: with no `SkelRoot` above
// the skeleton, as these stages have none, it answers the rest.
std::map<std::string, pxr::GfVec3f>
SourcePositions(const pxr::UsdStageRefPtr& stage, double time)
{
    const pxr::UsdSkelAnimation animation =
        pxr::UsdSkelAnimation::Get(stage, pxr::SdfPath("/Animation/BodyAnimation"));
    pxr::VtTokenArray tokens;
    pxr::VtQuatfArray rotations;
    pxr::VtVec3fArray translations;
    animation.GetJointsAttr().Get(&tokens);
    animation.GetRotationsAttr().Get(&rotations, pxr::UsdTimeCode(time));
    animation.GetTranslationsAttr().Get(&translations, pxr::UsdTimeCode(time));
    assert(rotations.size() == tokens.size() && translations.size() == tokens.size());

    std::map<std::string, pxr::GfMatrix4d> world;
    std::map<std::string, pxr::GfVec3f> out;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const std::string token = tokens[i].GetString();
        pxr::GfMatrix4d local;
        local.SetTransform(pxr::GfRotation(pxr::GfQuatd(rotations[i])), pxr::GfVec3d(translations[i]));
        const std::string parent = Parent(token);
        // Row vectors: the parent's transform applies after the child's.
        world[token] = parent.empty() ? local : local * world.at(parent);
        out[Leaf(token)] = pxr::GfVec3f(world[token].ExtractTranslation());
    }
    return out;
}

// ---------------------------------------------------------------- the target

int
AddBone(mmd::CanonicalDocument& model, std::string source, std::string stable, int parent,
        mmd::Double3 position)
{
    mmd::Bone bone;
    bone.name = {std::move(source), "", stable};
    bone.sourceIndex = model.skeleton.bones.size();
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

mmd::Double3
Hang(const mmd::Double3& from, double side, double degrees, double length)
{
    const double radians = degrees * 3.14159265358979323846 / 180.0;
    return {from[0] + side * std::cos(radians) * length, from[1] - std::sin(radians) * length, from[2]};
}

// A PMX humanoid whose arms hang 40° below level and whose shoulders 10°, so
// only the target rest makes it level-armed. With `upperChest3`, `上半身3`
// sits above `上半身2`, and `upperChest` binds; without it, as 15 of the 17
// local characters are, it does not.
mmd::CanonicalDocument
Pmx(bool upperChest3)
{
    mmd::CanonicalDocument model;
    const int root = AddBone(model, "全ての親", "Root", mmd::kNone, {0.0, 0.0, 0.0});
    const int waist = AddBone(model, "腰", "Waist", root, {0.0, 0.9, 0.0});
    const int lower = AddBone(model, "下半身", "Lower", waist, {0.0, 0.95, 0.0});
    const int spine = AddBone(model, "上半身", "Spine", waist, {0.0, 1.0, 0.0});
    int top = AddBone(model, "上半身2", "Spine2", spine, {0.0, 1.1, 0.0});
    if (upperChest3) {
        top = AddBone(model, "上半身3", "Spine3", top, {0.0, 1.2, 0.0});
    }
    const int neck = AddBone(model, "首", "Neck", top, {0.0, 1.35, 0.0});
    AddBone(model, "頭", "Head", neck, {0.0, 1.45, 0.0});
    for (const double side : {1.0, -1.0}) {
        const std::string jp = side > 0.0 ? "左" : "右";
        const std::string en = side > 0.0 ? "Left" : "Right";
        const mmd::Double3 shoulderAt{side * 0.02, 1.35, 0.0};
        const mmd::Double3 upperAt = Hang(shoulderAt, side, 10.0, 0.1);
        const mmd::Double3 lowerAt = Hang(upperAt, side, 40.0, 0.25);
        const mmd::Double3 handAt = Hang(lowerAt, side, 40.0, 0.25);
        const int shoulder = AddBone(model, jp + "肩", en + "Shoulder", top, shoulderAt);
        const int upper = AddBone(model, jp + "腕", en + "Arm", shoulder, upperAt);
        const int elbow = AddBone(model, jp + "ひじ", en + "Elbow", upper, lowerAt);
        AddBone(model, jp + "手首", en + "Wrist", elbow, handAt);
        const int leg = AddBone(model, jp + "足", en + "Leg", lower, {side * 0.1, 0.85, 0.0});
        const int knee = AddBone(model, jp + "ひざ", en + "Knee", leg, {side * 0.1, 0.45, 0.0});
        AddBone(model, jp + "足首", en + "Ankle", knee, {side * 0.1, 0.05, 0.0});
    }
    return model;
}

// ------------------------------------------------------------ the comparison

double
AngleDegrees(const pxr::GfVec3f& a, const pxr::GfVec3f& b)
{
    const pxr::GfVec3d x(a), y(b);
    return std::atan2(pxr::GfCross(x, y).GetLength(), pxr::GfDot(x, y)) * 180.0 /
           3.14159265358979323846;
}

struct Segment {
    HumanJoint from;
    HumanJoint to;
    const char* fromLeaf;
    const char* toLeaf;
};

// Each segment downstream of the upper chest, and the legs, which are not.
const Segment kSegments[] = {
    {HumanJoint::Neck, HumanJoint::Head, "neck", "head"},
    {HumanJoint::LeftShoulder, HumanJoint::LeftUpperArm, "leftShoulder", "leftUpperArm"},
    {HumanJoint::LeftUpperArm, HumanJoint::LeftLowerArm, "leftUpperArm", "leftLowerArm"},
    {HumanJoint::LeftLowerArm, HumanJoint::LeftHand, "leftLowerArm", "leftHand"},
    {HumanJoint::RightShoulder, HumanJoint::RightUpperArm, "rightShoulder", "rightUpperArm"},
    {HumanJoint::RightUpperArm, HumanJoint::RightLowerArm, "rightUpperArm", "rightLowerArm"},
    {HumanJoint::RightLowerArm, HumanJoint::RightHand, "rightLowerArm", "rightHand"},
    {HumanJoint::LeftUpperLeg, HumanJoint::LeftLowerLeg, "leftUpperLeg", "leftLowerLeg"},
    {HumanJoint::LeftLowerLeg, HumanJoint::LeftFoot, "leftLowerLeg", "leftFoot"},
};

struct Result {
    // The worst segment angle against UsdSkel's evaluation, per sample.
    std::vector<double> worst;
    openstrata::motion::RetargetDiagnostics diagnostics;
};

// What a consumer does with a generic clip and a PMX: read the stage, take the
// rest its skeleton states, and retarget with the adapter's map and target rest,
// folding an unbound intermediate's rotation into its bound ancestor when
// `fold` (MOTION_CONTRACT.md §10.9).
Result
RetargetOnto(const pxr::UsdStageRefPtr& stage, const mmd::CanonicalDocument& model, bool fold)
{
    openstrata::motion::MotionStageRead read;
    std::string error;
    const bool ok = openstrata::motion::ReadMotionStage(stage, {}, &read, &error);
    assert(ok && error.empty());
    assert(read.warnings.empty());
    assert(read.clip.samples.size() == 2);
    // A foreign stage: it claims no motionUsd contract, and that is not a defect.
    assert(!read.metadata.contractVersion.has_value());

    const auto source = openstrata::motion::BuildSkeletonDescriptor(read.skeleton.jointTokens,
                                                                    read.skeleton.restTransforms);
    assert(source.skeleton);
    const auto sourceRest = openstrata::motion::BuildSourceRestPose(*source.skeleton);
    assert(sourceRest.rest);

    const mmd::skeleton::AdaptedSkeleton target = mmd::skeleton::Adapt(model);
    openstrata::motion::RetargetOptions options;
    options.requiredBones = target.requiredJoints;
    options.targetRest = target.targetRest;
    options.foldUnboundIntermediateRotations = fold;
    Result result;
    const openstrata::motion::RetargetedAnimation animation =
        openstrata::motion::PoseRetargeter(target.skeleton, target.targetMap, *sourceRest.rest, options)
            .Retarget(read.clip, &result.diagnostics);
    assert(animation.samples.size() == 2);

    for (std::size_t sample = 0; sample < 2; ++sample) {
        const auto truth = SourcePositions(stage, static_cast<double>(sample));
        double worst = 0.0;
        for (const Segment& segment : kSegments) {
            pxr::GfQuatf qa, qb;
            pxr::GfVec3f pa, pb;
            const bool a = openstrata::motion::GetJointWorldTransform(
                target.skeleton, animation.samples[sample],
                target.targetMap.GetJointIndex(segment.from), &qa, &pa);
            const bool b = openstrata::motion::GetJointWorldTransform(
                target.skeleton, animation.samples[sample],
                target.targetMap.GetJointIndex(segment.to), &qb, &pb);
            assert(a && b);
            worst = std::max(worst, AngleDegrees(pb - pa, truth.at(segment.toLeaf) -
                                                              truth.at(segment.fromLeaf)));
        }
        result.worst.push_back(worst);
    }
    return result;
}

bool
OnlyUpperChestUnbound(const openstrata::motion::RetargetDiagnostics& diagnostics)
{
    using Code = openstrata::motion::RetargetDiagnosticCode;
    return diagnostics.reported.size() == 1 &&
           diagnostics.Subjects(Code::UnboundDrivenBone) == std::vector<std::string>{"upperChest"};
}

// Onto a PMX with `上半身3`, every segment points where UsdSkel puts the
// source's, at rest and in motion, whatever rotations the source's rests state.
// Every joint the clip drives is bound, so folding changes nothing.
void
TestOntoPmxWithUpperChest()
{
    const mmd::CanonicalDocument model = Pmx(true);
    for (const bool tilted : {false, true}) {
        for (const bool fold : {false, true}) {
            const Result result = RetargetOnto(SourceStage(SourceJoints(tilted)), model, fold);
            assert(result.diagnostics.IsClean());
            assert(result.worst[0] < 0.01);
            assert(result.worst[1] < 0.01);
        }
    }
}

// MOT-O13: onto a PMX without `上半身3`, the shared retarget's default drops
// the upper chest's motion (RETARGETING_POLICY.md §4.1, case 6) and says so.
// At rest nothing is lost; in motion, everything above the chest misses the
// upper chest's 15° -- all of it with identity rests, and less where a
// tilted rest turns that rotation's axis toward a segment. This is why §10.9
// has a consumer opt in to the fold.
void
TestOntoPmxWithoutUpperChestByDefault()
{
    const mmd::CanonicalDocument model = Pmx(false);
    for (const bool tilted : {false, true}) {
        const Result result = RetargetOnto(SourceStage(SourceJoints(tilted)), model, false);
        assert(OnlyUpperChestUnbound(result.diagnostics));
        assert(result.worst[0] < 0.01);
        assert(result.worst[1] > 5.0 && result.worst[1] < 15.01);
    }
}

// With the fold, the upper chest's rotation, its rolled rest removed, reaches
// `上半身2`, and every segment above it turns exactly as the source's does.
// The dropped joint is still reported: the PMX does not have it.
void
TestOntoPmxWithoutUpperChestFolded()
{
    const mmd::CanonicalDocument model = Pmx(false);
    for (const bool tilted : {false, true}) {
        const Result result = RetargetOnto(SourceStage(SourceJoints(tilted)), model, true);
        assert(OnlyUpperChestUnbound(result.diagnostics));
        assert(result.worst[0] < 0.01);
        assert(result.worst[1] < 0.01);
    }
}

} // namespace

int
main()
{
    TestOntoPmxWithUpperChest();
    TestOntoPmxWithoutUpperChestByDefault();
    TestOntoPmxWithoutUpperChestFolded();
    return 0;
}
