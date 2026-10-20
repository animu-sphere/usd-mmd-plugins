// SPDX-License-Identifier: Apache-2.0
#include "bone_binding.h"
#include <iostream>

namespace {
using namespace pxr;
using mmd_physics_probe::ReadBoneBinding;

void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

struct Fixture {
    UsdStageRefPtr stage{UsdStage::CreateInMemory()};
    UsdSkelSkeleton skeleton{UsdSkelSkeleton::Define(stage, SdfPath("/Skeleton"))};
    UsdPrim body{stage->DefinePrim(SdfPath("/Body"), TfToken("Xform"))};

    Fixture()
    {
        skeleton.CreateJointsAttr().Set(VtTokenArray{TfToken("root"), TfToken("root/child")});
        UsdPhysicsRigidBodyAPI::Apply(body).CreateKinematicEnabledAttr().Set(false);
        Attribute("bone", SdfValueTypeNames->Int).Set(1);
        Attribute("mode", SdfValueTypeNames->Token).Set(TfToken("dynamicWithBone"));
    }
    UsdAttribute Attribute(const char* name, SdfValueTypeName type)
    {
        return body.CreateAttribute(TfToken(std::string("mmd:physics:") + name), type,
                                    true, SdfVariabilityUniform);
    }
    UsdAttribute Get(const char* name)
    {
        return body.GetAttribute(TfToken(std::string("mmd:physics:") + name));
    }
    void Reject(const char* diagnostic)
    {
        try {
            ReadBoneBinding(body, skeleton, true);
        } catch (const std::runtime_error& error) {
            const std::string text(error.what());
            Check(text.find("/Body: MMD bone binding:") != std::string::npos &&
                  text.find(diagnostic) != std::string::npos, "missing path/field diagnostic");
            return;
        }
        throw std::runtime_error("malformed binding accepted");
    }
};
}

int main()
{
    try {
        for (const char* mode : {"followBone", "dynamic", "dynamicWithBone"}) {
            Fixture f;
            f.Get("mode").Set(TfToken(mode));
            UsdPhysicsRigidBodyAPI(f.body).GetKinematicEnabledAttr().Set(std::string(mode) == "followBone");
            for (int bone : {-1, 0, 1}) {
                f.Get("bone").Set(bone);
                const auto binding = ReadBoneBinding(f.body, f.skeleton, true);
                Check(binding && binding->bone == bone && binding->mode == TfToken(mode),
                      "mode/bone changed during validation");
            }
        }
        for (int bone : {-2, 2, 999}) {
            Fixture f;
            f.Get("bone").Set(bone);
            f.Reject("mmd:physics:bone");
        }
        for (const char* name : {"bone", "mode"}) {
            { Fixture f; f.body.RemoveProperty(f.Get(name).GetName()); f.Reject(name); }
            { Fixture f; f.Get(name).Clear(); f.Reject(name); }
            { Fixture f; f.Get(name).Block(); f.Reject(name); }
            { Fixture f; auto a = f.Get(name); a.SetVariability(SdfVariabilityVarying);
              VtValue value; a.Get(&value); a.Set(value, UsdTimeCode(1)); f.Reject(name); }
            { Fixture f; f.Get(name).SetVariability(SdfVariabilityVarying); f.Reject(name); }
            { Fixture f; f.body.RemoveProperty(f.Get(name).GetName());
              f.Attribute(name, SdfValueTypeNames->String).Set(std::string("1")); f.Reject(name); }
        }
        { Fixture f; f.Get("mode").Set(TfToken("unknown")); f.Reject("mode is unknown"); }
        { Fixture f; UsdPhysicsRigidBodyAPI(f.body).GetKinematicEnabledAttr().Set(true);
          f.Reject("physics:kinematicEnabled"); }
        { Fixture f; auto a = UsdPhysicsRigidBodyAPI(f.body).GetKinematicEnabledAttr();
          a.Set(false, UsdTimeCode(1)); f.Reject("physics:kinematicEnabled"); }
        { Fixture f; f.stage->RemovePrim(SdfPath("/Skeleton")); f.Reject("supplied skeleton"); }
        { Fixture f; f.skeleton.GetJointsAttr().Block(); f.Reject("supplied skeleton"); }
        { Fixture f; f.Get("bone").Set(-1);
          Check(ReadBoneBinding(f.body, UsdSkelSkeleton())->bone == -1, "sentinel requires a skeleton"); }
        { Fixture f; f.body.RemoveProperty(f.Get("bone").GetName());
          f.body.RemoveProperty(f.Get("mode").GetName());
          Check(!ReadBoneBinding(f.body, UsdSkelSkeleton()), "conventional body gained MMD defaults");
          f.Reject("mmd:physics:bone"); }
        std::cout << "MMD binding modes, sentinel, malformed declarations and diagnostics passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
