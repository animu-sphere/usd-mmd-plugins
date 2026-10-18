// SPDX-License-Identifier: Apache-2.0
#include "translation_springs.h"
#include <usd_physics/usd/box_scene.h>
#include <usd_physics/usd/joint_conversion.h>
#include <usd_physics/jolt/availability.h>
#include <pxr/usd/usdGeom/cube.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdPhysics/collisionAPI.h>
#include <pxr/usd/usdPhysics/massAPI.h>
#include <pxr/usd/usdPhysics/rigidBodyAPI.h>
#include <pxr/usd/usdPhysics/joint.h>
#include <algorithm>
#include <iostream>

namespace core = usd_physics::core;
namespace {
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void Springs(const pxr::UsdPrim& prim, const pxr::GfVec3f& k) {
    prim.CreateAttribute(pxr::TfToken("mmd:physics:translationSpring"),
                         pxr::SdfValueTypeNames->Vector3f).Set(k);
    prim.CreateAttribute(pxr::TfToken("mmd:physics:rotationSpring"),
                         pxr::SdfValueTypeNames->Vector3f).Set(pxr::GfVec3f(5,6,7));
}
double Response(int axis, int hz, float stiffness) {
    const auto stage = pxr::UsdStage::CreateInMemory();
    pxr::UsdGeomSetStageUpAxis(stage,pxr::UsdGeomTokens->y);
    pxr::UsdGeomSetStageMetersPerUnit(stage,1);
    const auto cube = pxr::UsdGeomCube::Define(stage,pxr::SdfPath("/Body"));
    pxr::GfVec3d initial(0); initial[axis] = .08; // One source length unit in SI.
    cube.AddTranslateOp().Set(initial);
    pxr::UsdPhysicsCollisionAPI::Apply(cube.GetPrim());
    pxr::UsdPhysicsRigidBodyAPI::Apply(cube.GetPrim());
    pxr::UsdPhysicsMassAPI::Apply(cube.GetPrim()).CreateMassAttr().Set(1.f);
    const auto joint = pxr::UsdPhysicsJoint::Define(stage,pxr::SdfPath("/Joint"));
    joint.CreateBody1Rel().SetTargets({cube.GetPath()});
    joint.GetPrim().CreateAttribute(pxr::TfToken("mmd:physics:type"),
                                   pxr::SdfValueTypeNames->Token).Set(pxr::TfToken("spring6Dof"));
    pxr::GfVec3f k(0); k[axis] = stiffness; Springs(joint.GetPrim(),k);
    const auto scene = usd_physics::usd::readPhysicsScene(stage);
    Check(scene.bodies.size()==1 && scene.joints.size()==1,"USD spring response snapshot");
    auto world = usd_physics::jolt::createWorld();
    const auto& b = scene.bodies.front();
    const auto shape = world->createShape(b.shape);
    const auto body = world->createBody({shape,b.motionType,b.transform,b.mass,{1,0},{0,0},{0,0}});
    auto descriptor = *usd_physics::usd::toSixDofConstraint(scene.joints.front(),{},body);
    const auto count = mmd_physics_probe::ApplyTranslationSprings(joint.GetPrim(),descriptor);
    Check(count == (stiffness>0 ? 1u : 0u),"source spring activation count");
    Check(descriptor.translationSprings[axis].stiffness == stiffness,
          "translation stiffness must not be multiplied by source length scale");
    auto* capability = dynamic_cast<core::SixDofConstraints*>(world.get());
    Check(capability!=nullptr,"spring capability");
    capability->createSixDofConstraint(descriptor);
    double worst=0;
    for (int i=1; i<=hz/2; ++i) {
        world->applyForce(body,{0,9.81,0});
        world->step(core::PhysicsWorld::Duration{1.0/hz});
        const auto p=world->bodyState(body).transform.translation;
        const double coordinates[]={p.x,p.y,p.z};
        const double expected=.08*std::cos(std::sqrt(stiffness)*i/hz);
        worst=std::max(worst,std::abs(coordinates[axis]-expected)/.08);
        for (int other=0; other!=3; ++other)
            Check(other==axis || std::abs(coordinates[other])<1e-5,"source spring axis isolation");
    }
    return worst;
}
}
int main() {
    try {
        const auto stage=pxr::UsdStage::CreateInMemory();
        const auto prim=stage->DefinePrim(pxr::SdfPath("/Joint"));
        Springs(prim,{1,2,-3});
        core::SixDofConstraintDescriptor d;
        Check(mmd_physics_probe::ApplyTranslationSprings(prim,d)==0,"absent type remains unsupported");
        const auto type=prim.CreateAttribute(pxr::TfToken("mmd:physics:type"),pxr::SdfValueTypeNames->Token);
        for (const char* value : {"sixDof","hinge","unknown"}) {
            type.Set(pxr::TfToken(value));
            Check(mmd_physics_probe::ApplyTranslationSprings(prim,d)==0,"non-spring source type");
        }
        type.Set(pxr::TfToken("spring6Dof"));
        d.limits[1].motion=core::AxisMotion::locked;
        Check(mmd_physics_probe::ApplyTranslationSprings(prim,d)==1 &&
              d.translationSprings[0].stiffness==1 && d.translationSprings[1].stiffness==0 &&
              d.translationSprings[2].stiffness==0,"signed and locked axes stay unsupported");
        for (const bool blocked : {false,true}) {
            if (blocked) type.Block();
            else type.Set(pxr::TfToken("spring6Dof"),pxr::UsdTimeCode(1));
            bool rejected=false;
            try { mmd_physics_probe::ApplyTranslationSprings(prim,d); }
            catch (const std::runtime_error&) { rejected=true; }
            Check(rejected,"invalid source type must reject");
            type.Clear();
        }
        type.Set(pxr::TfToken("spring6Dof"));
        std::string before,after;
        stage->GetRootLayer()->ExportToString(&before);
        mmd_physics_probe::ApplyTranslationSprings(prim,d);
        stage->GetRootLayer()->ExportToString(&after);
        Check(before==after,"spring conversion must not edit the stage");
        for (const int hz : {60,240}) {
            double worst=0;
            for (int axis=0; axis!=3; ++axis)
                for (const float k : {0.f,25.f,100.f})
                    worst=std::max(worst,Response(axis,hz,k));
            std::cout << hz << " Hz source-to-SI max normalized analytic error=" << worst << '\n';
            Check(worst < (hz==60 ? .24 : .07),"MMD translation spring response");
        }
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
