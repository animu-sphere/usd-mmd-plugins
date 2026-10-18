// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <usd_physics/core/descriptors.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/prim.h>
#include <cmath>
#include <stdexcept>

namespace mmd_physics_probe {

// Bullet's default damping factor is (1-d)^dt. Convert to a neutral
// inverse-second rate; Jolt approximates exp(-rate*dt) at each substep.
inline usd_physics::core::BodyDamping
ReadBodyDamping(const pxr::UsdPrim& body)
{
    const auto linear = body.GetAttribute(pxr::TfToken("mmd:physics:linearDamping"));
    const auto angular = body.GetAttribute(pxr::TfToken("mmd:physics:angularDamping"));
    if (!linear && !angular)
        return {};
    float l = 0, a = 0;
    if (!linear || !angular || linear.GetTypeName() != pxr::SdfValueTypeNames->Float ||
        angular.GetTypeName() != pxr::SdfValueTypeNames->Float || linear.GetNumTimeSamples() != 0 ||
        angular.GetNumTimeSamples() != 0 || !linear.Get(&l) || !angular.Get(&a)) {
        throw std::runtime_error(body.GetPath().GetString() +
                                 ": MMD damping requires two constant floats");
    }
    if (!std::isfinite(l) || !std::isfinite(a) || l < 0 || a < 0 || l > 1 || a > 1) {
        throw std::runtime_error(body.GetPath().GetString() +
                                 ": MMD damping coefficients must be finite and in [0,1]");
    }
    if (l == 1 || a == 1) {
        throw std::runtime_error(body.GetPath().GetString() +
                                 ": MMD damping equal to one requires instantaneous stopping; "
                                 "unsupported by finite decay rates");
    }
    const usd_physics::core::BodyDamping damping{-std::log1p(-double(l)), -std::log1p(-double(a))};
    usd_physics::core::validateBodyDamping(damping);
    return damping;
}

} // namespace mmd_physics_probe
