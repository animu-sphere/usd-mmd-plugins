// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <usd_physics/core/descriptors.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/prim.h>
#include <stdexcept>

namespace mmd_physics_probe {

// MMD-only conversion kept outside the installed generic USD reader. The
// stage contract preserves an acceptance mask: set bits admit groups.
inline usd_physics::core::CollisionFilter
ReadCollisionFilter(const pxr::UsdPrim& body)
{
    const auto group = body.GetAttribute(pxr::TfToken("mmd:physics:collisionGroup"));
    const auto mask = body.GetAttribute(pxr::TfToken("mmd:physics:collisionMask"));
    if (!group && !mask)
        return {}; // A conventional USD body retains the backend's default.

    int g = -1, m = -1;
    if (!group || !mask || group.GetTypeName() != pxr::SdfValueTypeNames->Int ||
        mask.GetTypeName() != pxr::SdfValueTypeNames->Int || group.GetNumTimeSamples() != 0 ||
        mask.GetNumTimeSamples() != 0 || !group.Get(&g) || !mask.Get(&m) || g < 0 || g > 15 ||
        m < 0 || m > 0xffff) {
        throw std::runtime_error(
            body.GetPath().GetString() +
            ": MMD collision filtering requires a constant int group [0,15] and mask [0,65535]");
    }
    using Categories = usd_physics::core::CollisionCategories;
    return {Categories{1} << g, static_cast<Categories>(m)};
}

} // namespace mmd_physics_probe
