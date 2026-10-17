// SPDX-License-Identifier: Apache-2.0
// Observe the installed generic reader against unmodified importer stages.
// A reported rejection is evidence of a gap, not successful MMD simulation.
#include <usd_physics/usd/box_scene.h>

#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/cube.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdPhysics/collisionAPI.h>
#include <pxr/usd/usdPhysics/massAPI.h>
#include <pxr/usd/usdPhysics/rigidBodyAPI.h>

#include <iostream>
#include <cmath>
#include <stdexcept>

namespace {
const char*
CodeName(usd_physics::usd::ImportErrorCode code)
{
    using Code = usd_physics::usd::ImportErrorCode;
    switch (code) {
    case Code::invalidStage:
        return "invalidStage";
    case Code::unsupportedUnits:
        return "unsupportedUnits";
    case Code::unsupportedDeclaration:
        return "unsupportedDeclaration";
    case Code::unsupportedTransform:
        return "unsupportedTransform";
    case Code::invalidValue:
        return "invalidValue";
    }
    throw std::runtime_error("unknown physicsUsd error code");
}

void
CheckControl()
{
    const auto stage = pxr::UsdStage::CreateInMemory();
    pxr::UsdGeomSetStageUpAxis(stage, pxr::UsdGeomTokens->y);
    pxr::UsdGeomSetStageMetersPerUnit(stage, 1.0);
    const auto cube = pxr::UsdGeomCube::Define(stage, pxr::SdfPath("/Box"));
    cube.AddTranslateOp().Set(pxr::GfVec3d(1.0, 2.0, 3.0));
    pxr::UsdPhysicsCollisionAPI::Apply(cube.GetPrim());
    pxr::UsdPhysicsRigidBodyAPI::Apply(cube.GetPrim());
    pxr::UsdPhysicsMassAPI::Apply(cube.GetPrim()).CreateMassAttr().Set(2.0f);
    const auto bodies = usd_physics::usd::readBoxScene(stage);
    if (bodies.size() != 1 || bodies[0].path != "/Box" ||
        bodies[0].motionType != usd_physics::core::MotionType::dynamicBody ||
        bodies[0].mass != 2.0 || bodies[0].shape.halfExtents.x != 1.0 ||
        bodies[0].transform.translation.y != 2.0) {
        throw std::runtime_error("installed Box reader failed the positive control");
    }
    std::cout << "control\taccepted\t1\n";

    const auto childStage = pxr::UsdStage::CreateInMemory();
    pxr::UsdGeomSetStageUpAxis(childStage, pxr::UsdGeomTokens->y);
    pxr::UsdGeomSetStageMetersPerUnit(childStage, 1.0);
    const auto parent = pxr::UsdGeomXform::Define(childStage, pxr::SdfPath("/World"));
    parent.AddTranslateOp().Set(pxr::GfVec3d(10, 0, 0));
    parent.AddRotateZOp().Set(90.0f);
    const auto body = pxr::UsdGeomXform::Define(childStage, pxr::SdfPath("/World/Body"));
    body.AddTranslateOp().Set(pxr::GfVec3d(2, 0, 0));
    pxr::UsdPhysicsRigidBodyAPI::Apply(body.GetPrim());
    pxr::UsdPhysicsMassAPI::Apply(body.GetPrim()).CreateMassAttr().Set(3.0f);
    const auto collider =
        pxr::UsdGeomCube::Define(childStage, pxr::SdfPath("/World/Body/collider"));
    collider.AddScaleOp().Set(pxr::GfVec3f(0.5f, 1, 2));
    pxr::UsdPhysicsCollisionAPI::Apply(collider.GetPrim());
    const auto children = usd_physics::usd::readBoxScene(childStage);
    if (children.size() != 1 || children[0].path != "/World/Body" ||
        children[0].motionType != usd_physics::core::MotionType::dynamicBody ||
        children[0].mass != 3.0 || children[0].shape.halfExtents.x != 0.5 ||
        children[0].shape.halfExtents.z != 2.0 ||
        std::abs(children[0].transform.translation.x - 10) > 1e-9 ||
        std::abs(children[0].transform.translation.y - 2) > 1e-9 ||
        std::abs(children[0].transform.rotation.z - std::sqrt(0.5)) > 1e-9) {
        throw std::runtime_error("installed reader failed the child Box control");
    }
    std::cout << "control-child-box\taccepted\t1\n";
}
} // namespace

int
main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: mmd_physics_consumer_probe STAGE [STAGE ...]\n";
        return 2;
    }
    try {
        CheckControl();
        for (int i = 1; i < argc; ++i) {
            const auto stage = pxr::UsdStage::Open(argv[i]);
            if (!stage)
                throw std::runtime_error("could not open input stage");
            std::size_t rigidBodies = 0;
            for (const auto& prim : stage->Traverse()) {
                if (prim.HasAPI<pxr::UsdPhysicsRigidBodyAPI>())
                    ++rigidBodies;
            }
            std::cout << argv[i] << "\trigidBodies=" << rigidBodies << '\t';
            try {
                const auto snapshot = usd_physics::usd::readBoxScene(stage);
                std::cout << "accepted\t" << snapshot.size() << '\n';
            } catch (const usd_physics::usd::ImportError& error) {
                std::cout << "rejected\t" << CodeName(error.code()) << '\t' << error.path() << '\t'
                          << error.what() << '\n';
            }
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
