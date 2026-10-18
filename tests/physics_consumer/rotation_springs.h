// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "joint_springs.h"
#include <usd_physics/core/six_dof.h>

namespace mmd_physics_probe {

// Experimental small-angle motor approximation, not Bullet's Euler springs.
// STAGE_CONTRACT section 6.2 fixes s=0.08 m/source unit. With mass/time
// unchanged, inertia and torque scale by s*s; angles do not scale.
inline std::size_t
ApplyRotationSprings(const pxr::UsdPrim& prim,
                     usd_physics::core::SixDofConstraintDescriptor& descriptor)
{
    const auto springs = ReadJointSprings(prim);
    if (!IsSpring6Dof(prim))
        return 0;
    constexpr double scaleSquared = 0.08 * 0.08;
    std::size_t applied = 0;
    for (int axis = 0; axis != 3; ++axis) {
        if (springs.rotation[axis] <= 0 ||
            descriptor.limits[axis + 3].motion == usd_physics::core::AxisMotion::locked)
            continue;
        descriptor.rotationSprings[axis] = {springs.rotation[axis] * scaleSquared, 0};
        ++applied;
    }
    return applied;
}
} // namespace mmd_physics_probe
