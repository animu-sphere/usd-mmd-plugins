// SPDX-License-Identifier: Apache-2.0
#include <mmdPhysicsAdapter/JointAdmission.h>
#include <mmdPhysicsAdapter/Codes.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdPhysics/joint.h>
#include <pxr/usd/usdPhysics/rigidBodyAPI.h>
#include <pxr/usd/sdf/types.h>

namespace mmd::physics {
Result<JointAdmission> ReadJointAdmission(const pxr::UsdPrim& prim)
{
    using namespace pxr;
    using R = Result<JointAdmission>;
    const auto fail = [&](const std::string& message) {
        return R::Failure(MakeDiagnostic(codes::PhysicsBindingInvalid,
            (prim ? prim.GetPath().GetString() : "<invalid prim>") + ": " + message));
    };
    const auto constant = [](const UsdAttribute& a, const SdfValueTypeName& type) {
        return a && a.GetTypeName() == type && a.GetNumTimeSamples() == 0;
    };
    if (!prim || !prim.IsA<UsdPhysicsJoint>()) return fail("Expected a standard MMD joint");
    TfToken type;
    const auto typeAttr = prim.GetAttribute(TfToken("mmd:physics:type"));
    if (!typeAttr) return R::Success(JointAdmission::Execute);
    if (!constant(typeAttr, SdfValueTypeNames->Token) ||
        typeAttr.GetVariability() != SdfVariabilityUniform || !typeAttr.Get(&type))
        return fail("Joint admission requires uniform unsampled type metadata");
    if (type != TfToken("spring6Dof")) return R::Success(JointAdmission::Execute);
    const UsdPhysicsJoint joint(prim);
    bool inert = true;
    for (const auto& rel : {joint.GetBody0Rel(), joint.GetBody1Rel()}) {
        SdfPathVector targets;
        if (!rel.GetForwardedTargets(&targets) || targets.size() != 1 || !targets[0].IsPrimPath())
            return fail("Joint admission requires two MMD body endpoints");
        const auto body = prim.GetStage()->GetPrimAtPath(targets[0]);
        if (!body || !body.HasAPI<UsdPhysicsRigidBodyAPI>())
            return fail("Invalid MMD body endpoint " + targets[0].GetString());
        const UsdPhysicsRigidBodyAPI api(body);
        const auto modeAttr = body.GetAttribute(TfToken("mmd:physics:mode"));
        TfToken mode;
        bool kinematic = false, enabled = false;
        if (!constant(modeAttr, SdfValueTypeNames->Token) ||
            modeAttr.GetVariability() != SdfVariabilityUniform || !modeAttr.Get(&mode) ||
            (mode != TfToken("followBone") && mode != TfToken("dynamic") &&
             mode != TfToken("dynamicWithBone")) ||
            !constant(api.GetKinematicEnabledAttr(), SdfValueTypeNames->Bool) ||
            !api.GetKinematicEnabledAttr().Get(&kinematic) || kinematic != (mode == TfToken("followBone")) ||
            !constant(api.GetRigidBodyEnabledAttr(), SdfValueTypeNames->Bool) ||
            !api.GetRigidBodyEnabledAttr().Get(&enabled) || !enabled)
            return fail("Invalid or inconsistent MMD body mode at " + targets[0].GetString());
        inert = inert && kinematic;
    }
    return R::Success(inert ? JointAdmission::Inert : JointAdmission::Execute);
}
} // namespace mmd::physics
