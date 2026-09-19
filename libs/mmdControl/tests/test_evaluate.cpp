// SPDX-License-Identifier: Apache-2.0
//
// Evaluation over synthetic rigs with known answers (MOTION_CONTRACT.md
// §11): each model is a PMX document stated as data and canonicalized, and
// each motion is bound data in the USD basis -- except the last test, which
// goes through a VMD document and Bind, as a runtime would. One PMX unit is
// 0.08 m, so positions are written in multiples of 12.5: 12.5 units is 1 m.
#include "mmdControl/Codes.h"
#include "mmdControl/Evaluator.h"

#include "TestSupport.h"

#include <mmdModel/Canonicalize.h>
#include <mmdMotionBinding/Bind.h>
#include <motionVmd/Cp932.h>
#include <motionVmd/Motion.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>
#include <vector>

namespace {

using namespace mmd;
using namespace mmd::control;
using mmd::control::test::AboutX;
using mmd::control::test::AboutZ;
using mmd::control::test::Near;
using mmd::control::test::SameRotation;
namespace pmx = mmd::pmx;
namespace vmd = motionVmd;

constexpr double kPi = std::numbers::pi;
constexpr motionVmd::Bezier kLinear{20, 20, 107, 107};

pmx::Bone
MakeBone(const std::string& name, pmx::Vec3 position, std::int32_t parent, std::uint16_t flags = 0)
{
    pmx::Bone b;
    b.name = name;
    b.position = position;
    b.parent = parent;
    b.flags = flags;
    return b;
}

pmx::Bone
Appending(pmx::Bone b, std::int32_t source, float ratio, std::uint16_t kind)
{
    b.flags |= kind;
    b.appendParent = source;
    b.appendRatio = ratio;
    return b;
}

CanonicalDocument
Canonical(const pmx::Document& doc)
{
    auto result = Canonicalize(doc);
    assert(result.ok());
    return std::move(result).value();
}

Evaluator
Prepared(const CanonicalDocument& model)
{
    auto result = Evaluator::Prepare(model);
    assert(result.ok());
    return std::move(result).value();
}

std::int32_t
J(const CanonicalDocument& model, std::size_t sourceBone)
{
    return model.skeleton.jointOfSourceBone[sourceBone];
}

/// A one-key bone track: the motion holds it at every time.
binding::BoneTrack
Hold(std::int32_t joint, Float3 translation, const Quat& rotation)
{
    binding::BoneKey key;
    key.translation = translation;
    key.rotation = test::ToFloat4(rotation);
    key.curves = {kLinear, kLinear, kLinear, kLinear};
    return binding::BoneTrack{joint, {key}};
}

void
SortTracks(binding::BoundMotion& motion)
{
    std::sort(motion.bones.begin(), motion.bones.end(),
              [](const auto& a, const auto& b) { return a.joint < b.joint; });
}

bool
HasCode(const std::vector<Diagnostic>& diagnostics, const Code& code)
{
    return std::any_of(diagnostics.begin(), diagnostics.end(),
                       [&](const Diagnostic& d) { return d.code == code.id; });
}

// -- Rest pose and forward kinematics (§11.4) ---------------------------------

void
TestRestAndForwardKinematics()
{
    pmx::Document doc;
    doc.bones = {MakeBone("root", {0, 0, 0}, pmx::kNoIndex),
                 MakeBone("arm", {12.5f, 0, 0}, 0),
                 MakeBone("hand", {25.0f, 0, 0}, 1)};
    const CanonicalDocument model = Canonical(doc);
    const Evaluator evaluator = Prepared(model);

    const Pose rest = evaluator.Evaluate({}, 0.0);
    assert(rest.joints.size() == 3 && rest.channels.empty() && rest.visible);
    const std::vector<JointTransform> restWorld = evaluator.World(rest);
    for (std::size_t j = 0; j < 3; ++j) {
        assert(rest.joints[j].translation == model.skeleton.bones[j].localTranslation);
        assert(rest.joints[j].rotation == kIdentity);
        assert(Near(restWorld[j].translation, model.skeleton.bones[j].position, 1e-12));
    }

    // The root turned 90 degrees about Z carries the hand from (2, 0, 0) to
    // (0, 2, 0).
    binding::BoundMotion motion;
    motion.bones.push_back(Hold(J(model, 0), {0, 0, 0}, AboutZ(kPi / 2)));
    const std::vector<JointTransform> world = evaluator.World(evaluator.Evaluate(motion, 0.0));
    assert(Near(world[static_cast<std::size_t>(J(model, 2))].translation, {0.0, 2.0, 0.0}, 1e-6));
    std::puts("ok  rest pose and forward kinematics");
}

// -- Evaluation order (§11.5) --------------------------------------------------

void
TestOrder()
{
    pmx::Document doc;
    doc.bones = {MakeBone("a", {0, 0, 0}, pmx::kNoIndex),
                 MakeBone("b", {0, 0, 0}, pmx::kNoIndex),
                 MakeBone("c", {0, 0, 0}, pmx::kNoIndex, pmx::BoneFlag::DeformAfterPhysics),
                 MakeBone("d", {0, 0, 0}, pmx::kNoIndex)};
    doc.bones[0].transformLayer = 1;
    const CanonicalDocument model = Canonical(doc);
    const Evaluator evaluator = Prepared(model);
    // Layer 0 by source index, then layer 1, then after physics.
    const std::vector<std::int32_t> expected{J(model, 1), J(model, 3), J(model, 0), J(model, 2)};
    assert(evaluator.Order() == expected);
    std::puts("ok  evaluation order");
}

// -- Morphs (§11.3) -----------------------------------------------------------

void
TestMorphs()
{
    pmx::Document doc;
    doc.bones = {MakeBone("root", {0, 0, 0}, pmx::kNoIndex), MakeBone("x", {12.5f, 0, 0}, 0)};
    const Quat quarter = AboutZ(kPi / 2); // about Z, which the mirror keeps
    pmx::Morph bone;
    bone.name = "bone";
    bone.type = pmx::MorphType::Bone;
    bone.boneOffsets.push_back(pmx::BoneOffset{1, {12.5f, 0, 0}, test::ToFloat4(quarter)});
    pmx::Morph inner;
    inner.name = "inner";
    inner.type = pmx::MorphType::Group;
    inner.groupOffsets = {{0, 1.0f}};
    pmx::Morph outer;
    outer.name = "outer";
    outer.type = pmx::MorphType::Group;
    outer.groupOffsets = {{1, 0.5f}, {0, 1.0f}};
    pmx::Morph vertex;
    vertex.name = "vertex";
    vertex.type = pmx::MorphType::Vertex;
    doc.morphs = {bone, inner, outer, vertex};
    const CanonicalDocument model = Canonical(doc);
    const Evaluator evaluator = Prepared(model);

    binding::BoundMotion motion;
    motion.bones.push_back(Hold(J(model, 1), {0, 0, 0}, AboutZ(kPi / 18))); // 10 degrees
    motion.morphs = {binding::MorphTrack{0, {{0, 0.2f}}},
                     binding::MorphTrack{2, {{0, 1.0f}}},
                     binding::MorphTrack{3, {{0, 0.25f}}}};
    const Pose pose = evaluator.Evaluate(motion, 0.0);

    // The bone morph's effective weight: its own 0.2, the outer group's 1,
    // and the inner group's 0.5 through the outer -- 1.7 -- applied after
    // the key: 1.7 * 90 + 10 = 163 degrees, and 1.7 m along X.
    const JointTransform& x = pose.joints[static_cast<std::size_t>(J(model, 1))];
    assert(Near(x.translation,
                {model.skeleton.bones[1].localTranslation[0] + 1.7, 0.0, 0.0}, 1e-6));
    assert(SameRotation(x.rotation, AboutZ(163.0 * kPi / 180.0), 1e-9));

    // Channels: every track but the bone morph's, weights unexpanded.
    const std::vector<MorphChannel> channels{{2, 1.0}, {3, 0.25}};
    assert(pose.channels == channels);
    std::puts("ok  bone and group morphs, and channels");
}

// -- Appends (§11.6) ----------------------------------------------------------

void
TestAppends()
{
    using pmx::BoneFlag;
    pmx::Document doc;
    doc.bones = {
        MakeBone("A", {0, 0, 0}, pmx::kNoIndex),
        Appending(MakeBone("B", {12.5f, 0, 0}, pmx::kNoIndex), 0, 0.5f, BoneFlag::AppendRotation),
        Appending(MakeBone("C", {25.0f, 0, 0}, pmx::kNoIndex), 1, 1.0f, BoneFlag::AppendRotation),
        Appending(MakeBone("D", {37.5f, 0, 0}, pmx::kNoIndex), 0, -1.0f, BoneFlag::AppendRotation),
        Appending(MakeBone("E", {50.0f, 0, 0}, pmx::kNoIndex), 0, 2.0f, BoneFlag::AppendTranslation),
    };
    const CanonicalDocument model = Canonical(doc);
    const Evaluator evaluator = Prepared(model);

    binding::BoundMotion motion;
    motion.bones.push_back(Hold(J(model, 0), {0.1f, 0, 0}, AboutZ(kPi / 2)));
    motion.bones.push_back(Hold(J(model, 1), {0, 0, 0}, AboutZ(kPi / 18)));
    SortTracks(motion);
    const Pose pose = evaluator.Evaluate(motion, 0.0);
    const auto joint = [&](std::size_t source) {
        return pose.joints[static_cast<std::size_t>(J(model, source))];
    };

    const double degree = kPi / 180.0;
    // B: its own 10 degrees, then half of A's 90.
    assert(SameRotation(joint(1).rotation, AboutZ(55 * degree), 1e-9));
    // C: B's whole rotation -- key and append -- so a chain composes.
    assert(SameRotation(joint(2).rotation, AboutZ(55 * degree), 1e-9));
    // D: a negative ratio turns the other way.
    assert(SameRotation(joint(3).rotation, AboutZ(-90 * degree), 1e-9));
    // E: twice A's motion translation, on top of its rest; no rotation.
    assert(Near(joint(4).translation,
                {model.skeleton.bones[static_cast<std::size_t>(J(model, 4))].localTranslation[0] + 0.2,
                 0.0,
                 0.0},
                1e-7));
    assert(joint(4).rotation == kIdentity);
    std::puts("ok  appends: ratio, negative ratio, chains, translation");
}

// -- IK (§11.7) ---------------------------------------------------------------

/// A one-link arm from the origin to (1, 0, 0) m, and an IK bone at
/// (0, 1, 0) m whose effector is the arm's tip.
pmx::Document
OneLinkArm(float limitAngle, std::int32_t loops)
{
    pmx::Document doc;
    doc.bones = {MakeBone("base", {0, 0, 0}, pmx::kNoIndex),
                 MakeBone("tip", {12.5f, 0, 0}, 0),
                 MakeBone("ik", {0, 12.5f, 0}, pmx::kNoIndex, pmx::BoneFlag::Ik)};
    doc.bones[2].ik.target = 1;
    doc.bones[2].ik.loopCount = loops;
    doc.bones[2].ik.limitAngle = limitAngle;
    doc.bones[2].ik.links = {pmx::IkLink{0, false, {}, {}}};
    return doc;
}

void
TestIkSingleLink()
{
    // Unlimited: one iteration turns the base 90 degrees about Z, exactly.
    {
        const CanonicalDocument model = Canonical(OneLinkArm(static_cast<float>(kPi), 10));
        const Evaluator evaluator = Prepared(model);
        const Pose pose = evaluator.Evaluate({}, 0.0);
        assert(SameRotation(pose.joints[static_cast<std::size_t>(J(model, 0))].rotation,
                            AboutZ(kPi / 2), 1e-12));
        const auto world = evaluator.World(pose);
        assert(Near(world[static_cast<std::size_t>(J(model, 1))].translation, {0, 1, 0}, 1e-12));
    }
    // The angle limit: 0.2 rad per iteration, three iterations -- 0.6 rad.
    {
        const CanonicalDocument model = Canonical(OneLinkArm(0.2f, 3));
        const Evaluator evaluator = Prepared(model);
        const Pose pose = evaluator.Evaluate({}, 0.0);
        assert(SameRotation(pose.joints[static_cast<std::size_t>(J(model, 0))].rotation,
                            AboutZ(3.0 * static_cast<double>(0.2f)), 1e-9));
    }
    // A limited link, about X and Z: the Euler Z angle stops at 0.5 rad, and
    // the next iteration improves nothing, so the solve stops there.
    {
        pmx::Document doc = OneLinkArm(static_cast<float>(kPi), 10);
        doc.bones[2].ik.links[0] = pmx::IkLink{0, true, {-0.1f, 0, -0.5f}, {0.1f, 0, 0.5f}};
        const CanonicalDocument model = Canonical(doc);
        const Evaluator evaluator = Prepared(model);
        const Pose pose = evaluator.Evaluate({}, 0.0);
        assert(SameRotation(pose.joints[static_cast<std::size_t>(J(model, 0))].rotation,
                            AboutZ(static_cast<double>(0.5f)), 1e-9));
    }
    // No iterations, no solve.
    {
        const CanonicalDocument model = Canonical(OneLinkArm(static_cast<float>(kPi), 0));
        const Evaluator evaluator = Prepared(model);
        const Pose pose = evaluator.Evaluate({}, 0.0);
        assert(pose.joints[static_cast<std::size_t>(J(model, 0))].rotation == kIdentity);
    }
    std::puts("ok  IK: one link, angle limit, Euler limits, zero loops");
}

/// A leg: thigh at 1 m, knee at 0.5 m, ankle at the origin; the knee bends
/// about X only, as distributed models' knees do; 足D follows the thigh, one
/// layer later.
pmx::Document
Leg()
{
    using pmx::BoneFlag;
    pmx::Document doc;
    doc.bones = {MakeBone("root", {0, 0, 0}, pmx::kNoIndex),
                 MakeBone("左足", {0, 12.5f, 0}, 0),
                 MakeBone("左ひざ", {0, 6.25f, 0}, 1),
                 MakeBone("左足首", {0, 0, 0}, 2),
                 MakeBone("左足ＩＫ", {0, 0, 0}, 0, BoneFlag::Ik),
                 Appending(MakeBone("左足D", {0, 12.5f, 0}, 0), 1, 1.0f, BoneFlag::AppendRotation)};
    doc.bones[5].transformLayer = 1;
    pmx::Ik& ik = doc.bones[4].ik;
    ik.target = 3;
    ik.loopCount = 40;
    ik.limitAngle = 2.0f;
    // The source's knee limit, about its own X: [-180, -0.5] degrees.
    ik.links = {pmx::IkLink{2, true, {static_cast<float>(-kPi), 0, 0}, {-0.00872665f, 0, 0}},
                pmx::IkLink{1, false, {}, {}}};
    return doc;
}

void
TestIkLeg()
{
    const CanonicalDocument model = Canonical(Leg());
    const Evaluator evaluator = Prepared(model);
    const auto at = [&](std::size_t source) { return static_cast<std::size_t>(J(model, source)); };

    // The goal raised 0.2 m and brought 0.05 m forward: 0.80 m from the hip,
    // within the leg's 1 m.
    binding::BoundMotion motion;
    motion.bones.push_back(Hold(J(model, 4), {0, 0.2f, 0.05f}, kIdentity));
    const Pose pose = evaluator.Evaluate(motion, 0.0);
    const auto world = evaluator.World(pose);
    const Double3 goal = world[at(4)].translation;
    assert(Near(goal, {0.0, 0.2, 0.05}, 1e-6));
    assert(Near(world[at(3)].translation, goal, 1e-3));

    // The knee turns about X only, within its converted limit [0.5, 180]
    // degrees; the thigh carries the rest.
    const Quat knee = pose.joints[at(2)].rotation;
    assert(Near(knee[1], 0.0, 1e-12) && Near(knee[2], 0.0, 1e-12));
    const double kneeAngle = 2.0 * std::atan2(knee[0], knee[3]);
    assert(kneeAngle >= 0.00872665 - 1e-6 && kneeAngle <= kPi + 1e-6);
    assert(!SameRotation(pose.joints[at(1)].rotation, kIdentity, 1e-6));

    // 足D follows the solved thigh.
    assert(SameRotation(pose.joints[at(5)].rotation, pose.joints[at(1)].rotation, 1e-12));

    // Disabled by its IK track, the chain is not solved: the links keep
    // their motion, here none.
    binding::BoundMotion disabled = motion;
    disabled.ik.push_back(binding::IkTrack{J(model, 4), {{0, false}}});
    const Pose off = evaluator.Evaluate(disabled, 0.0);
    assert(off.joints[at(1)].rotation == kIdentity && off.joints[at(2)].rotation == kIdentity);
    assert(Near(evaluator.World(off)[at(3)].translation, {0, 0, 0}, 1e-12));
    std::puts("ok  IK: a leg with a plane knee, 足D, and the IK-enable track");
}

// -- Determinism and robustness of the inputs (§11.1) --------------------------

void
TestStatelessAndDeterministic()
{
    const CanonicalDocument model = Canonical(Leg());
    const Evaluator evaluator = Prepared(model);
    binding::BoundMotion motion;
    binding::BoneTrack goal;
    goal.joint = J(model, 4);
    binding::BoneKey k0;
    k0.curves = {kLinear, kLinear, kLinear, kLinear};
    binding::BoneKey k1 = k0;
    k1.frame = 30;
    k1.translation = {0, 0.4f, 0.1f};
    goal.keys = {k0, k1};
    motion.bones.push_back(goal);

    const Pose first = evaluator.Evaluate(motion, 12.5);
    (void)evaluator.Evaluate(motion, 29.0);
    Pose again;
    evaluator.Evaluate(motion, 12.5, again);
    assert(first == again); // bit for bit, whatever came between

    // Tracks bound to another model are ignored.
    binding::BoundMotion stray;
    stray.bones.push_back(Hold(99, {1, 1, 1}, AboutZ(1.0)));
    stray.morphs.push_back(binding::MorphTrack{99, {{0, 1.0f}}});
    stray.ik.push_back(binding::IkTrack{99, {{0, false}}});
    const Pose ignored = evaluator.Evaluate(stray, 0.0);
    assert(ignored == evaluator.Evaluate({}, 0.0));

    // Visibility.
    binding::BoundMotion hidden;
    hidden.visibility = {{0, true}, {10, false}};
    assert(evaluator.Evaluate(hidden, 5.0).visible);
    assert(!evaluator.Evaluate(hidden, 10.0).visible);
    std::puts("ok  stateless, deterministic, stray tracks ignored, visibility");
}

// -- Diagnostics (§11.8) -------------------------------------------------------

void
TestDiagnostics()
{
    using pmx::BoneFlag;
    pmx::Document doc = OneLinkArm(1.0f, 1000);
    doc.bones.push_back(MakeBone("external", {0, 0, 0}, pmx::kNoIndex, BoneFlag::ExternalParent));
    doc.bones.back().externalParentKey = 7;
    doc.bones.push_back(Appending(MakeBone("local", {0, 0, 0}, pmx::kNoIndex), 0, 1.0f,
                                  BoneFlag::AppendRotation | BoneFlag::LocalAppend));
    const CanonicalDocument model = Canonical(doc);
    auto prepared = Evaluator::Prepare(model);
    assert(prepared.ok());
    const auto& diagnostics = prepared.diagnostics();
    assert(diagnostics.size() == 3);
    assert(HasCode(diagnostics, codes::MotionIkLoopClamped));
    assert(HasCode(diagnostics, codes::MotionExternalParentIgnored));
    assert(HasCode(diagnostics, codes::MotionLocalAppendApproximated));

    pmx::Document negative = OneLinkArm(1.0f, -5);
    auto clamped = Evaluator::Prepare(Canonical(negative));
    assert(clamped.diagnostics().size() == 1 &&
           clamped.diagnostics()[0].code == codes::MotionIkLoopClamped.id);
    // Clamped to zero: nothing is solved.
    assert(clamped.value().Evaluate({}, 0.0).joints[0].rotation == kIdentity);

    assert(Evaluator::Prepare(Canonical(OneLinkArm(1.0f, 256))).diagnostics().empty());
    std::puts("ok  diagnostics");
}

// -- Through Bind, as a runtime calls it ---------------------------------------

vmd::Name
VmdName(const std::string& utf8, std::size_t width)
{
    std::string bytes = vmd::EncodeCp932(utf8).value();
    bytes.resize(std::min(bytes.size(), width));
    const auto decoded = vmd::DecodeCp932(
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()));
    return vmd::Name{bytes, decoded.text};
}

void
TestThroughBind()
{
    const CanonicalDocument model = Canonical(Leg());
    const Evaluator evaluator = Prepared(model);

    // The same goal as TestIkLeg, in the source basis: (0, 0.2, 0.05) m is
    // (0, 2.5, -0.625) MMD units. The IK is switched off at frame 10.
    vmd::Document doc;
    vmd::BoneKeyframe key;
    key.bone = VmdName("左足ＩＫ", 15);
    key.translation = {0.0f, 2.5f, -0.625f};
    key.interpolation.fill(20);
    doc.boneKeyframes.push_back(key);
    for (const auto& [frame, enabled] : {std::pair{0u, 1}, std::pair{10u, 0}}) {
        vmd::IkKeyframe ik;
        ik.frame = frame;
        ik.visible = 1;
        ik.ik.push_back(vmd::IkState{VmdName("左足ＩＫ", 20), static_cast<std::uint8_t>(enabled)});
        doc.ikKeyframes.push_back(ik);
    }
    doc.sectionsPresent = vmd::kSectionCount;
    auto built = vmd::BuildMotion(doc);
    assert(built.ok());
    auto bound = binding::Bind(built.value(), model);
    assert(bound.ok() && bound.value().bones.size() == 1 && bound.value().ik.size() == 1);

    const std::size_t ankle = static_cast<std::size_t>(J(model, 3));
    const auto on = evaluator.World(evaluator.Evaluate(bound.value(), 0.0));
    assert(Near(on[ankle].translation, {0.0, 0.2, 0.05}, 1e-3));
    const auto off = evaluator.World(evaluator.Evaluate(bound.value(), 10.0));
    assert(Near(off[ankle].translation, {0, 0, 0}, 1e-12));
    std::puts("ok  a VMD through Bind: IK on, then off");
}

} // namespace

void
RunEvaluateTests()
{
    TestRestAndForwardKinematics();
    TestOrder();
    TestMorphs();
    TestAppends();
    TestIkSingleLink();
    TestIkLeg();
    TestStatelessAndDeterministic();
    TestDiagnostics();
    TestThroughBind();
}
