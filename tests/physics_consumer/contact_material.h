// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <usd_physics/core/descriptors.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/prim.h>
#include <stdexcept>

namespace mmd_physics_probe {

// Source coefficients remain MMD metadata. Generic physicsUsd does not read it.
// Bullet's default non-negative pair coefficients: product, friction capped
// at 10. This reference policy does not establish native MMD trajectories.
inline usd_physics::core::ContactMixing
ContactMixing()
{
    using Rule = usd_physics::core::ContactCombineRule;
    return {Rule::product, Rule::product, 10};
}

inline usd_physics::core::ContactMaterial
ReadContactMaterial(const pxr::UsdPrim& body)
{
    const auto friction = body.GetAttribute(pxr::TfToken("mmd:physics:friction"));
    const auto restitution = body.GetAttribute(pxr::TfToken("mmd:physics:restitution"));
    if (!friction && !restitution)
        return {};
    float f = 0, r = 0;
    if (!friction || !restitution || friction.GetTypeName() != pxr::SdfValueTypeNames->Float ||
        restitution.GetTypeName() != pxr::SdfValueTypeNames->Float ||
        friction.GetNumTimeSamples() != 0 || restitution.GetNumTimeSamples() != 0 ||
        !friction.Get(&f) || !restitution.Get(&r)) {
        throw std::runtime_error(body.GetPath().GetString() +
                                 ": MMD contact coefficients require two constant floats");
    }
    const usd_physics::core::ContactMaterial material{f, r};
    try {
        usd_physics::core::validateContactMaterial(material);
    } catch (const std::invalid_argument& error) {
        throw std::runtime_error(body.GetPath().GetString() + ": " + error.what());
    }
    return material;
}

} // namespace mmd_physics_probe
