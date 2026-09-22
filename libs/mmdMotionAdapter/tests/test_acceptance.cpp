// SPDX-License-Identifier: Apache-2.0
//
// Phase 9 acceptance, entirely synthetic and deterministic:
// VMD records -> binding -> MMD control evaluation (including leg IK) ->
// MotionClip -> motionUsd round trip -> motionRetarget -> a PMX-derived stage
// skeleton and a separately named, non-MMD skeleton.

#include "generic_retarget.h"

#include <mmdModel/Canonicalize.h>
#include <mmdMotionAdapter/Adapter.h>
#include <mmdMotionBinding/Bind.h>
#include <mmdPmx/Document.h>
#include <motionUsd/ClipReader.h>
#include <motionUsd/ClipWriter.h>
#include <motionVmd/Cp932.h>
#include <motionVmd/Motion.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/quatf.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec3h.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdSkel/animation.h>
#include <pxr/usd/usdSkel/bindingAPI.h>
#include <pxr/usd/usdSkel/root.h>
#include <pxr/usd/usdSkel/skeleton.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <numbers>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace pmx = mmd::pmx;
namespace vmd = motionVmd;
using J = openstrata::motion::HumanJoint;

pmx::Bone
Bone(std::string name, pmx::Vec3 position, std::int32_t parent, std::uint16_t flags = 0)
{
    pmx::Bone bone;
    bone.name = std::move(name);
    bone.position = position;
    bone.parent = parent;
    bone.flags = flags;
    return bone;
}

pmx::Document
Humanoid(bool withIk, float scale)
{
    const auto p = [scale](float x, float y, float z = 0.0f) {
        return pmx::Vec3{x * scale, y * scale, z * scale};
    };
    pmx::Document doc;
    doc.model.name = withIk ? "synthetic VMD source" : "synthetic PMX target";
    doc.bones = {
        Bone("全ての親", p(0, 0), pmx::kNoIndex),       // 0
        Bone("腰", p(0, 0), 0),                        // 1
        Bone("下半身", p(0, 12.5f), 1),                // 2
        Bone("上半身", p(0, 15.0f), 1),                // 3
        Bone("首", p(0, 20.0f), 3),                    // 4
        Bone("頭", p(0, 22.5f), 4),                    // 5
        Bone("左足", p(2.0f, 12.5f), 2),               // 6
        Bone("左ひざ", p(2.0f, 6.25f), 6),             // 7
        Bone("左足首", p(2.0f, 0), 7),                 // 8
        Bone("右足", p(-2.0f, 12.5f), 2),              // 9
        Bone("右ひざ", p(-2.0f, 6.25f), 9),            // 10
        Bone("右足首", p(-2.0f, 0), 10),               // 11
        Bone("左腕", p(3.0f, 18.0f), 3),               // 12
        Bone("左ひじ", p(6.0f, 18.0f), 12),            // 13
        Bone("左手首", p(9.0f, 18.0f), 13),            // 14
        Bone("右腕", p(-3.0f, 18.0f), 3),              // 15
        Bone("右ひじ", p(-6.0f, 18.0f), 15),           // 16
        Bone("右手首", p(-9.0f, 18.0f), 16),           // 17
    };
    if (withIk) {
        doc.bones.push_back(Bone("左足ＩＫ", p(2.0f, 0), 0, pmx::BoneFlag::Ik));
        pmx::Ik& ik = doc.bones.back().ik;
        ik.target = 8;
        ik.loopCount = 40;
        ik.limitAngle = 2.0f;
        ik.links = {
            pmx::IkLink{7,
                        true,
                        {static_cast<float>(-std::numbers::pi), 0.0f, 0.0f},
                        {-0.00872665f, 0.0f, 0.0f}},
            pmx::IkLink{6, false, {}, {}},
        };
    }
    return doc;
}

mmd::CanonicalDocument
Canonical(const pmx::Document& document)
{
    auto result = mmd::Canonicalize(document);
    assert(result);
    return std::move(result).value();
}

vmd::Name
Name(const std::string& utf8, std::size_t width)
{
    std::string bytes = vmd::EncodeCp932(utf8).value();
    bytes.resize(std::min(bytes.size(), width));
    const auto decoded = vmd::DecodeCp932(
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()));
    return {bytes, decoded.text};
}

vmd::Document
IkMotion()
{
    vmd::Document document;
    document.header.modelName = Name("synthetic source", 20);
    vmd::BoneKeyframe goal;
    goal.bone = Name("左足ＩＫ", 15);
    // Relative to the IK bone: +0.2 m Y and +0.05 m Z after basis conversion.
    goal.translation = {0.0f, 2.5f, -0.625f};
    goal.interpolation.fill(20);
    document.boneKeyframes.push_back(goal);

    vmd::IkKeyframe enabled;
    enabled.visible = 1;
    enabled.ik.push_back({Name("左足ＩＫ", 20), 1});
    document.ikKeyframes.push_back(enabled);
    document.sectionsPresent = vmd::kSectionCount;
    return document;
}

bool
IsIdentity(const pxr::GfQuatf& rotation)
{
    return std::fabs(pxr::GfDot(rotation.GetNormalized(), pxr::GfQuatf(1.0f))) > 1.0f - 1.0e-5f;
}

openstrata::motion::MotionClip
RoundTrip(const openstrata::motion::MotionClip& clip,
          const mmd::skeleton::AdaptedSkeleton& source)
{
    openstrata::motion::MotionStageRest rest;
    rest.localRotations = source.sourceRest.localRotations;
    rest.localTranslations = source.sourceRest.localTranslations;
    rest.present = source.sourcePresent;

    openstrata::motion::MotionStageOptions options;
    options.sourceFormat = "vmd";
    options.rootMotionSource = "mapped hips world transform";
    options.rest = rest;
    const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
    openstrata::motion::MotionStageReport report;
    std::string error;
    assert(openstrata::motion::AuthorMotionStage(stage, clip, options, &report, &error));
    assert(report.sampleCount == clip.samples.size());

    openstrata::motion::MotionStageRead read;
    assert(openstrata::motion::ReadMotionStage(stage, {}, &read, &error));
    assert(read.warnings.empty());
    assert(read.metadata.sourceFormat == "vmd");
    assert(read.clip.samples.size() == clip.samples.size());
    return std::move(read.clip);
}

pxr::UsdStageRefPtr
PmxSkeletonStage(const mmd::skeleton::AdaptedSkeleton& target)
{
    const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
    pxr::UsdSkelRoot::Define(stage, pxr::SdfPath("/Asset"));
    const pxr::UsdSkelSkeleton skeleton =
        pxr::UsdSkelSkeleton::Define(stage, pxr::SdfPath("/Asset/skel/Skeleton"));
    pxr::VtTokenArray tokens;
    pxr::VtMatrix4dArray rests;
    for (const auto& joint : target.skeleton.GetJoints()) {
        tokens.push_back(pxr::TfToken(joint.token));
        pxr::GfMatrix4d matrix(1.0);
        matrix.SetTranslateOnly(pxr::GfVec3d(joint.restTranslation));
        rests.push_back(matrix);
    }
    skeleton.CreateJointsAttr().Set(tokens);
    skeleton.CreateRestTransformsAttr().Set(rests);
    return stage;
}

openstrata::motion::SkeletonDescriptor
ReadTargetSkeleton(const pxr::UsdStageRefPtr& stage)
{
    const pxr::UsdSkelSkeleton skeleton(
        stage->GetPrimAtPath(pxr::SdfPath("/Asset/skel/Skeleton")));
    pxr::VtTokenArray tokens;
    pxr::VtMatrix4dArray rests;
    assert(skeleton.GetJointsAttr().Get(&tokens));
    assert(skeleton.GetRestTransformsAttr().Get(&rests));
    std::vector<std::string> names;
    names.reserve(tokens.size());
    for (const auto& token : tokens) {
        names.push_back(token.GetString());
    }
    const auto built = openstrata::motion::BuildSkeletonDescriptor(
        names, std::vector<pxr::GfMatrix4d>(rests.begin(), rests.end()));
    assert(built.skeleton);
    return *built.skeleton;
}

void
BindAnimation(const pxr::UsdStageRefPtr& stage,
              const openstrata::motion::SkeletonDescriptor& skeleton,
              const openstrata::motion::RetargetedAnimation& animation)
{
    const pxr::SdfPath path("/Asset/skel/RetargetedAnimation");
    const pxr::UsdSkelAnimation authored = pxr::UsdSkelAnimation::Define(stage, path);
    pxr::VtTokenArray joints;
    for (const std::string& joint : animation.joints) {
        joints.push_back(pxr::TfToken(joint));
    }
    authored.CreateJointsAttr().Set(joints);
    pxr::VtVec3hArray scales;
    for (const auto& joint : skeleton.GetJoints()) {
        scales.push_back(pxr::GfVec3h(joint.restScale));
    }
    authored.CreateScalesAttr().Set(scales);
    for (const auto& sample : animation.samples) {
        authored.CreateRotationsAttr().Set(
            pxr::VtQuatfArray(sample.rotations.begin(), sample.rotations.end()),
            pxr::UsdTimeCode(sample.timestamp * 30.0));
        authored.CreateTranslationsAttr().Set(
            pxr::VtVec3fArray(sample.translations.begin(), sample.translations.end()),
            pxr::UsdTimeCode(sample.timestamp * 30.0));
    }
    const pxr::UsdSkelSkeleton target(
        stage->GetPrimAtPath(pxr::SdfPath("/Asset/skel/Skeleton")));
    const pxr::UsdSkelBindingAPI binding = pxr::UsdSkelBindingAPI::Apply(target.GetPrim());
    assert(binding.CreateAnimationSourceRel().SetTargets({path}));
}

struct GenericRig {
    openstrata::motion::SkeletonDescriptor skeleton;
    openstrata::motion::RetargetMap map;
    std::vector<J> required;
};

GenericRig
MakeGenericRig()
{
    GenericRig rig;
    const auto add = [&](J role, std::string token, pxr::GfVec3f rest) {
        openstrata::motion::SkeletonJoint joint;
        joint.token = std::move(token);
        joint.restTranslation = rest;
        rig.skeleton.AddJoint(joint);
        rig.required.push_back(role);
    };
    add(J::Hips, "World/Pelvis", {0.0f, 1.1f, 0.0f});
    add(J::Spine, "World/Pelvis/Torso", {0.0f, 0.35f, 0.0f});
    add(J::Neck, "World/Pelvis/Torso/NeckBase", {0.0f, 0.35f, 0.0f});
    add(J::Head, "World/Pelvis/Torso/NeckBase/Cranium", {0.0f, 0.2f, 0.0f});
    add(J::LeftUpperLeg, "World/Pelvis/Leg_L", {0.14f, -0.08f, 0.0f});
    add(J::LeftLowerLeg, "World/Pelvis/Leg_L/Shin_L", {0.0f, -0.55f, 0.0f});
    add(J::LeftFoot, "World/Pelvis/Leg_L/Shin_L/Foot_L", {0.0f, -0.5f, 0.0f});
    add(J::RightUpperLeg, "World/Pelvis/Leg_R", {-0.14f, -0.08f, 0.0f});
    add(J::RightLowerLeg, "World/Pelvis/Leg_R/Shin_R", {0.0f, -0.55f, 0.0f});
    add(J::RightFoot, "World/Pelvis/Leg_R/Shin_R/Foot_R", {0.0f, -0.5f, 0.0f});
    add(J::LeftUpperArm, "World/Pelvis/Torso/Arm_L", {0.25f, 0.3f, 0.0f});
    add(J::LeftLowerArm, "World/Pelvis/Torso/Arm_L/Forearm_L", {0.35f, 0.0f, 0.0f});
    add(J::LeftHand, "World/Pelvis/Torso/Arm_L/Forearm_L/Palm_L", {0.3f, 0.0f, 0.0f});
    add(J::RightUpperArm, "World/Pelvis/Torso/Arm_R", {-0.25f, 0.3f, 0.0f});
    add(J::RightLowerArm, "World/Pelvis/Torso/Arm_R/Forearm_R", {-0.35f, 0.0f, 0.0f});
    add(J::RightHand, "World/Pelvis/Torso/Arm_R/Forearm_R/Palm_R", {-0.3f, 0.0f, 0.0f});
    rig.skeleton.ResolveParentsFromTokens();
    for (std::size_t i = 0; i < rig.required.size(); ++i) {
        assert(rig.map.SetJointIndex(rig.required[i], static_cast<int>(i), rig.skeleton.GetSize()));
    }
    return rig;
}

void
TestEndToEnd()
{
    const mmd::CanonicalDocument sourceModel = Canonical(Humanoid(true, 1.0f));
    const auto motion = vmd::BuildMotion(IkMotion());
    assert(motion);
    const auto bound = mmd::binding::Bind(motion.value(), sourceModel);
    assert(bound && bound.value().bones.size() == 1 && bound.value().ik.size() == 1);
    const auto evaluator = mmd::control::Evaluator::Prepare(sourceModel);
    assert(evaluator);
    const mmd::skeleton::AdaptedSkeleton source = mmd::skeleton::Adapt(sourceModel);
    const auto built = mmd::motion::BuildClip(
        sourceModel, bound.value(), evaluator.value(), source,
        mmd::motion::ClipOptions{0.0, 1.0 / 30.0, 30.0});
    assert(built && built.diagnostics().empty());
    assert(built.value().samples.size() == 2);

    const std::size_t leftLeg = static_cast<std::size_t>(J::LeftUpperLeg);
    const std::size_t leftKnee = static_cast<std::size_t>(J::LeftLowerLeg);
    assert(built.value().samples[0].validRotations.test(leftLeg));
    assert(built.value().samples[0].validRotations.test(leftKnee));
    assert(!IsIdentity(built.value().samples[0].localRotations[leftLeg]));
    assert(!IsIdentity(built.value().samples[0].localRotations[leftKnee]));

    const openstrata::motion::MotionClip clip = RoundTrip(built.value(), source);

    const mmd::CanonicalDocument targetModel = Canonical(Humanoid(false, 1.2f));
    const mmd::skeleton::AdaptedSkeleton target = mmd::skeleton::Adapt(targetModel);
    const pxr::UsdStageRefPtr targetStage = PmxSkeletonStage(target);
    const openstrata::motion::SkeletonDescriptor stageSkeleton = ReadTargetSkeleton(targetStage);
    assert(stageSkeleton == target.skeleton);

    openstrata::motion::RetargetDiagnostics pmxDiagnostics;
    const openstrata::motion::RetargetedAnimation pmxAnimation = RetargetGeneric(
        clip, stageSkeleton, target.targetMap, source.sourceRest,
        target.requiredJoints, &pmxDiagnostics);
    assert(pmxDiagnostics.IsClean());
    assert(pmxAnimation.samples.size() == clip.samples.size());
    assert(pmxAnimation.joints.size() == stageSkeleton.GetSize());
    const int pmxLeg = target.targetMap.GetJointIndex(J::LeftUpperLeg);
    const int pmxKnee = target.targetMap.GetJointIndex(J::LeftLowerLeg);
    assert(pmxLeg >= 0 && pmxKnee >= 0);
    assert(!IsIdentity(pmxAnimation.samples[0].rotations[static_cast<std::size_t>(pmxLeg)]));
    assert(!IsIdentity(pmxAnimation.samples[0].rotations[static_cast<std::size_t>(pmxKnee)]));
    BindAnimation(targetStage, stageSkeleton, pmxAnimation);
    std::vector<pxr::SdfPath> targets;
    const pxr::UsdSkelBindingAPI binding(
        targetStage->GetPrimAtPath(pxr::SdfPath("/Asset/skel/Skeleton")));
    assert(binding.GetAnimationSourceRel().GetTargets(&targets));
    assert(targets == std::vector<pxr::SdfPath>{pxr::SdfPath("/Asset/skel/RetargetedAnimation")});

    const GenericRig generic = MakeGenericRig();
    openstrata::motion::RetargetDiagnostics genericDiagnostics;
    const openstrata::motion::RetargetedAnimation genericAnimation = RetargetGeneric(
        clip, generic.skeleton, generic.map, source.sourceRest,
        generic.required, &genericDiagnostics);
    assert(genericDiagnostics.IsClean());
    assert(genericAnimation.samples.size() == clip.samples.size());
    assert(genericAnimation.joints.front() == "World/Pelvis");
    const int genericLeg = generic.map.GetJointIndex(J::LeftUpperLeg);
    assert(!IsIdentity(
        genericAnimation.samples[0].rotations[static_cast<std::size_t>(genericLeg)]));

    std::puts("ok  VMD + MMD IK -> motionUsd -> PMX and generic retarget targets");
}

} // namespace

int
main()
{
    TestEndToEnd();
    return 0;
}
