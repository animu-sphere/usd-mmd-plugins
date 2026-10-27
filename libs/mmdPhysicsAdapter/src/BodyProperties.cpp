// SPDX-License-Identifier: Apache-2.0
#include <mmdPhysicsAdapter/BodyProperties.h>
#include <mmdPhysicsAdapter/Codes.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usd/attribute.h>
#include <cmath>
#include <algorithm>

namespace mmd::physics {
namespace {
using namespace pxr;
bool
Constant(const UsdAttribute& attr, const SdfValueTypeName& type)
{
    return attr && attr.GetTypeName() == type && attr.GetVariability() == SdfVariabilityUniform &&
           attr.GetNumTimeSamples() == 0;
}
} // namespace

Result<SourceDampingResponse>
ResolveSourceDamping(float coefficient)
{
    if (!std::isfinite(coefficient))
        return Result<SourceDampingResponse>::Failure(MakeDiagnostic(
            codes::PhysicsMetadataInvalid, "MMD source damping must be finite"));
    SourceDampingResponse response;
    response.coefficient = std::clamp(coefficient, 0.f, 1.f);
    response.clamped = response.coefficient != coefficient;
    if (response.coefficient == 1.f)
        response.mode = SourceDampingMode::ClearPriorVelocity;
    else
        response.rate = -std::log1p(-double(response.coefficient));
    return Result<SourceDampingResponse>::Success(response);
}

Result<BodyProperties>
ReadBodyProperties(const pxr::UsdPrim& body)
{
    const auto fail = [&](const Code& code, const char* message) {
        return Result<BodyProperties>::Failure(MakeDiagnostic(
            code, (body ? body.GetPath().GetString() : "<invalid prim>") + ": " + message));
    };
    if (!body)
        return fail(codes::PhysicsMetadataInvalid, "Body properties require a valid prim");
    BodyProperties properties;
    const auto group = body.GetAttribute(TfToken("mmd:physics:collisionGroup"));
    const auto mask = body.GetAttribute(TfToken("mmd:physics:collisionMask"));
    if (group || mask) {
        int g = -1, m = -1;
        if (!Constant(group, SdfValueTypeNames->Int) || !Constant(mask, SdfValueTypeNames->Int) ||
            !group.Get(&g) || !mask.Get(&m) || g < 0 || g > 15 || m < 0 || m > 65535)
            return fail(codes::PhysicsMetadataInvalid,
                        "MMD filtering requires uniform int group [0,15] and mask [0,65535]");
        properties.filter = BodyProperties::Filter{uint32_t{1} << g, static_cast<uint32_t>(m)};
    }
    const auto friction = body.GetAttribute(TfToken("mmd:physics:friction"));
    const auto restitution = body.GetAttribute(TfToken("mmd:physics:restitution"));
    if (friction || restitution) {
        float f = 0, r = 0;
        if (!Constant(friction, SdfValueTypeNames->Float) ||
            !Constant(restitution, SdfValueTypeNames->Float) || !friction.Get(&f) ||
            !restitution.Get(&r) || !std::isfinite(f) || !std::isfinite(r) || f < 0 || r < 0 ||
            r > 1)
            return fail(
                codes::PhysicsMetadataInvalid,
                "MMD contact requires uniform finite floats: friction >= 0, restitution [0,1]");
        properties.contact = BodyProperties::Contact{f, r};
    }
    const auto linear = body.GetAttribute(TfToken("mmd:physics:linearDamping"));
    const auto angular = body.GetAttribute(TfToken("mmd:physics:angularDamping"));
    if (linear || angular) {
        float l = 0, a = 0;
        if (!Constant(linear, SdfValueTypeNames->Float) ||
            !Constant(angular, SdfValueTypeNames->Float) || !linear.Get(&l) || !angular.Get(&a) ||
            !std::isfinite(l) || !std::isfinite(a) || l < 0 || a < 0 || l > 1 || a > 1)
            return fail(codes::PhysicsMetadataInvalid,
                        "MMD damping requires uniform finite float coefficients in [0,1]");
        if (l == 1 || a == 1)
            return fail(codes::PhysicsDampingUnsupported,
                        "MMD damping equal to one requires unsupported instantaneous stopping");
        properties.damping =
            BodyProperties::Damping{-std::log1p(-double(l)), -std::log1p(-double(a))};
    }
    return Result<BodyProperties>::Success(std::move(properties));
}
} // namespace mmd::physics
