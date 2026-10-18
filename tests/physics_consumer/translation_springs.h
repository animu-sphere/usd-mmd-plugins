// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "joint_springs.h"
#include <usd_physics/core/six_dof.h>

namespace mmd_physics_probe {

// Experimental Hookean approximation only. Bullet's iterative motor response
// and dimensionless spring damping are not equivalent to a viscous damper.
// With length multiplied by s and mass/time unchanged: s * k * x = k * (s*x),
// so translation stiffness retains its number. Joint frames are already SI.
inline std::size_t
ApplyTranslationSprings(const pxr::UsdPrim& prim,
                        usd_physics::core::SixDofConstraintDescriptor& descriptor)
{
    const auto springs = ReadJointSprings(prim);
    if (!IsSpring6Dof(prim))
        return 0;
    std::size_t applied = 0;
    for (int axis = 0; axis != 3; ++axis) {
        if (springs.translation[axis] <= 0 ||
            descriptor.limits[axis].motion == usd_physics::core::AxisMotion::locked)
            continue; // Signed/locked coefficients remain visible fidelity gaps.
        descriptor.translationSprings[axis] = {springs.translation[axis], 0, 0};
        ++applied;
    }
    return applied;
}
} // namespace mmd_physics_probe
