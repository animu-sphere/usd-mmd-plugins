// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <mmdPmx/Result.h>
#include <pxr/usd/usd/prim.h>
#include <array>
#include <optional>

namespace mmd::physics {

/// Signed coefficients converted to SI, without selecting executable axes.
/// Translation retains the source number; rotation scales by 0.08^2.
struct JointSprings {
    bool spring6Dof = false;
    std::array<double, 3> translation{}, rotation{};
};

/// Read an optional complete, uniform, unsampled vector3f pair and optional
/// uniform token type. Malformed declarations refuse with the joint path.
/// No pair returns nullopt. Missing/other types never activate springs.
/// Hosts independently opt into experimental translation/rotation response,
/// selecting only positive coefficients on free/limited standard joint axes.
/// Zero equilibrium and zero viscous damping apply; signed/locked/format-only
/// meaning remains unsupported. This conversion alone claims no execution.
Result<std::optional<JointSprings>> ReadJointSprings(const pxr::UsdPrim& joint);

} // namespace mmd::physics
