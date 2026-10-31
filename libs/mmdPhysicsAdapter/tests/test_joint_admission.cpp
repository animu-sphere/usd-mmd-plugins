// SPDX-License-Identifier: Apache-2.0
#include <mmdPhysicsAdapter/JointAdmission.h>
#include <mmdPhysicsAdapter/Codes.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usdPhysics/joint.h>
#include <pxr/usd/usdPhysics/rigidBodyAPI.h>
#include <pxr/usd/sdf/types.h>
#include <cassert>

using namespace pxr;
struct Fixture {
    UsdStageRefPtr stage = UsdStage::CreateInMemory();
    UsdPhysicsJoint joint = UsdPhysicsJoint::Define(stage, SdfPath("/Joint"));
    Fixture(const char* first = "followBone", const char* second = "followBone") {
        joint.GetPrim().CreateAttribute(TfToken("mmd:physics:type"), SdfValueTypeNames->Token,
            true, SdfVariabilityUniform).Set(TfToken("spring6Dof"));
        int i = 0;
        for (const char* mode : {first, second}) {
            const SdfPath path(i++ ? "/B" : "/A");
            auto body = stage->DefinePrim(path);
            body.CreateAttribute(TfToken("mmd:physics:mode"), SdfValueTypeNames->Token,
                true, SdfVariabilityUniform).Set(TfToken(mode));
            auto api = UsdPhysicsRigidBodyAPI::Apply(body);
            api.CreateRigidBodyEnabledAttr().Set(true);
            api.CreateKinematicEnabledAttr().Set(TfToken(mode) == TfToken("followBone"));
        }
        joint.CreateBody0Rel().SetTargets({SdfPath("/A")});
        joint.CreateBody1Rel().SetTargets({SdfPath("/B")});
    }
    auto Read() { return mmd::physics::ReadJointAdmission(joint.GetPrim()); }
    void Refused() {
        auto r = Read();
        assert(!r && r.fatal()->code == mmd::codes::PhysicsBindingInvalid.id);
        assert(r.fatal()->message.find("/Joint") != std::string::npos);
    }
};
int main() {
    assert(!mmd::physics::ReadJointAdmission({}));
    for (const char* a : {"followBone", "dynamic", "dynamicWithBone"})
        for (const char* b : {"followBone", "dynamic", "dynamicWithBone"}) {
            Fixture f(a, b);
            std::string before, after;
            f.stage->GetRootLayer()->ExportToString(&before);
            auto r = f.Read(); assert(r);
            assert((r.value() == mmd::physics::JointAdmission::Inert) ==
                (std::string(a) == "followBone" && std::string(b) == "followBone"));
            f.stage->GetRootLayer()->ExportToString(&after); assert(before == after);
            f.stage.Reset(); assert(r);
        }
    for (bool second : {false, true}) {
        // Even a dynamic first endpoint must not bypass invalid second metadata.
        for (int defect = 0; defect < 11; ++defect) {
            Fixture f("dynamic", "followBone");
            const auto path = SdfPath(second ? "/B" : "/A");
            const auto body = f.stage->GetPrimAtPath(path);
            auto mode = body.GetAttribute(TfToken("mmd:physics:mode"));
            const auto rel = second ? f.joint.GetBody1Rel() : f.joint.GetBody0Rel();
            if (defect == 0) rel.SetTargets({});
            if (defect == 1) rel.SetTargets({path, SdfPath("/Missing")});
            if (defect == 2) rel.SetTargets({SdfPath("/Missing")});
            if (defect == 3) mode.Set(TfToken("unknown"));
            if (defect == 4) mode.Block();
            if (defect == 5) mode.Set(TfToken("dynamic"), 1);
            if (defect == 6) mode.SetVariability(SdfVariabilityVarying);
            const UsdPhysicsRigidBodyAPI api(body);
            if (defect == 7) api.GetKinematicEnabledAttr().Set(!second);
            if (defect == 8) api.GetRigidBodyEnabledAttr().Set(false);
            if (defect == 9) api.GetKinematicEnabledAttr().Block();
            if (defect == 10) api.GetRigidBodyEnabledAttr().Set(true, 1);
            f.Refused();
        }
    }
    for (const char* type : {"unknown", "point2point"}) {
        Fixture f;
        f.joint.GetPrim().GetAttribute(TfToken("mmd:physics:type")).Set(TfToken(type));
        auto r = f.Read();
        assert(r && r.value() == mmd::physics::JointAdmission::Execute);
    }
    Fixture absent;
    absent.joint.GetPrim().RemoveProperty(TfToken("mmd:physics:type"));
    assert(absent.Read().value() == mmd::physics::JointAdmission::Execute);
    for (int defect = 0; defect < 4; ++defect) {
        Fixture f;
        auto attr = f.joint.GetPrim().GetAttribute(TfToken("mmd:physics:type"));
        if (defect == 0) attr.Block();
        if (defect == 1) attr.Set(TfToken("spring6Dof"), 1);
        if (defect == 2) attr.SetVariability(SdfVariabilityVarying);
        if (defect == 3) {
            f.joint.GetPrim().RemoveProperty(attr.GetName());
            f.joint.GetPrim().CreateAttribute(TfToken("mmd:physics:type"),
                SdfValueTypeNames->String).Set(std::string("spring6Dof"));
        }
        f.Refused();
    }
}
