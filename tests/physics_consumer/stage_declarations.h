// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "bone_binding.h"
#include <pxr/usd/usdPhysics/joint.h>
#include <vector>

namespace mmd_physics_probe {

// Strict preservation validation for the canonical importer layout only.
// Conventional USD inputs retain their existing generic-reader behavior.
inline void
RequirePhysicsValue(const pxr::UsdPrim& prim, const char* name, pxr::SdfValueTypeName type)
{
    const auto attr = prim.GetAttribute(pxr::TfToken(std::string("mmd:physics:") + name));
    pxr::VtValue value;
    if (!attr || attr.GetTypeName() != type ||
        attr.GetVariability() != pxr::SdfVariabilityUniform || attr.GetNumTimeSamples() != 0 ||
        !attr.Get(&value) || value.IsEmpty())
        throw std::runtime_error(prim.GetPath().GetString() + ": mmd:physics:" + name +
                                 " requires a constant uniform " + type.GetAsToken().GetString());
}

inline void
ValidateJointReferences(const pxr::UsdPrim& joint, const std::vector<pxr::UsdPrim>& bodies)
{
    RequirePhysicsValue(joint, "type", pxr::SdfValueTypeNames->Token);
    pxr::TfToken type;
    joint.GetAttribute(pxr::TfToken("mmd:physics:type")).Get(&type);
    const std::string kind = type.GetString();
    if (kind != "spring6Dof" && kind != "sixDof" && kind != "pointToPoint" &&
        kind != "coneTwist" && kind != "slider" && kind != "hinge")
        throw std::runtime_error(joint.GetPath().GetString() + ": unknown mmd:physics:type");
    const bool standard = kind == "spring6Dof" || kind == "sixDof";
    if ((standard && joint.GetTypeName() != pxr::TfToken("PhysicsJoint")) ||
        (!standard && !joint.GetTypeName().IsEmpty()))
        throw std::runtime_error(joint.GetPath().GetString() + ": joint schema disagrees with mmd:physics:type");
    const pxr::UsdPhysicsJoint api(joint);
    int endpoint = 0;
    for (const char* name : {"rigidBodyA", "rigidBodyB"}) {
        RequirePhysicsValue(joint, name, pxr::SdfValueTypeNames->Int);
        int index = -1;
        joint.GetAttribute(pxr::TfToken(std::string("mmd:physics:") + name)).Get(&index);
        if (index < 0 || static_cast<std::size_t>(index) >= bodies.size())
            throw std::runtime_error(joint.GetPath().GetString() + ": mmd:physics:" + name +
                                     " does not resolve to a rigid body");
        if (standard) {
            pxr::SdfPathVector targets;
            (endpoint == 0 ? api.GetBody0Rel() : api.GetBody1Rel()).GetTargets(&targets);
            if (targets != pxr::SdfPathVector{bodies[index].GetPath()})
                throw std::runtime_error(joint.GetPath().GetString() + ": mmd:physics:" + name +
                                         " disagrees with the standard body relationship");
        }
        ++endpoint;
    }
}

inline void
ValidateStageDeclarations(const pxr::UsdStageRefPtr& stage)
{
    const auto scope = stage->GetPrimAtPath(pxr::SdfPath("/Asset/physics"));
    if (!scope) return;
    const auto rigidBodies = scope.GetChild(pxr::TfToken("rigidBodies"));
    if (!rigidBodies)
        throw std::runtime_error(scope.GetPath().GetString() + ": missing rigidBodies scope");
    std::vector<pxr::UsdPrim> bodies;
    const auto skeleton = pxr::UsdSkelSkeleton::Get(stage, pxr::SdfPath("/Asset/skel/Skeleton"));
    for (const auto& body : rigidBodies.GetChildren()) {
        bodies.push_back(body);
        ReadBoneBinding(body, skeleton, true);
        for (const char* name : {"collisionGroup", "collisionMask"})
            RequirePhysicsValue(body, name, pxr::SdfValueTypeNames->Int);
        for (const char* name : {"mass", "linearDamping", "angularDamping", "restitution", "friction"})
            RequirePhysicsValue(body, name, pxr::SdfValueTypeNames->Float);
        RequirePhysicsValue(body, "shape", pxr::SdfValueTypeNames->Token);
        RequirePhysicsValue(body, "size", pxr::SdfValueTypeNames->Float3);
    }
    const auto joints = scope.GetChild(pxr::TfToken("joints"));
    if (!joints) return;
    for (const auto& joint : joints.GetChildren()) {
        ValidateJointReferences(joint, bodies);
        RequirePhysicsValue(joint, "position", pxr::SdfValueTypeNames->Point3d);
        RequirePhysicsValue(joint, "orientation", pxr::SdfValueTypeNames->Quatf);
        for (const char* name : {"translationLowerLimit", "translationUpperLimit", "rotationLowerLimit",
                                 "rotationUpperLimit", "translationSpring", "rotationSpring"})
            RequirePhysicsValue(joint, name, pxr::SdfValueTypeNames->Vector3f);
    }
}

} // namespace mmd_physics_probe
