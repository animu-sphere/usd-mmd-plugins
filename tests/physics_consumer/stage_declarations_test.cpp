// SPDX-License-Identifier: Apache-2.0
#include "stage_declarations.h"
#include <functional>
#include <iostream>

namespace {
using namespace pxr;
using namespace mmd_physics_probe;

void Reject(const std::function<void()>& operation, const char* field)
{
    try {
        operation();
    } catch (const std::runtime_error& error) {
        const std::string text(error.what());
        if (text.find("/Joint") != std::string::npos && text.find(field) != std::string::npos)
            return;
        throw;
    }
    throw std::runtime_error("malformed declaration accepted");
}

struct Fixture {
    UsdStageRefPtr stage{UsdStage::CreateInMemory()};
    std::vector<UsdPrim> bodies{stage->DefinePrim(SdfPath("/BodyA")),
                                stage->DefinePrim(SdfPath("/BodyB"))};
    UsdPhysicsJoint api{UsdPhysicsJoint::Define(stage, SdfPath("/Joint"))};
    UsdPrim joint{api.GetPrim()};
    Fixture()
    {
        Attribute("type", SdfValueTypeNames->Token).Set(TfToken("spring6Dof"));
        Attribute("rigidBodyA", SdfValueTypeNames->Int).Set(0);
        Attribute("rigidBodyB", SdfValueTypeNames->Int).Set(1);
        api.CreateBody0Rel().SetTargets({bodies[0].GetPath()});
        api.CreateBody1Rel().SetTargets({bodies[1].GetPath()});
    }
    UsdAttribute Attribute(const char* name, SdfValueTypeName type)
    {
        return joint.CreateAttribute(TfToken(std::string("mmd:physics:") + name), type,
                                     true, SdfVariabilityUniform);
    }
    void Validate() { ValidateJointReferences(joint, bodies); }
};
}

int main()
{
    try {
        for (const char* kind : {"spring6Dof", "sixDof", "pointToPoint", "coneTwist", "slider", "hinge"}) {
            Fixture f;
            f.Attribute("type", SdfValueTypeNames->Token).Set(TfToken(kind));
            if (std::string(kind) != "spring6Dof" && std::string(kind) != "sixDof")
                f.joint.SetTypeName(TfToken());
            f.Validate();
        }
        for (const char* name : {"rigidBodyA", "rigidBodyB"}) {
            for (int index : {-2, -1, 2, 999}) {
                Fixture f; f.Attribute(name, SdfValueTypeNames->Int).Set(index);
                Reject([&] { f.Validate(); }, name);
            }
            { Fixture f; f.joint.RemoveProperty(TfToken(std::string("mmd:physics:") + name));
              Reject([&] { f.Validate(); }, name); }
            { Fixture f; f.Attribute(name, SdfValueTypeNames->Int).Block();
              Reject([&] { f.Validate(); }, name); }
            { Fixture f; auto a = f.Attribute(name, SdfValueTypeNames->Int);
              a.SetVariability(SdfVariabilityVarying); a.Set(0, UsdTimeCode(1));
              Reject([&] { f.Validate(); }, name); }
            { Fixture f; f.joint.RemoveProperty(TfToken(std::string("mmd:physics:") + name));
              f.Attribute(name, SdfValueTypeNames->Int64).Set(std::int64_t(0));
              Reject([&] { f.Validate(); }, name); }
        }
        { Fixture f; f.api.GetBody0Rel().SetTargets({f.bodies[1].GetPath()});
          Reject([&] { f.Validate(); }, "rigidBodyA"); }
        { Fixture f; f.api.GetBody1Rel().SetTargets({});
          Reject([&] { f.Validate(); }, "rigidBodyB"); }
        { Fixture f; f.Attribute("type", SdfValueTypeNames->Token).Set(TfToken("unknown"));
          Reject([&] { f.Validate(); }, "type"); }
        { Fixture f; f.joint.SetTypeName(TfToken()); Reject([&] { f.Validate(); }, "schema"); }
        { Fixture f; f.Attribute("type", SdfValueTypeNames->Token).Set(TfToken("hinge"));
          Reject([&] { f.Validate(); }, "schema"); }
        { Fixture f;
          const auto check = [&] { RequirePhysicsValue(f.joint, "position", SdfValueTypeNames->Point3d); };
          Reject(check, "position");
          auto a = f.Attribute("position", SdfValueTypeNames->Point3d);
          a.Set(GfVec3d(1, 2, 3)); check();
          a.Block(); Reject(check, "position"); }
        std::cout << "MMD joint types, endpoints, required values and diagnostics passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
