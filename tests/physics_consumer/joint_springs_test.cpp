// SPDX-License-Identifier: Apache-2.0
#include "joint_springs.h"
#include <pxr/usd/usd/stage.h>
#include <iostream>
#include <limits>

namespace {
void
Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

pxr::UsdAttribute
Spring(const pxr::UsdPrim& joint, const char* name)
{
    return joint.CreateAttribute(
        pxr::TfToken(name), pxr::SdfValueTypeNames->Vector3f, true, pxr::SdfVariabilityUniform);
}

void
SetSprings(const pxr::UsdPrim& joint)
{
    Spring(joint, "mmd:physics:translationSpring").Set(pxr::GfVec3f(1, 0, -3));
    Spring(joint, "mmd:physics:rotationSpring").Set(pxr::GfVec3f(0, 5, 6));
}

void
Reject(const pxr::UsdPrim& joint)
{
    bool rejected = false;
    try {
        mmd_physics_probe::ReadJointSprings(joint);
    } catch (const std::runtime_error& error) {
        rejected = std::string(error.what()).find(joint.GetPath().GetString()) != std::string::npos;
    }
    Check(rejected, "malformed spring metadata must reject with the prim path");
}
} // namespace

int
main()
{
    try {
        const auto stage = pxr::UsdStage::CreateInMemory();
        auto joint = stage->DefinePrim(pxr::SdfPath("/Joint"));
        Check(mmd_physics_probe::ReadJointSprings(joint).NonzeroAxes() == 0,
              "conventional joints have no source springs");
        for (int axis = 0; axis != 6; ++axis) {
            pxr::GfVec3f translation(0), rotation(0);
            (axis < 3 ? translation : rotation)[axis % 3] = 2;
            Spring(joint, "mmd:physics:translationSpring").Set(translation);
            Spring(joint, "mmd:physics:rotationSpring").Set(rotation);
            const auto value = mmd_physics_probe::ReadJointSprings(joint);
            Check(value.translation == translation && value.rotation == rotation &&
                      value.NonzeroAxes() == 1,
                  "preserve and identify each spring axis independently");
        }
        SetSprings(joint);
        const auto value = mmd_physics_probe::ReadJointSprings(joint);
        Check(value.translation == pxr::GfVec3f(1, 0, -3) &&
                  value.rotation == pxr::GfVec3f(0, 5, 6) && value.NonzeroAxes() == 4,
              "preserve signed source coefficients without scaling or clamping");
        for (const auto* name : {"mmd:physics:translationSpring", "mmd:physics:rotationSpring"}) {
            for (int axis = 0; axis != 3; ++axis) {
                for (const float invalid : {std::numeric_limits<float>::quiet_NaN(),
                                            std::numeric_limits<float>::infinity(),
                                            -std::numeric_limits<float>::infinity()}) {
                    SetSprings(joint);
                    pxr::GfVec3f bad(0);
                    bad[axis] = invalid;
                    Spring(joint, name).Set(bad);
                    Reject(joint);
                }
            }
            SetSprings(joint);
            const auto attr = Spring(joint, name);
            attr.Set(pxr::GfVec3f(0), pxr::UsdTimeCode(1));
            Reject(joint);
            attr.Clear();
            Reject(joint);
            attr.Block();
            Reject(joint);
            joint.RemoveProperty(attr.GetName());
            Reject(joint);
            for (const auto& type :
                 {pxr::SdfValueTypeNames->Float3, pxr::SdfValueTypeNames->Vector3d}) {
                joint.CreateAttribute(pxr::TfToken(name), type);
                Reject(joint);
                joint.RemoveProperty(pxr::TfToken(name));
            }
        }
        SetSprings(joint);
        std::string before, after;
        stage->GetRootLayer()->ExportToString(&before);
        mmd_physics_probe::ReadJointSprings(joint);
        stage->GetRootLayer()->ExportToString(&after);
        Check(before == after, "spring reader must not edit stage");
        std::cout << "MMD joint spring metadata controls passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
