// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdPhysics/rigidBodyAPI.h>
#include <pxr/usd/usdSkel/skeleton.h>
#include <optional>
#include <stdexcept>

namespace mmd_physics_probe {

// Validation only, not a coupling API. The caller supplies the skeleton;
// neither this reader nor the importer creates a world or advances time.
struct BoneBinding {
    int bone{-1};
    pxr::TfToken mode;
};

inline std::optional<BoneBinding>
ReadBoneBinding(const pxr::UsdPrim& body, const pxr::UsdSkelSkeleton& skeleton,
                bool required = false)
{
    const auto bone = body.GetAttribute(pxr::TfToken("mmd:physics:bone"));
    const auto mode = body.GetAttribute(pxr::TfToken("mmd:physics:mode"));
    if (!required && !bone && !mode)
        return std::nullopt; // Conventional USD body; no MMD binding declared.

    const auto reject = [&](const char* reason) {
        throw std::runtime_error(body.GetPath().GetString() + ": MMD bone binding: " + reason);
    };
    const auto constant = [](const pxr::UsdAttribute& attr, pxr::SdfValueTypeName type) {
        return attr && attr.GetTypeName() == type && attr.GetNumTimeSamples() == 0 &&
               attr.GetVariability() == pxr::SdfVariabilityUniform;
    };
    BoneBinding result;
    if (!constant(bone, pxr::SdfValueTypeNames->Int) || !bone.Get(&result.bone))
        reject("mmd:physics:bone requires a constant uniform int value");
    if (!constant(mode, pxr::SdfValueTypeNames->Token) || !mode.Get(&result.mode))
        reject("mmd:physics:mode requires a constant uniform token value");
    if (result.bone < -1)
        reject("mmd:physics:bone must be -1 or a canonical joint index");
    if (result.bone >= 0) {
        pxr::VtTokenArray joints;
        const auto attr = skeleton ? skeleton.GetJointsAttr() : pxr::UsdAttribute{};
        if (!attr || attr.GetNumTimeSamples() != 0 || !attr.Get(&joints) ||
            static_cast<std::size_t>(result.bone) >= joints.size())
            reject("mmd:physics:bone does not resolve to the supplied skeleton");
    }
    const bool follow = result.mode == pxr::TfToken("followBone");
    if (!follow && result.mode != pxr::TfToken("dynamic") &&
        result.mode != pxr::TfToken("dynamicWithBone"))
        reject("mmd:physics:mode is unknown");
    bool kinematic = false;
    const pxr::UsdPhysicsRigidBodyAPI api(body);
    if (!body.HasAPI<pxr::UsdPhysicsRigidBodyAPI>() ||
        !api.GetKinematicEnabledAttr().Get(&kinematic) ||
        api.GetKinematicEnabledAttr().GetNumTimeSamples() != 0 || kinematic != follow)
        reject("mmd:physics:mode disagrees with physics:kinematicEnabled");
    return result; // -1 is preserved; it cannot drive or receive a bone pose.
}

} // namespace mmd_physics_probe
