// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <pxr/base/gf/vec3f.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/prim.h>
#include <cmath>
#include <stdexcept>

namespace mmd_physics_probe {

// Source coefficients are preserved verbatim, not converted to drive units.
struct JointSprings {
    pxr::GfVec3f translation{0};
    pxr::GfVec3f rotation{0};

    int NonzeroAxes() const
    {
        int count = 0;
        for (int axis = 0; axis != 3; ++axis) {
            count += translation[axis] != 0;
            count += rotation[axis] != 0;
        }
        return count;
    }
};

inline JointSprings
ReadJointSprings(const pxr::UsdPrim& joint)
{
    const auto translation = joint.GetAttribute(pxr::TfToken("mmd:physics:translationSpring"));
    const auto rotation = joint.GetAttribute(pxr::TfToken("mmd:physics:rotationSpring"));
    if (!translation && !rotation)
        return {};
    JointSprings result;
    if (!translation || !rotation ||
        translation.GetTypeName() != pxr::SdfValueTypeNames->Vector3f ||
        rotation.GetTypeName() != pxr::SdfValueTypeNames->Vector3f ||
        translation.GetNumTimeSamples() != 0 || rotation.GetNumTimeSamples() != 0 ||
        !translation.Get(&result.translation) || !rotation.Get(&result.rotation)) {
        throw std::runtime_error(joint.GetPath().GetString() +
                                 ": MMD joint springs require two constant vector3f values");
    }
    for (int axis = 0; axis != 3; ++axis) {
        if (!std::isfinite(result.translation[axis]) || !std::isfinite(result.rotation[axis]))
            throw std::runtime_error(joint.GetPath().GetString() +
                                     ": MMD joint spring coefficients must be finite");
    }
    return result;
}

} // namespace mmd_physics_probe
