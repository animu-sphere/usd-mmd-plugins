// SPDX-License-Identifier: Apache-2.0

#include <mmdMotionAdapter/Adapter.h>

#include <cassert>
#include <cmath>
#include <string>

namespace {

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

mmd::binding::BoneKey
Key(std::uint32_t frame, mmd::Float3 translation, mmd::Float4 rotation)
{
    mmd::binding::BoneKey key;
    key.frame = frame;
    key.translation = translation;
    key.rotation = rotation;
    return key;
}

void
TestClip()
{
    mmd::CanonicalDocument model;
    const int root = AddBone(model, "全ての親", "Root", mmd::kNone, {0.0, 0.0, 0.0});
    const int waist = AddBone(model, "腰", "Waist", root, {0.0, 1.0, 0.0});
    const int lower = AddBone(model, "下半身", "Lower", waist, {0.0, 1.1, 0.0});
    const int upper = AddBone(model, "上半身", "Upper", waist, {0.0, 1.4, 0.0});
    AddBone(model, "左足", "LeftLeg", waist, {0.1, 0.9, 0.0});
    AddBone(model, "右足", "RightLeg", waist, {-0.1, 0.9, 0.0});
    AddBone(model, "頭", "Head", upper, {0.0, 1.8, 0.0});
    model.rig.bones.resize(model.skeleton.bones.size());

    mmd::Morph smile;
    smile.name = {"笑い", "", "Smile"};
    smile.type = mmd::MorphType::Vertex;
    model.morphs.push_back(smile);

    mmd::binding::BoundMotion bound;
    bound.sourceModelName = "テストモデル";
    bound.bones.push_back({root,
                           {Key(0, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}),
                            Key(30, {2.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f})}});
    constexpr float s = 0.7071067811865476f;
    bound.bones.push_back({upper,
                           {Key(0, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}),
                            Key(30, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, s, s})}});
    bound.morphs.push_back({0, {{0, 0.25f}, {30, 0.75f}}});

    const mmd::skeleton::AdaptedSkeleton skeleton = mmd::skeleton::Adapt(model);
    const auto prepared = mmd::control::Evaluator::Prepare(model);
    assert(prepared);

    const auto result = mmd::motion::BuildClip(
        model, bound, prepared.value(), skeleton, mmd::motion::ClipOptions{0.0, 1.0, 1.0});
    assert(result);
    const openstrata::motion::MotionClip& clip = result.value();
    assert(clip.samples.size() == 2);
    assert(clip.startTime == 0.0 && clip.endTime == 1.0);
    assert(clip.nominalFrameRate == 1.0);
    assert(clip.source.protocol == "vmd");
    assert(clip.source.sourceId == "テストモデル");

    const openstrata::motion::MotionPose& first = clip.samples.front();
    const openstrata::motion::MotionPose& last = clip.samples.back();
    assert(first.timestamp == 0.0 && last.timestamp == 1.0);
    assert(first.root.hasPosition && first.root.hasOrientation);
    assert(std::abs(last.root.worldPosition[0] - 2.0f) < 1.0e-6f);
    assert(std::abs(last.root.worldPosition[1] - 1.1f) < 1.0e-6f);
    assert(
        last.validRotations.test(static_cast<std::size_t>(openstrata::motion::HumanJoint::Hips)));
    const auto spine = static_cast<std::size_t>(openstrata::motion::HumanJoint::Spine);
    assert(last.validRotations.test(spine));
    assert(std::abs(last.localRotations[spine].GetImaginary()[2] - s) < 1.0e-5f);
    assert(std::abs(*first.channels.Find("mmd:笑い") - 0.25f) < 1.0e-6f);
    assert(std::abs(*last.channels.Find("mmd:笑い") - 0.75f) < 1.0e-6f);

    bool missingRequired = false;
    for (const mmd::Diagnostic& diagnostic : result.diagnostics()) {
        missingRequired |= diagnostic.code == "MMD_MOTION_MISSING_REQUIRED_JOINT";
    }
    assert(missingRequired);
    assert(skeleton.SourceJoint(openstrata::motion::HumanJoint::Hips) == lower);
}

void
TestRangeValidation()
{
    const mmd::CanonicalDocument model;
    const mmd::binding::BoundMotion bound;
    const mmd::skeleton::AdaptedSkeleton skeleton = mmd::skeleton::Adapt(model);
    const auto prepared = mmd::control::Evaluator::Prepare(model);
    assert(prepared);
    const auto result = mmd::motion::BuildClip(
        model, bound, prepared.value(), skeleton, mmd::motion::ClipOptions{1.0, 0.0, 30.0});
    assert(!result);
    assert(result.fatal());
    assert(result.fatal()->code == "MMD_MOTION_INVALID_SAMPLE_RANGE");
}

} // namespace

int
main()
{
    TestClip();
    TestRangeValidation();
    return 0;
}
