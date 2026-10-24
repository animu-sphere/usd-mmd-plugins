// SPDX-License-Identifier: Apache-2.0
// Generated PMX + VMD -> owner continuation -> coupling -> post append.
// Caller supplies the body snapshot; no physics step occurs in this smoke.
#include <mmdPhysicsAdapter/Adapter.h>
#include <mmdPhysicsAdapter/BodyProperties.h>
#include <mmdPhysicsAdapter/JointSprings.h>
#include <mmdModel/Canonicalize.h>
#include <mmdPmx/Reader.h>
#include <motionVmd/Reader.h>
#include <motionVmd/Motion.h>
#include <pxr/base/gf/rotation.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/xformCache.h>
#include <pxr/usd/usdPhysics/rigidBodyAPI.h>
#include <cmath>
#include <filesystem>
#include <iostream>

using namespace pxr;
namespace {
bool
Near(double a, double b)
{
    return std::abs(a - b) < 1e-6;
}
bool
Rotation(const mmd::control::Quat& q, double degrees)
{
    const auto expected = GfRotation(GfVec3d(0, 0, 1), degrees).GetQuat();
    const auto v = expected.GetImaginary();
    const double dot = q[0] * v[0] + q[1] * v[1] + q[2] * v[2] + q[3] * expected.GetReal();
    return Near(std::abs(dot), 1);
}
} // namespace
int
main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const auto source = mmd::pmx::ReadFile(std::filesystem::path(argv[1]));
    const auto motion = motionVmd::ReadFile(std::filesystem::path(argv[2]));
    if (!source || !motion)
        return 3;
    const auto canonical = mmd::Canonicalize(source.value());
    if (!canonical)
        return 4;
    const auto& model = canonical.value();
    const auto built = motionVmd::BuildMotion(motion.value());
    if (!built)
        return 5;
    const auto bound = mmd::binding::Bind(built.value(), model);
    const auto prepared = mmd::control::Evaluator::Prepare(model);
    auto stage = UsdStage::Open(argv[1]);
    if (!bound || !prepared || !stage)
        return 6;
    std::vector<UsdPrim> bodies;
    for (const auto& prim : stage->Traverse())
        if (prim.HasAPI<UsdPhysicsRigidBodyAPI>())
            bodies.push_back(prim);
    const auto skeleton = UsdSkelSkeleton(stage->GetPrimAtPath(SdfPath("/Asset/skel/Skeleton")));
    // A second generated fixture supplies a standard spring6Dof declaration.
    const auto springStage = UsdStage::Open(
        (std::filesystem::path(argv[1]).parent_path() / "live-physics-pendulum.pmx").string());
    if (!springStage)
        return 20;
    bool springRead = false;
    for (const auto& prim : springStage->Traverse()) {
        if (!prim.GetAttribute(TfToken("mmd:physics:translationSpring")))
            continue;
        const auto springs = mmd::physics::ReadJointSprings(prim);
        if (!springs || !springs.value() || !springs.value()->spring6Dof)
            return 18;
        GfVec3f sourceRotation;
        prim.GetAttribute(TfToken("mmd:physics:rotationSpring")).Get(&sourceRotation);
        for (int axis = 0; axis < 3; ++axis)
            if (!Near(springs.value()->rotation[axis], double(sourceRotation[axis]) * .0064))
                return 19;
        springRead = true;
    }
    if (!springRead)
        return 20;
    for (const auto& body : bodies) {
        const auto properties = mmd::physics::ReadBodyProperties(body);
        if (!properties || !properties.value().filter || !properties.value().contact ||
            !properties.value().damping)
            return 16;
        int group = -1, mask = -1;
        body.GetAttribute(TfToken("mmd:physics:collisionGroup")).Get(&group);
        body.GetAttribute(TfToken("mmd:physics:collisionMask")).Get(&mask);
        if (properties.value().filter->category != (uint32_t{1} << group) ||
            properties.value().filter->mask != uint32_t(mask))
            return 17;
    }
    VtTokenArray joints;
    if (!skeleton.GetJointsAttr().Get(&joints) || joints.size() != model.skeleton.bones.size())
        return 7;
    for (std::size_t i = 0; i < joints.size(); ++i)
        if (joints[i].GetString() != model.skeleton.bones[i].jointPath)
            return 8;
    auto binding = mmd::physics::Adapter::Bind(skeleton, bodies);
    if (!binding) {
        std::cerr << binding.fatal()->message << '\n';
        return 9;
    }
    const auto& evaluator = prepared.value();
    auto& adapter = binding.value();
    UsdGeomXformCache cache;
    std::vector<mmd::physics::BodyTransform> snapshot;
    for (const auto& prim : bodies) {
        TfToken mode;
        prim.GetAttribute(TfToken("mmd:physics:mode")).Get(&mode);
        if (mode == TfToken("followBone"))
            continue;
        const auto m = cache.GetLocalToWorldTransform(prim);
        const auto t = m.ExtractTranslation();
        const double angle = mode == TfToken("dynamic") ? 170 : 60;
        const auto q = GfRotation(GfVec3d(0, 0, 1), angle).GetQuat();
        const auto v = q.GetImaginary();
        snapshot.push_back(
            {prim.GetPath(), {{t[0] + 3, t[1] + 4, t[2] + 5}, {v[0], v[1], v[2], q.GetReal()}}});
    }
    if (bodies.size() != 3 || model.skeleton.bones.size() != 3 || snapshot.size() != 2)
        return 10;
    mmd::control::Pose first;
    for (int replay = 0; replay < 3; ++replay) {
        adapter.Reset();
        const auto continuation = evaluator.BeginPhysics(bound.value(), 0);
        const auto& before = continuation.BeforePhysics();
        const auto targets = adapter.UpdateKinematicBodies(before);
        const auto feedback = adapter.ApplyPhysicsToPose(snapshot, before);
        if (!targets || targets.value().size() != 1 || !feedback || feedback.value().size() != 2 ||
            feedback.value()[0].joint != 0 || feedback.value()[1].joint != 1)
            return 11;
        // Frame 0 of sample.vmd drives the root at its rest pose.
        if (!Near(targets.value()[0].world.translation[0], 0.08) ||
            !Near(targets.value()[0].world.translation[1], 0))
            return 12;
        const auto final = evaluator.ResumePhysics(continuation, feedback.value());
        if (!final || !Rotation(final.value().joints[0].rotation, 170) ||
            !Rotation(final.value().joints[1].rotation, -110) ||
            !Rotation(final.value().joints[2].rotation, -55) ||
            final.value().joints[0].translation != mmd::Double3{3, 4, 5} ||
            final.value().joints[1].translation != before.joints[1].translation ||
            final.value().channels != before.channels || final.value().visible != before.visible)
            return 13;
        if (replay && final.value() != first)
            return 14;
        first = final.value();
        // Reusing the retained barrier must not accumulate append/feedback.
        const auto repeated = evaluator.ResumePhysics(continuation, feedback.value());
        if (!repeated || repeated.value() != first)
            return 15;
    }
    std::cout << "coupling=3 targets=1 feedback=2 postAppend=-55 replay=3\n";
}
