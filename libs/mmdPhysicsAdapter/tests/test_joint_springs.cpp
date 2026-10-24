// SPDX-License-Identifier: Apache-2.0
#include <mmdPhysicsAdapter/JointSprings.h>
#include <mmdPhysicsAdapter/Codes.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/stage.h>
#include <cassert>
#include <cmath>
#include <limits>

using namespace pxr;
namespace {
struct Fixture {
    UsdStageRefPtr stage = UsdStage::CreateInMemory();
    UsdPrim joint = stage->DefinePrim(SdfPath("/Joint"));
    Fixture() {
        joint.CreateAttribute(TfToken("mmd:physics:type"), SdfValueTypeNames->Token,
                              true, SdfVariabilityUniform).Set(TfToken("spring6Dof"));
        for (const char* name : {"translationSpring", "rotationSpring"})
            joint.CreateAttribute(TfToken(std::string("mmd:physics:") + name),
                SdfValueTypeNames->Vector3f, true, SdfVariabilityUniform).Set(GfVec3f(2, 0, -3));
    }
    void Refused() {
        const auto r = mmd::physics::ReadJointSprings(joint);
        assert(!r && r.fatal()->code == mmd::codes::PhysicsMetadataInvalid.id);
        assert(r.fatal()->message.find("/Joint") != std::string::npos);
    }
};
}
int main()
{
    assert(!mmd::physics::ReadJointSprings({}));
    Fixture f;
    std::string before, after;
    f.stage->GetRootLayer()->ExportToString(&before);
    auto r = mmd::physics::ReadJointSprings(f.joint);
    assert(r && r.value() && r.value()->spring6Dof);
    assert((r.value()->translation == std::array<double, 3>{2, 0, -3}));
    assert(std::abs(r.value()->rotation[0] - .0128) < 1e-12);
    assert(std::abs(r.value()->rotation[2] + .0192) < 1e-12);
    f.stage->GetRootLayer()->ExportToString(&after);
    assert(before == after);
    f.stage.Reset(); // Returned storage is independent of USD lifetime.
    assert(r.value()->translation[2] == -3);
    for (const char* type : {"", "point2point", "unknown"}) {
        Fixture other;
        if (*type) other.joint.GetAttribute(TfToken("mmd:physics:type")).Set(TfToken(type));
        else other.joint.RemoveProperty(TfToken("mmd:physics:type"));
        auto v = mmd::physics::ReadJointSprings(other.joint);
        assert(v && v.value() && !v.value()->spring6Dof);
    }
    {
        Fixture empty;
        empty.joint.RemoveProperty(TfToken("mmd:physics:translationSpring"));
        empty.joint.RemoveProperty(TfToken("mmd:physics:rotationSpring"));
        auto v = mmd::physics::ReadJointSprings(empty.joint);
        assert(v && !v.value());
    }
    for (const char* name : {"type", "translationSpring", "rotationSpring"}) {
        const TfToken token(std::string("mmd:physics:") + name);
        for (int mode = 0; mode < 6; ++mode) {
            Fixture bad;
            auto a = bad.joint.GetAttribute(token);
            if (mode == 0) {
                if (std::string(name) == "type") continue; // Optional type.
                bad.joint.RemoveProperty(token);
            }
            if (mode == 1) a.Clear();
            if (mode == 2) a.Block();
            if (mode == 3) {
                bad.joint.RemoveProperty(token);
                bad.joint.CreateAttribute(token, SdfValueTypeNames->String).Set(std::string("bad"));
            }
            if (mode == 4) {
                if (std::string(name) == "type") a.Set(TfToken("spring6Dof"), 1);
                else a.Set(GfVec3f(1), 1);
            }
            if (mode == 5) a.SetVariability(SdfVariabilityVarying);
            bad.Refused();
        }
    }
    for (const char* name : {"translationSpring", "rotationSpring"})
        for (int axis = 0; axis < 3; ++axis)
            for (float invalid : {std::numeric_limits<float>::infinity(),
                                  std::numeric_limits<float>::quiet_NaN()}) {
                Fixture bad;
                GfVec3f value(0); value[axis] = invalid;
                bad.joint.GetAttribute(TfToken(std::string("mmd:physics:") + name)).Set(value);
                bad.Refused();
            }
}
