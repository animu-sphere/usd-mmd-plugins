// SPDX-License-Identifier: Apache-2.0
#include <mmdPhysicsAdapter/Adapter.h>
#include <mmdPhysicsAdapter/Codes.h>
#include <pxr/base/gf/rotation.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/xformCache.h>
#include <pxr/usd/usdPhysics/rigidBodyAPI.h>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

using namespace pxr;
using namespace mmd;
using control::JointTransform;
using physics::Adapter;
using physics::BodyTransform;

namespace {
control::Quat
Rotation(double degrees, GfVec3d axis = {0, 0, 1})
{
    const auto q = GfRotation(axis, degrees).GetQuat();
    const auto v = q.GetImaginary();
    return {v[0], v[1], v[2], q.GetReal()};
}
bool
Close(double a, double b)
{
    return std::abs(a - b) < 1e-6;
}
void
EqualRotation(control::Quat a, control::Quat b)
{
    double dot = 0;
    for (int i = 0; i < 4; ++i)
        dot += a[i] * b[i];
    assert(Close(std::abs(dot), 1));
}
struct Fixture {
    UsdStageRefPtr stage = UsdStage::CreateInMemory();
    UsdSkelSkeleton skeleton;
    std::vector<UsdPrim> bodies;
    control::Pose pose;
    Fixture(double unit = 1)
    {
        UsdGeomSetStageUpAxis(stage, UsdGeomTokens->y);
        UsdGeomSetStageMetersPerUnit(stage, unit);
        const auto asset = UsdGeomXform::Define(stage, SdfPath("/Asset"));
        asset.AddTranslateOp().Set(GfVec3d(10, 0, 0) / unit);
        asset.AddRotateZOp().Set(90.0f);
        skeleton = UsdSkelSkeleton::Define(stage, SdfPath("/Asset/Skeleton"));
        skeleton.CreateJointsAttr().Set(
            VtTokenArray{TfToken("root"), TfToken("root/mid"), TfToken("root/mid/tip")});
        GfMatrix4d root(1), mid(1), tip(1);
        mid.SetTranslate({1 / unit, 0, 0});
        tip.SetTranslate({1 / unit, 0, 0});
        skeleton.CreateRestTransformsAttr().Set(VtMatrix4dArray{root, mid, tip});
        // Body order deliberately differs from joint order.
        Body("tipBody", 2, "dynamicWithBone", {2 / unit, 1 / unit, 0}, 30);
        Body("follow", 0, "followBone", {0, 1 / unit, 0}, 30);
        Body("free", -1, "dynamic", {1 / unit, 0, 0});
        Body("midBody", 1, "dynamicWithBone", {1 / unit, 0, 0});
        pose.joints = {{{0, 0, 0}, control::kIdentity},
                       {{1, 0, 0}, control::kIdentity},
                       {{1, 0, 0}, control::kIdentity}};
    }
    void Body(const char* name, int bone, const char* mode, GfVec3d position, float angle = 0)
    {
        auto xf = UsdGeomXform::Define(stage, SdfPath(std::string("/Asset/") + name));
        xf.AddTranslateOp().Set(position);
        xf.AddRotateZOp().Set(angle);
        const auto api = UsdPhysicsRigidBodyAPI::Apply(xf.GetPrim());
        api.CreateKinematicEnabledAttr().Set(std::string(mode) == "followBone");
        xf.GetPrim()
            .CreateAttribute(
                TfToken("mmd:physics:bone"), SdfValueTypeNames->Int, true, SdfVariabilityUniform)
            .Set(bone);
        xf.GetPrim()
            .CreateAttribute(
                TfToken("mmd:physics:mode"), SdfValueTypeNames->Token, true, SdfVariabilityUniform)
            .Set(TfToken(mode));
        bodies.push_back(xf.GetPrim());
    }
    auto Bind() { return Adapter::Bind(skeleton, bodies); }
};
void
TestModesAndSpaces()
{
    for (double unit : {1.0, 0.01}) {
        Fixture f(unit);
        auto binding = f.Bind();
        if (!binding)
            std::cerr << binding.fatal()->message << "\n";
        assert(binding);
        auto& adapter = binding.value();
        f.pose.joints[0].rotation = Rotation(90);
        const auto pose = f.pose;
        const auto updates = adapter.UpdateKinematicBodies(pose);
        assert(updates && updates.value().size() == 1);
        const auto& follow = updates.value()[0];
        assert(follow.body == f.bodies[1].GetPath());
        assert(Close(follow.world.translation[0], 10) && Close(follow.world.translation[1], -1));
        EqualRotation(follow.world.rotation, Rotation(210));
        // World rotations: placement 90 + mid 45, tip local 15 + offset 30.
        // Translations are arbitrary: mode 2 keeps the pre-step local position.
        std::vector<BodyTransform> snapshot{
            {f.bodies[0].GetPath(), {{200, 300, 400}, Rotation(180)}},
            {f.bodies[3].GetPath(), {{400, 500, 600}, Rotation(135)}},
            {f.bodies[2].GetPath(), {{700, 800, 900}, Rotation(270)}}};
        const auto feedback = adapter.ApplyPhysicsToPose(snapshot, pose);
        assert(feedback && feedback.value().size() == 2);
        assert(feedback.value()[0].joint == 1 && feedback.value()[1].joint == 2);
        EqualRotation(feedback.value()[0].local.rotation, Rotation(-45));
        EqualRotation(feedback.value()[1].local.rotation, Rotation(15));
        assert(feedback.value()[0].local.translation == pose.joints[1].translation);
        assert(feedback.value()[1].local.translation == pose.joints[2].translation);
        assert(f.pose == pose);
        const auto first = feedback.value();
        for (int i = 0; i < 3; ++i) {
            adapter.Reset();
            const auto replay = adapter.ApplyPhysicsToPose(snapshot, pose);
            assert(replay && replay.value().size() == first.size());
            for (std::size_t j = 0; j < first.size(); ++j)
                assert(replay.value()[j].local == first[j].local);
        }
        // Owned binding survives stage destruction and has no feedback history.
        f.skeleton = UsdSkelSkeleton();
        f.bodies.clear();
        f.stage.Reset();
        assert(adapter.UpdateKinematicBodies(pose).value() == updates.value());
    }
}
void
TestFailures()
{
    Fixture f;
    const auto expect = [](const auto& r, const Code& c) { assert(!r && r.fatal()->code == c.id); };
    const auto bone = f.bodies[0].GetAttribute(TfToken("mmd:physics:bone"));
    for (int value : {-2, 3}) {
        bone.Set(value);
        expect(f.Bind(), codes::PhysicsBindingInvalid);
    }
    bone.Set(-1);
    auto unbound = f.Bind();
    assert(unbound && unbound.diagnostics().size() == 2);
    assert(unbound.diagnostics()[0].code == codes::PhysicsBoneUnassociated.id);
    bone.Set(2);
    f.bodies[3].GetAttribute(TfToken("mmd:physics:bone")).Set(2);
    expect(f.Bind(), codes::PhysicsBindingInvalid);
    f.bodies[3].GetAttribute(TfToken("mmd:physics:bone")).Set(1);
    // Modes 1 and 2 are both writers; neither may silently override the other.
    f.bodies[2].GetAttribute(TfToken("mmd:physics:bone")).Set(1);
    expect(f.Bind(), codes::PhysicsBindingInvalid);
    f.bodies[2].GetAttribute(TfToken("mmd:physics:bone")).Set(-1);
    auto scale = UsdGeomXformable(f.stage->GetPrimAtPath(SdfPath("/Asset"))).AddScaleOp();
    for (const auto value : {GfVec3f(2), GfVec3f(1, 2, 1), GfVec3f(-1, 1, 1), GfVec3f(0)}) {
        scale.Set(value);
        expect(f.Bind(), codes::PhysicsTransformUnsupported);
    }
    scale.Set(GfVec3f(1));
    auto adapter = f.Bind().value();
    auto bad = f.pose;
    bad.joints.pop_back();
    expect(adapter.UpdateKinematicBodies(bad), codes::PhysicsPoseInvalid);
    bad = f.pose;
    bad.joints[0].rotation = {0, 0, 0, 0};
    expect(adapter.ApplyPhysicsToPose({}, bad), codes::PhysicsPoseInvalid);
    expect(adapter.ApplyPhysicsToPose({}, f.pose), codes::PhysicsSnapshotInvalid);
    std::vector<BodyTransform> snapshot{{f.bodies[0].GetPath(), {}}, {f.bodies[3].GetPath(), {}}};
    assert(adapter.ApplyPhysicsToPose(snapshot, f.pose));
    snapshot.push_back(snapshot[0]);
    expect(adapter.ApplyPhysicsToPose(snapshot, f.pose), codes::PhysicsSnapshotInvalid);
    snapshot.pop_back();
    snapshot[0].body = SdfPath("/Unknown");
    expect(adapter.ApplyPhysicsToPose(snapshot, f.pose), codes::PhysicsSnapshotInvalid);
    snapshot[0].body = f.bodies[0].GetPath();
    snapshot[0].world.translation[0] = std::numeric_limits<double>::infinity();
    expect(adapter.ApplyPhysicsToPose(snapshot, f.pose), codes::PhysicsSnapshotInvalid);
}
void
TestNonCommutingFeedback()
{
    Fixture f;
    f.pose.joints[0].rotation = Rotation(20, {1, 0, 0});
    auto binding = f.Bind().value();
    const auto q = [](const control::Quat& r) { return GfQuatd(r[3], r[0], r[1], r[2]); };
    const auto root = q(Rotation(90)) * q(f.pose.joints[0].rotation);
    const auto midLocal = Rotation(40, {0, 1, 0});
    const auto tipLocal = Rotation(-30, {1, 0, 0});
    const auto midWorld = root * q(midLocal);
    const auto tipWorld = midWorld * q(tipLocal) * q(Rotation(30));
    const auto t = [](GfQuatd r) {
        const auto v = r.GetImaginary();
        return JointTransform{{100, 200, 300}, {v[0], v[1], v[2], r.GetReal()}};
    };
    std::vector<BodyTransform> rows{{f.bodies[0].GetPath(), t(tipWorld)},
                                    {f.bodies[3].GetPath(), t(midWorld)}};
    const auto feedback = binding.ApplyPhysicsToPose(rows, f.pose);
    assert(feedback && feedback.value().size() == 2);
    EqualRotation(feedback.value()[0].local.rotation, midLocal);
    EqualRotation(feedback.value()[1].local.rotation, tipLocal);
    f.bodies[3].GetAttribute(TfToken("mmd:physics:mode")).Set(TfToken("followBone"));
    UsdPhysicsRigidBodyAPI(f.bodies[3]).GetKinematicEnabledAttr().Set(true);
    f.pose.joints[1].rotation = midLocal;
    auto intermediate = f.Bind().value();
    rows.resize(1);
    const auto tip = intermediate.ApplyPhysicsToPose(rows, f.pose);
    assert(tip && tip.value().size() == 1 && tip.value()[0].joint == 2);
    EqualRotation(tip.value()[0].local.rotation, tipLocal);
}
void
TestDynamicChain()
{
    const auto matrix = [](const JointTransform& t) {
        GfMatrix4d m(1);
        m.SetRotate(GfQuatd(t.rotation[3], t.rotation[0], t.rotation[1], t.rotation[2]));
        m.SetTranslateOnly({t.translation[0], t.translation[1], t.translation[2]});
        return m;
    };
    const auto transform = [](const GfMatrix4d& m) {
        const auto q = m.ExtractRotationQuat().GetNormalized();
        const auto v = q.GetImaginary();
        const auto t = m.ExtractTranslation();
        return JointTransform{{t[0], t[1], t[2]}, {v[0], v[1], v[2], q.GetReal()}};
    };
    for (double unit : {1.0, 0.01}) {
        Fixture f(unit);
        // A mode-2 root of a swinging chain, followed by a mode-1 tip.
        f.bodies[0].GetAttribute(TfToken("mmd:physics:mode")).Set(TfToken("dynamic"));
        f.pose.joints[0].rotation = Rotation(20, {1, 0, 0});
        UsdGeomXformCache cache;
        auto placement = cache.GetLocalToWorldTransform(f.skeleton.GetPrim());
        placement.SetTranslateOnly(placement.ExtractTranslation() * unit);
        auto restMid = matrix(f.pose.joints[1]) * placement;
        auto restTip = matrix(f.pose.joints[2]) * restMid;
        // Rest offsets are independent of the animation in f.pose.
        auto bodyRest = cache.GetLocalToWorldTransform(f.bodies[0]);
        bodyRest.SetTranslateOnly(bodyRest.ExtractTranslation() * unit);
        const auto offset = bodyRest * restTip.GetInverse();
        const JointTransform mid{{1, 0, 0}, Rotation(40, {0, 1, 0})};
        const JointTransform tip{{.7, -.2, .3}, Rotation(-30, {1, 0, 0})};
        const auto rootWorld = matrix(f.pose.joints[0]) * placement;
        const auto midWorld = matrix(mid) * rootWorld;
        const auto tipWorld = matrix(tip) * midWorld;
        auto midBody = transform(midWorld);
        midBody.translation = {100, 200, 300}; // mode 2 keeps the original local position
        std::vector<BodyTransform> rows{{f.bodies[0].GetPath(), transform(offset * tipWorld)},
                                        {f.bodies[3].GetPath(), midBody}};
        const auto before = f.pose;
        const auto simulated = rows;
        auto binding = f.Bind().value();
        std::vector<control::Evaluator::PhysicsFeedback> first;
        for (int replay = 0; replay < 3; ++replay) {
            binding.Reset();
            const auto result = binding.ApplyPhysicsToPose(rows, before);
            assert(result && result.value().size() == 2);
            assert(result.value()[0].joint == 1 && result.value()[1].joint == 2);
            EqualRotation(result.value()[0].local.rotation, mid.rotation);
            EqualRotation(result.value()[1].local.rotation, tip.rotation);
            assert(result.value()[0].local.translation == before.joints[1].translation);
            for (int axis = 0; axis < 3; ++axis)
                assert(Close(result.value()[1].local.translation[axis], tip.translation[axis]));
            if (replay)
                for (std::size_t i = 0; i < first.size(); ++i)
                    assert(first[i].joint == result.value()[i].joint &&
                           first[i].local == result.value()[i].local);
            first = result.value();
            assert(f.pose == before && rows == simulated);
        }
        rows.erase(rows.begin());
        const auto missing = binding.ApplyPhysicsToPose(rows, before);
        assert(!missing && missing.fatal()->code == codes::PhysicsSnapshotInvalid.id);
        // Mode 1 also drives a root joint, removing its rest offset/placement once.
        f.bodies[2].GetAttribute(TfToken("mmd:physics:bone")).Set(0);
        auto rootRest = cache.GetLocalToWorldTransform(f.bodies[2]);
        rootRest.SetTranslateOnly(rootRest.ExtractTranslation() * unit);
        const auto rootOffset = rootRest * placement.GetInverse();
        const JointTransform root{{.3, .4, -.2}, Rotation(35, {0, 1, 0})};
        const std::vector<UsdPrim> rootOnly{f.bodies[2]};
        const auto rootBinding = Adapter::Bind(f.skeleton, rootOnly).value();
        const std::vector<BodyTransform> rootRows{
            {rootOnly[0].GetPath(), transform(rootOffset * matrix(root) * placement)}};
        const auto result = rootBinding.ApplyPhysicsToPose(rootRows, before);
        assert(result && result.value().size() == 1 && result.value()[0].joint == 0);
        EqualRotation(result.value()[0].local.rotation, root.rotation);
        for (int axis = 0; axis < 3; ++axis)
            assert(Close(result.value()[0].local.translation[axis], root.translation[axis]));
    }
}
void
TestDeclarations()
{
    const auto expect = [](const auto& r, const Code& c) { assert(!r && r.fatal()->code == c.id); };
    {
        Fixture f;
        auto mode = f.bodies[0].GetAttribute(TfToken("mmd:physics:mode"));
        mode.Set(TfToken("unknown"));
        expect(f.Bind(), codes::PhysicsBindingInvalid);
        mode.Set(TfToken("followBone"));
        expect(f.Bind(), codes::PhysicsBindingInvalid); // kinematic disagreement
        mode.Set(TfToken("dynamicWithBone"));
        mode.Set(TfToken("dynamicWithBone"), UsdTimeCode(1));
        expect(f.Bind(), codes::PhysicsBindingInvalid);
        mode.ClearAtTime(UsdTimeCode(1));
        mode.Block();
        expect(f.Bind(), codes::PhysicsBindingInvalid);
    }
    {
        Fixture f;
        auto prim = f.bodies[0];
        prim.RemoveProperty(TfToken("mmd:physics:bone"));
        prim.CreateAttribute(
                TfToken("mmd:physics:bone"), SdfValueTypeNames->Float, true, SdfVariabilityUniform)
            .Set(2.0f);
        expect(f.Bind(), codes::PhysicsBindingInvalid);
    }
    {
        Fixture f;
        auto prim = f.bodies[0];
        prim.RemoveProperty(TfToken("mmd:physics:bone"));
        prim.CreateAttribute(TfToken("mmd:physics:bone"), SdfValueTypeNames->Int).Set(2);
        expect(f.Bind(), codes::PhysicsBindingInvalid); // varying declaration
    }
    {
        Fixture f;
        f.bodies.push_back(f.bodies[0]);
        expect(f.Bind(), codes::PhysicsBindingInvalid);
    }
    {
        Fixture f, other;
        f.bodies.push_back(other.bodies[0]);
        expect(f.Bind(), codes::PhysicsBindingInvalid);
    }
    {
        Fixture f;
        UsdPhysicsRigidBodyAPI(f.bodies[0]).CreateRigidBodyEnabledAttr().Set(false);
        expect(f.Bind(), codes::PhysicsBindingInvalid);
    }
    {
        Fixture f;
        UsdGeomSetStageUpAxis(f.stage, UsdGeomTokens->z);
        expect(f.Bind(), codes::PhysicsBindingInvalid);
    }
    {
        Fixture f;
        f.skeleton.GetJointsAttr().Set(
            VtTokenArray{TfToken("root/mid"), TfToken("root"), TfToken("root/mid/tip")});
        expect(f.Bind(), codes::PhysicsBindingInvalid);
    }
    {
        Fixture f;
        auto op = UsdGeomXformable(f.bodies[0]).AddScaleOp();
        op.Set(GfVec3f(2));
        expect(f.Bind(), codes::PhysicsTransformUnsupported);
        op.Set(GfVec3f(1));
        op.Set(GfVec3f(1), UsdTimeCode(1));
        expect(f.Bind(), codes::PhysicsTransformUnsupported);
    }
    {
        Fixture f;
        auto order = f.bodies[0].GetAttribute(TfToken("xformOpOrder"));
        order.Set(VtTokenArray{TfToken("xformOp:translate:missing")});
        expect(f.Bind(), codes::PhysicsTransformUnsupported);
    }
    {
        Fixture f;
        f.skeleton.GetRestTransformsAttr().Set(VtMatrix4dArray{});
        expect(f.Bind(), codes::PhysicsBindingInvalid);
    }
    {
        Fixture f;
        auto binding = f.Bind().value();
        for (const auto badQ : {control::Quat{0, 0, 0, 0},
                                control::Quat{0, 0, 0, 2},
                                control::Quat{0, 0, 0, std::numeric_limits<double>::quiet_NaN()}}) {
            auto pose = f.pose;
            pose.joints[0].rotation = badQ;
            expect(binding.UpdateKinematicBodies(pose), codes::PhysicsPoseInvalid);
            std::vector<BodyTransform> rows{{f.bodies[0].GetPath(), {{}, badQ}}};
            expect(binding.ApplyPhysicsToPose(rows, f.pose), codes::PhysicsSnapshotInvalid);
        }
    }
    {
        Fixture f;
        // Unassociated simulated bodies produce no feedback.
        f.bodies[0].GetAttribute(TfToken("mmd:physics:mode")).Set(TfToken("dynamic"));
        f.bodies[3].GetAttribute(TfToken("mmd:physics:mode")).Set(TfToken("dynamic"));
        f.bodies[0].GetAttribute(TfToken("mmd:physics:bone")).Set(-1);
        f.bodies[3].GetAttribute(TfToken("mmd:physics:bone")).Set(-1);
        const auto binding = f.Bind();
        assert(binding && binding.value().ApplyPhysicsToPose({}, f.pose).value().empty());
        // Missing follow association cannot be updated; the limitation is reported.
        f.bodies[1].GetAttribute(TfToken("mmd:physics:bone")).Set(-1);
        const auto unbound = f.Bind();
        assert(unbound && !unbound.diagnostics().empty());
        assert(unbound.value().UpdateKinematicBodies(f.pose).value().empty());
    }
}
} // namespace
int
main()
{
    TestModesAndSpaces();
    TestFailures();
    TestDeclarations();
    TestNonCommutingFeedback();
    TestDynamicChain();
    std::cout << "MMD coupling modes, pose spaces, reset and rejection checks passed\n";
}
