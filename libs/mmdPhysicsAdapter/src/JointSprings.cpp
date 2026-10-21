// SPDX-License-Identifier: Apache-2.0
#include <mmdPhysicsAdapter/JointSprings.h>
#include <mmdPhysicsAdapter/Codes.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usd/attribute.h>
#include <cmath>

namespace mmd::physics {
namespace {
using namespace pxr;
bool Constant(const UsdAttribute& a, const SdfValueTypeName& type)
{
    return a && a.GetTypeName() == type && a.GetVariability() == SdfVariabilityUniform &&
           a.GetNumTimeSamples() == 0;
}
} // namespace

Result<std::optional<JointSprings>> ReadJointSprings(const pxr::UsdPrim& joint)
{
    using R = Result<std::optional<JointSprings>>;
    const auto fail = [&](const char* message) {
        return R::Failure(MakeDiagnostic(codes::PhysicsMetadataInvalid,
            (joint ? joint.GetPath().GetString() : "<invalid prim>") + ": " + message));
    };
    if (!joint)
        return fail("Joint springs require a valid prim");
    JointSprings result;
    const auto type = joint.GetAttribute(TfToken("mmd:physics:type"));
    if (type) {
        TfToken value;
        if (!Constant(type, SdfValueTypeNames->Token) || !type.Get(&value))
            return fail("MMD joint type requires a uniform unsampled token");
        result.spring6Dof = value == TfToken("spring6Dof");
    }
    const auto translation = joint.GetAttribute(TfToken("mmd:physics:translationSpring"));
    const auto rotation = joint.GetAttribute(TfToken("mmd:physics:rotationSpring"));
    if (!translation && !rotation)
        return R::Success(std::nullopt);
    GfVec3f t, r;
    if (!Constant(translation, SdfValueTypeNames->Vector3f) ||
        !Constant(rotation, SdfValueTypeNames->Vector3f) ||
        !translation.Get(&t) || !rotation.Get(&r))
        return fail("MMD joint springs require two uniform unsampled vector3f values");
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(t[axis]) || !std::isfinite(r[axis]))
            return fail("MMD joint spring coefficients must be finite");
        result.translation[axis] = t[axis];
        result.rotation[axis] = double(r[axis]) * (0.08 * 0.08);
    }
    return R::Success(result);
}
} // namespace mmd::physics
