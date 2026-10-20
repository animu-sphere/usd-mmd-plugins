// SPDX-License-Identifier: Apache-2.0
// Observe the installed generic reader against unmodified importer stages.
// A reported rejection is evidence of a gap, not successful MMD simulation.
#include <usd_physics/usd/box_scene.h>
#include "collision_filter.h"
#include "contact_material.h"
#include "body_damping.h"
#include "bone_binding.h"
#include "stage_declarations.h"
#include "joint_springs.h"
#ifdef MMD_PHYSICS_ROTATION_SPRINGS
#include "rotation_springs.h"
#endif
#ifdef MMD_PHYSICS_TRANSLATION_SPRINGS
#include "translation_springs.h"
#endif
#ifdef MMD_PHYSICS_EXECUTION
#include <usd_physics/usd/joint_conversion.h>
#include <usd_physics/jolt/availability.h>
#include <map>
#endif

#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/cube.h>
#include <pxr/usd/usdGeom/sphere.h>
#include <pxr/usd/usdGeom/capsule.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdPhysics/collisionAPI.h>
#include <pxr/usd/usdPhysics/massAPI.h>
#include <pxr/usd/usdPhysics/rigidBodyAPI.h>
#include <pxr/usd/usdPhysics/joint.h>
#include <pxr/usd/usdPhysics/limitAPI.h>

#include <iostream>
#include <cmath>
#include <stdexcept>

namespace {
#ifdef MMD_PHYSICS_EXECUTION
struct ExecutionResult {
    std::size_t constraints{0}, translationSpringAxes{0}, rotationSpringAxes{0};
    std::vector<usd_physics::core::BodyState> trajectory;
};
ExecutionResult
Execute(const usd_physics::usd::PhysicsScene& scene,
        const pxr::UsdStageRefPtr& stage,
        const std::vector<usd_physics::core::CollisionFilter>& filters,
        const std::vector<usd_physics::core::ContactMaterial>& materials,
        const std::vector<usd_physics::core::BodyDamping>& damping,
        bool reportApproximations = true)
{
    namespace core = usd_physics::core;
    auto world = usd_physics::jolt::createWorld(mmd_physics_probe::ContactMixing());
    auto* constraints = dynamic_cast<core::SixDofConstraints*>(world.get());
    if (!constraints)
        throw std::runtime_error("installed backend lacks six-DOF capability");
    std::map<std::string, core::BodyHandle> bodies;
    for (std::size_t i = 0; i != scene.bodies.size(); ++i) {
        const auto& body = scene.bodies[i];
        const auto shape = world->createShape(body.shape);
        bodies.emplace(
            body.path,
            world->createBody({shape, body.motionType, body.transform, body.mass,
                               filters.at(i), materials.at(i), damping.at(i)}));
    }
    ExecutionResult result;
    for (const auto& joint : scene.joints) {
        const auto endpoint = [&](const std::string& path) {
            return path.empty() ? core::BodyHandle{} : bodies.at(path);
        };
        auto descriptor = usd_physics::usd::toSixDofConstraint(
            joint, endpoint(joint.body0), endpoint(joint.body1));
        if (descriptor) {
#ifdef MMD_PHYSICS_ROTATION_SPRINGS
            const auto rotationAxes = mmd_physics_probe::ApplyRotationSprings(
                stage->GetPrimAtPath(pxr::SdfPath(joint.path)), *descriptor);
            result.rotationSpringAxes += rotationAxes;
            if (rotationAxes && reportApproximations)
                std::cerr << stage->GetRootLayer()->GetIdentifier() << '\t' << joint.path
                          << "\tapproximateRotationSprings\taxes=" << rotationAxes
                          << "\tstiffness scaled by 0.08^2; small-angle motor; source response "
                             "unverified\n";
#endif
#ifdef MMD_PHYSICS_TRANSLATION_SPRINGS
            const auto applied = mmd_physics_probe::ApplyTranslationSprings(
                stage->GetPrimAtPath(pxr::SdfPath(joint.path)), *descriptor);
            result.translationSpringAxes += applied;
            if (applied && reportApproximations)
                std::cerr << stage->GetRootLayer()->GetIdentifier() << '\t' << joint.path
                          << "\tapproximateTranslationSprings\taxes=" << applied
                          << "\tHookean SI stiffness; zero viscous damping; source response unverified\n";
#endif
            constraints->createSixDofConstraint(*descriptor);
            ++result.constraints;
        }
    }
    for (int frame = 0; frame != 120; ++frame) {
        world->step(core::PhysicsWorld::Duration{1.0 / 120.0});
        for (const auto& entry : bodies) {
            const auto state = world->bodyState(entry.second);
            const auto p = state.transform.translation;
            const auto q = state.transform.rotation;
            const auto v = state.linearVelocity;
            for (const double value : {p.x,p.y,p.z,q.w,q.x,q.y,q.z,v.x,v.y,v.z})
                if (!std::isfinite(value)) throw std::runtime_error("non-finite runtime state");
            result.trajectory.push_back(state);
        }
        world->takeChangedBodyStates();
    }
    return result;
}
#endif
pxr::GfMatrix4d
Matrix(const usd_physics::core::Transform& pose)
{
    pxr::GfMatrix4d matrix(1.0);
    const auto q = pose.rotation;
    matrix.SetRotate(pxr::GfQuatd(q.w, q.x, q.y, q.z));
    const auto p = pose.translation;
    matrix.SetTranslateOnly(pxr::GfVec3d(p.x, p.y, p.z));
    return matrix;
}

// Imported PMX joint frames describe one common rest frame. Reconstruct it
// solely from the installed reader's neutral snapshot, never collider frames.
void
CheckPmxJointFrames(const usd_physics::usd::PhysicsScene& scene)
{
    for (const auto& joint : scene.joints) {
        const auto world = [&](const std::string& path, const usd_physics::core::Transform& local) {
            for (const auto& body : scene.bodies) {
                if (body.path == path)
                    return Matrix(local) * Matrix(body.transform);
            }
            throw std::runtime_error("PMX joint endpoint has no body snapshot");
        };
        const auto a = world(joint.body0, joint.localFrame0);
        const auto b = world(joint.body1, joint.localFrame1);
        for (int row = 0; row != 4; ++row) {
            for (int col = 0; col != 4; ++col) {
                if (std::abs(a[row][col] - b[row][col]) > 1e-6)
                    throw std::runtime_error("PMX joint local frames disagree in world space");
            }
        }
    }
}

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

    collider.ClearXformOpOrder();
    collider.AddTranslateOp().Set(pxr::GfVec3d(1, 2, 3));
    collider.AddRotateZOp().Set(90.0f);
    collider.AddScaleOp().Set(pxr::GfVec3f(0.5f, 1, 2));
    const auto offset = usd_physics::usd::readBoxScene(childStage);
    if (offset.size() != 1 || offset[0].path != children[0].path ||
        offset[0].transform != children[0].transform ||
        std::abs(offset[0].shape.halfExtents.x - children[0].shape.halfExtents.x) > 1e-9 ||
        std::abs(offset[0].shape.halfExtents.y - children[0].shape.halfExtents.y) > 1e-9 ||
        std::abs(offset[0].shape.halfExtents.z - children[0].shape.halfExtents.z) > 1e-9 ||
        offset[0].shape.localTransform.translation != usd_physics::core::Vector3{1, 2, 3} ||
        std::abs(offset[0].shape.localTransform.rotation.z - std::sqrt(0.5)) > 1e-9) {
        throw std::runtime_error("installed reader failed the local collider pose control");
    }
    std::cout << "control-offset-box\taccepted\t1\n";

    pxr::UsdPhysicsMassAPI(body.GetPrim()).CreateMassAttr().Set(0.0f);
    if (std::abs(usd_physics::usd::readBoxScene(childStage)[0].mass - 8000.0) > 1e-8)
        throw std::runtime_error("installed reader failed inferred child Box mass");
    std::cout << "control-inferred-box\taccepted\tmass=8000\n";
    pxr::UsdPhysicsMassAPI(body.GetPrim()).CreateMassAttr().Set(3.0f);

    for (const bool capsule : {false, true}) {
        const auto prim = collider.GetPrim();
        prim.SetTypeName(pxr::TfToken(capsule ? "Capsule" : "Sphere"));
        prim.GetAttribute(pxr::TfToken("xformOp:scale")).Set(pxr::GfVec3f(2));
        if (capsule) {
            const pxr::UsdGeomCapsule geometry(prim);
            geometry.CreateRadiusAttr().Set(0.25);
            geometry.CreateHeightAttr().Set(1.5);
            geometry.CreateAxisAttr().Set(pxr::UsdGeomTokens->x);
        } else {
            pxr::UsdGeomSphere(prim).CreateRadiusAttr().Set(0.25);
        }
        const auto rounded = usd_physics::usd::readPrimitiveScene(childStage);
        if (rounded.size() != 1 || rounded[0].path != children[0].path ||
            rounded[0].transform != children[0].transform || rounded[0].mass != 3 ||
            rounded[0].shape.type != (capsule ? usd_physics::core::ShapeType::capsule
                                              : usd_physics::core::ShapeType::sphere) ||
            std::abs(rounded[0].shape.radius - 0.5) > 1e-9 ||
            (capsule && std::abs(rounded[0].shape.halfHeight - 1.5) > 1e-9) ||
            rounded[0].shape.localTransform.translation != usd_physics::core::Vector3{1, 2, 3}) {
            throw std::runtime_error("installed reader failed rounded geometry control");
        }
        std::cout << (capsule ? "control-capsule" : "control-sphere") << "\taccepted\t1\n";
        pxr::UsdPhysicsMassAPI(body.GetPrim()).CreateMassAttr().Set(0.0f);
        const auto inferred = usd_physics::usd::readPrimitiveScene(childStage);
        const double expected = 1000.0 * std::acos(-1.0) * (capsule ? 11.0 / 12.0 : 1.0 / 6.0);
        if (std::abs(inferred[0].mass - expected) > expected * 1e-12)
            throw std::runtime_error("installed reader failed inferred rounded mass");
        std::cout << (capsule ? "control-inferred-capsule" : "control-inferred-sphere")
                  << "\taccepted\tmass=" << inferred[0].mass << '\n';
        pxr::UsdPhysicsMassAPI(body.GetPrim()).CreateMassAttr().Set(3.0f);
    }
    pxr::UsdPhysicsRigidBodyAPI(body.GetPrim()).CreateKinematicEnabledAttr().Set(true);
    body.GetPrim().RemoveAPI<pxr::UsdPhysicsMassAPI>();
    body.GetPrim().RemoveProperty(pxr::TfToken("physics:mass"));
    for (const auto* type : {"Cube", "Sphere", "Capsule"}) {
        collider.GetPrim().SetTypeName(pxr::TfToken(type));
        const auto snapshot = usd_physics::usd::readPrimitiveScene(childStage);
        if (snapshot.size() != 1 || snapshot[0].path != children[0].path ||
            snapshot[0].motionType != usd_physics::core::MotionType::kinematicBody ||
            snapshot[0].transform != children[0].transform ||
            snapshot[0].shape.localTransform.translation != usd_physics::core::Vector3{1, 2, 3}) {
            throw std::runtime_error("installed reader failed kinematic child control");
        }
        std::cout << "control-kinematic-" << type << "\taccepted\t1\n";
    }
    const auto other = pxr::UsdGeomCube::Define(childStage, pxr::SdfPath("/Other"));
    pxr::UsdPhysicsCollisionAPI::Apply(other.GetPrim());
    pxr::UsdPhysicsRigidBodyAPI::Apply(other.GetPrim());
    pxr::UsdPhysicsMassAPI::Apply(other.GetPrim()).CreateMassAttr().Set(1.0f);
    const auto joint = pxr::UsdPhysicsJoint::Define(childStage, pxr::SdfPath("/Joint"));
    joint.CreateBody0Rel().SetTargets({body.GetPath()});
    joint.CreateBody1Rel().SetTargets({other.GetPath()});
    joint.CreateLocalPos0Attr().Set(pxr::GfVec3f(1, 2, 3));
    joint.CreateLocalPos1Attr().Set(pxr::GfVec3f(-1, -2, -3));
    const auto angular = pxr::UsdPhysicsLimitAPI::Apply(joint.GetPrim(), pxr::TfToken("rotZ"));
    angular.CreateLowAttr().Set(-90.0f);
    angular.CreateHighAttr().Set(90.0f);
    const auto locked = pxr::UsdPhysicsLimitAPI::Apply(joint.GetPrim(), pxr::TfToken("transX"));
    locked.CreateLowAttr().Set(0.25f);
    locked.CreateHighAttr().Set(0.25f);
    const auto scene = usd_physics::usd::readPhysicsScene(childStage);
    if (scene.bodies.size() != 2 || scene.joints.size() != 1 ||
        scene.joints[0].body0 != "/World/Body" || scene.joints[0].body1 != "/Other" ||
        scene.joints[0].localFrame0.translation != usd_physics::core::Vector3{1, 2, 3} ||
        scene.joints[0].localFrame1.translation != usd_physics::core::Vector3{-1, -2, -3} ||
        scene.joints[0].limits[1].applied || !scene.joints[0].limits[0].applied ||
        scene.joints[0].limits[0].low != 0.25 || scene.joints[0].limits[0].high != 0.25 ||
        std::abs(scene.joints[0].limits[5].low + std::acos(-1.0) / 2) > 1e-9 ||
        std::abs(scene.joints[0].limits[5].high - std::acos(-1.0) / 2) > 1e-9) {
        throw std::runtime_error("installed reader failed the joint frame/limit control");
    }
    std::cout << "control-joint\taccepted\tbodies=2\tjoints=1\n";
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
        int rejectedStages = 0;
        for (int i = 1; i < argc; ++i) {
            const auto stage = pxr::UsdStage::Open(argv[i]);
            if (!stage)
                throw std::runtime_error("could not open input stage");
            std::size_t rigidBodies = 0;
            std::size_t joints = 0, preservedOnlyJoints = 0;
            std::size_t springJoints = 0, springAxes = 0;
            for (const auto& prim : stage->Traverse()) {
                if (prim.HasAPI<pxr::UsdPhysicsRigidBodyAPI>())
                    ++rigidBodies;
                if (prim.IsA<pxr::UsdPhysicsJoint>())
                    ++joints;
                else if (prim.HasAttribute(pxr::TfToken("mmd:physics:type")))
                    ++preservedOnlyJoints;
            }
            std::cout << argv[i] << "\trigidBodies=" << rigidBodies << "\tjoints=" << joints
                      << "\tpreservedOnlyJoints=" << preservedOnlyJoints << '\t';
            try {
                mmd_physics_probe::ValidateStageDeclarations(stage);
                const auto snapshot = usd_physics::usd::readPhysicsScene(stage);
                if (snapshot.bodies.size() != rigidBodies || snapshot.joints.size() != joints)
                    throw std::runtime_error(
                        "PMX standard declaration counts differ from snapshot");
                CheckPmxJointFrames(snapshot);
                for (const auto& prim : stage->Traverse()) {
                    const auto springs = mmd_physics_probe::ReadJointSprings(prim);
                    if (springs.NonzeroAxes() != 0) {
                        ++springJoints;
                        springAxes += springs.NonzeroAxes();
                        std::cerr << argv[i] << '\t' << prim.GetPath()
                                  << "\tunsupportedJointSprings\ttranslation="
                                  << springs.translation << "\trotation=" << springs.rotation
                                  << "\tsource fidelity unsupported; experimental spring "
                                     "execution, if enabled, is approximate\n";
                    }
                }
                std::vector<usd_physics::core::CollisionFilter> filters;
                std::vector<usd_physics::core::ContactMaterial> materials;
                std::vector<usd_physics::core::BodyDamping> damping;
                for (const auto& body : snapshot.bodies) {
                    const auto prim = stage->GetPrimAtPath(pxr::SdfPath(body.path));
                    const auto binding = mmd_physics_probe::ReadBoneBinding(
                        prim, pxr::UsdSkelSkeleton::Get(stage, pxr::SdfPath("/Asset/skel/Skeleton")),
                        prim.GetPath().HasPrefix(pxr::SdfPath("/Asset/physics/rigidBodies")));
                    if (binding && binding->bone == -1)
                        std::cerr << argv[i] << '\t' << prim.GetPath()
                                  << "\tunboundBone\tbone=-1; no bone synchronization or feedback\n";
                    filters.push_back(mmd_physics_probe::ReadCollisionFilter(
                        stage->GetPrimAtPath(pxr::SdfPath(body.path))));
                    materials.push_back(mmd_physics_probe::ReadContactMaterial(
                        stage->GetPrimAtPath(pxr::SdfPath(body.path))));
                    damping.push_back(mmd_physics_probe::ReadBodyDamping(
                        stage->GetPrimAtPath(pxr::SdfPath(body.path))));
                }
#ifdef MMD_PHYSICS_EXECUTION
                const auto constraints = Execute(snapshot, stage, filters, materials, damping);
                // A fresh world must reproduce every exposed state, not just
                // finish with the same body counts or a finite final pose.
                const auto replay = Execute(snapshot, stage, filters, materials, damping, false);
                if (constraints.trajectory.size() != replay.trajectory.size())
                    throw std::runtime_error("runtime replay state count differs");
                for (std::size_t sample = 0; sample != constraints.trajectory.size(); ++sample) {
                    const auto& a = constraints.trajectory[sample];
                    const auto& b = replay.trajectory[sample];
                    if (a.transform != b.transform || a.linearVelocity != b.linearVelocity)
                        throw std::runtime_error("runtime replay state differs");
                }
#endif
                std::cout << "accepted\tbodies=" << snapshot.bodies.size()
                          << "\tjoints=" << snapshot.joints.size()
                          << "\tfilters=" << filters.size()
                          << "\tmaterials=" << materials.size()
                          << "\tdamping=" << damping.size()
                          << "\tunsupportedSpringJoints=" << springJoints
                          << "\tunsupportedSpringAxes=" << springAxes
                          << "\tjointFidelity="
                          << ((springJoints || preservedOnlyJoints) ? "approximate" : "unverified");
#ifdef MMD_PHYSICS_EXECUTION
                std::cout << "\tconstraints=" << constraints.constraints << "\tsteps=120"
                          << "\treplay=identical\treplaySamples=" << constraints.trajectory.size();
#endif
#ifdef MMD_PHYSICS_TRANSLATION_SPRINGS
                std::cout << "\ttranslationSpringModel=experimentalHookean"
                          << "\tapproximateTranslationSpringAxes=" << constraints.translationSpringAxes;
#endif
#ifdef MMD_PHYSICS_ROTATION_SPRINGS
                std::cout << "\trotationSpringModel=experimentalSmallAngle"
                          << "\tapproximateRotationSpringAxes=" << constraints.rotationSpringAxes;
#endif
                std::cout << '\n';
            } catch (const usd_physics::usd::ImportError& error) {
                ++rejectedStages;
                std::cout << "rejected\t" << CodeName(error.code()) << '\t' << error.path() << '\t'
                          << error.what() << '\n';
            } catch (const std::runtime_error& error) {
                ++rejectedStages;
                std::cout << "rejected\tconsumerError\t" << error.what() << '\n';
            }
        }
        if (rejectedStages != 0) return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
