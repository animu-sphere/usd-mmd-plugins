// SPDX-License-Identifier: Apache-2.0
#include "rotation_springs.h"
#include <usd_physics/usd/box_scene.h>
#include <usd_physics/usd/joint_conversion.h>
#include <usd_physics/jolt/availability.h>
#include <pxr/usd/usdGeom/sphere.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdPhysics/collisionAPI.h>
#include <pxr/usd/usdPhysics/massAPI.h>
#include <pxr/usd/usdPhysics/rigidBodyAPI.h>
#include <pxr/usd/usdPhysics/joint.h>
#include <algorithm>
#include <iostream>

namespace core = usd_physics::core;
namespace {
void
Check(bool ok, const char* message)
{
    if (!ok)
        throw std::runtime_error(message);
}
void
Springs(const pxr::UsdPrim& prim, const pxr::GfVec3f& k)
{
    prim.CreateAttribute(pxr::TfToken("mmd:physics:translationSpring"),
                         pxr::SdfValueTypeNames->Vector3f)
        .Set(pxr::GfVec3f(10));
    prim.CreateAttribute(pxr::TfToken("mmd:physics:rotationSpring"),
                         pxr::SdfValueTypeNames->Vector3f)
        .Set(k);
}
double
Response(int axis, int hz, float stiffness, float mass)
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    pxr::UsdGeomSetStageUpAxis(stage, pxr::UsdGeomTokens->y);
    pxr::UsdGeomSetStageMetersPerUnit(stage, 1);
    const auto sphere = pxr::UsdGeomSphere::Define(stage, pxr::SdfPath("/Body"));
    sphere.CreateRadiusAttr().Set(.16); // Source radius 2, scaled once to SI.
    pxr::GfVec3f direction(0);
    direction[axis] = 1;
    sphere.AddOrientOp().Set(pxr::GfQuatf(std::cos(.05), direction * std::sin(.05)));
    pxr::UsdPhysicsCollisionAPI::Apply(sphere.GetPrim());
    pxr::UsdPhysicsRigidBodyAPI::Apply(sphere.GetPrim());
    pxr::UsdPhysicsMassAPI::Apply(sphere.GetPrim()).CreateMassAttr().Set(mass);
    const auto joint = pxr::UsdPhysicsJoint::Define(stage, pxr::SdfPath("/Joint"));
    joint.CreateBody1Rel().SetTargets({sphere.GetPath()});
    joint.GetPrim()
        .CreateAttribute(pxr::TfToken("mmd:physics:type"), pxr::SdfValueTypeNames->Token)
        .Set(pxr::TfToken("spring6Dof"));
    pxr::GfVec3f k(0);
    k[axis] = stiffness;
    Springs(joint.GetPrim(), k);
    const auto scene = usd_physics::usd::readPhysicsScene(stage);
    Check(scene.bodies.size() == 1 && scene.joints.size() == 1, "rotation response USD snapshot");
    auto world = usd_physics::jolt::createWorld();
    const auto& b = scene.bodies.front();
    const auto shape = world->createShape(b.shape);
    const auto body =
        world->createBody({shape, b.motionType, b.transform, b.mass, {1, 0}, {0, 0}, {0, 0}});
    auto d = *usd_physics::usd::toSixDofConstraint(scene.joints.front(), {}, body);
    for (int i = 0; i != 3; ++i)
        d.limits[i].motion = core::AxisMotion::locked;
    const auto count = mmd_physics_probe::ApplyRotationSprings(joint.GetPrim(), d);
    Check(count == (stiffness > 0 ? 1u : 0u), "source rotation spring activation count");
    Check(std::abs(d.rotationSprings[axis].stiffness - stiffness * .0064) < 1e-12,
          "rotation stiffness must scale by source length squared");
    auto* cap = dynamic_cast<core::SixDofConstraints*>(world.get());
    Check(cap != nullptr, "rotation spring capability");
    cap->createSixDofConstraint(d);
    double worst = 0;
    for (int i = 1; i <= hz / 2; ++i) {
        world->step(core::PhysicsWorld::Duration{1.0 / hz});
        const auto q = world->bodyState(body).transform.rotation;
        const double v[] = {q.x, q.y, q.z};
        const double theta = 2 * std::atan2(v[axis], q.w);
        // Expected response in SOURCE units: I=2*m*r^2/5, r=2.
        // A missing s^2 conversion changes the period by a factor of 12.5.
        const double expected = .1 * std::cos(std::sqrt(stiffness / (1.6 * mass)) * i / hz);
        worst = std::max(worst, std::abs(theta - expected) / .1);
        for (int other = 0; other != 3; ++other)
            Check(other == axis || std::abs(v[other]) < 1e-5, "source rotation axis isolation");
    }
    return worst;
}
} // namespace
int
main()
{
    try {
        const auto stage = pxr::UsdStage::CreateInMemory();
        auto prim = stage->DefinePrim(pxr::SdfPath("/Joint"));
        Springs(prim, {1, 2, -3});
        core::SixDofConstraintDescriptor d;
        Check(mmd_physics_probe::ApplyRotationSprings(prim, d) == 0, "absent source type");
        const auto type =
            prim.CreateAttribute(pxr::TfToken("mmd:physics:type"), pxr::SdfValueTypeNames->Token);
        for (const char* value : {"sixDof", "hinge", "unknown"}) {
            type.Set(pxr::TfToken(value));
            Check(mmd_physics_probe::ApplyRotationSprings(prim, d) == 0, "non-spring type");
        }
        type.Set(pxr::TfToken("spring6Dof"));
        d.limits[4].motion = core::AxisMotion::locked;
        Check(mmd_physics_probe::ApplyRotationSprings(prim, d) == 1 &&
                  d.rotationSprings[0].stiffness == .0064 && d.rotationSprings[1].stiffness == 0 &&
                  d.rotationSprings[2].stiffness == 0 && d.translationSprings[0].stiffness == 0,
              "locked/signed axes and translation remain unexecuted");
        for (const bool blocked : {false, true}) {
            if (blocked)
                type.Block();
            else
                type.Set(pxr::TfToken("spring6Dof"), pxr::UsdTimeCode(1));
            bool rejected = false;
            try {
                mmd_physics_probe::ApplyRotationSprings(prim, d);
            } catch (const std::runtime_error&) {
                rejected = true;
            }
            Check(rejected, "invalid source type must reject");
            type.Clear();
        }
        prim.RemoveProperty(type.GetName());
        prim.CreateAttribute(type.GetName(), pxr::SdfValueTypeNames->String)
            .Set(std::string("spring6Dof"));
        bool rejected = false;
        try {
            mmd_physics_probe::ApplyRotationSprings(prim, d);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        Check(rejected, "wrong source type must reject");
        prim.RemoveProperty(type.GetName());
        prim.CreateAttribute(type.GetName(), pxr::SdfValueTypeNames->Token)
            .Set(pxr::TfToken("spring6Dof"));
        std::string before, after;
        stage->GetRootLayer()->ExportToString(&before);
        mmd_physics_probe::ApplyRotationSprings(prim, d);
        stage->GetRootLayer()->ExportToString(&after);
        Check(before == after, "rotation conversion must not edit stage");
        for (const int hz : {60, 240}) {
            double worst = 0;
            for (int axis = 0; axis != 3; ++axis)
                for (float k : {0.f, 25.f, 100.f})
                    for (float mass : {1.f, 4.f})
                        worst = std::max(worst, Response(axis, hz, k, mass));
            std::cout << hz << " Hz source-to-SI rotation max normalized analytic error=" << worst
                      << '\n';
            Check(worst < (hz == 60 ? .2 : .06), "MMD rotation spring response");
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
