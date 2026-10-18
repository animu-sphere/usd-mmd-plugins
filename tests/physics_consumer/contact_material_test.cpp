// SPDX-License-Identifier: Apache-2.0
#include "contact_material.h"
#include <pxr/usd/usd/stage.h>
#ifdef MMD_PHYSICS_EXECUTION
#include <usd_physics/jolt/availability.h>
#endif
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace {
namespace core = usd_physics::core;
using mmd_physics_probe::ReadContactMaterial;

void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

pxr::UsdAttribute Coefficient(const pxr::UsdPrim& body, const char* name)
{
    return body.CreateAttribute(pxr::TfToken(name), pxr::SdfValueTypeNames->Float,
                                true, pxr::SdfVariabilityUniform);
}

core::ContactMaterial Material(const pxr::UsdPrim& body, float friction, float restitution)
{
    Coefficient(body, "mmd:physics:friction").Set(friction);
    Coefficient(body, "mmd:physics:restitution").Set(restitution);
    return ReadContactMaterial(body);
}

void Reject(const pxr::UsdPrim& body)
{
    bool rejected = false;
    try { ReadContactMaterial(body); }
    catch (const std::runtime_error&) { rejected = true; }
    Check(rejected, "invalid MMD contact metadata must reject");
}

#ifdef MMD_PHYSICS_EXECUTION
// This includes the MMD conversion before backend creation. Quantitative
// mixing and precision coverage belongs to the shared backend's own tests.
double Response(const pxr::UsdPrim& bodyPrim, float friction, float restitution, bool sliding)
{
    auto world = usd_physics::jolt::createWorld();
    const auto material = Material(bodyPrim, friction, restitution);
    const auto floor = world->createShape({core::ShapeType::box, {20, .5, 5}});
    core::ShapeDescriptor geometry;
    if (!sliding) geometry.type = core::ShapeType::sphere;
    const auto shape = world->createShape(geometry);
    world->createBody({floor, core::MotionType::staticBody, {{0, -.5, 0}, {}},
                       0, {}, material});
    const auto body = world->createBody(
        {shape, core::MotionType::dynamicBody, {{0, sliding ? .51 : 3, 0}, {}},
         1, {}, material});
    if (sliding) {
        for (int i = 0; i != 24; ++i) world->step(core::PhysicsWorld::Duration{1.0/240});
        Check(world->setLinearVelocity(body, {5, 0, 0}), "could not start sliding control");
    }
    double upward = 0;
    for (int i = 0; i != (sliding ? 120 : 360); ++i) {
        world->step(core::PhysicsWorld::Duration{1.0/240});
        const auto state = world->bodyState(body);
        Check(std::isfinite(state.linearVelocity.x) && std::isfinite(state.linearVelocity.y),
              "contact response must remain finite");
        upward = std::max(upward, state.linearVelocity.y);
        world->takeChangedBodyStates();
    }
    return sliding ? world->bodyState(body).linearVelocity.x : upward;
}
#endif
}

int main()
{
    try {
        const auto stage = pxr::UsdStage::CreateInMemory();
        auto body = stage->DefinePrim(pxr::SdfPath("/Body"));
        const auto generic = ReadContactMaterial(body);
        Check(generic.friction == .2 && generic.restitution == 0,
              "conventional body retains neutral defaults");
        for (const float friction : {0.f, .75f, 4.f}) {
            for (const float restitution : {0.f, .5f, 1.f}) {
                const auto value = Material(body, friction, restitution);
                Check(value.friction == friction && value.restitution == restitution,
                      "source coefficients must survive without clamping");
            }
        }
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float infinity = std::numeric_limits<float>::infinity();
        for (const auto* name : {"mmd:physics:friction", "mmd:physics:restitution"}) {
            Material(body, .75f, .5f);
            auto attr = Coefficient(body, name);
            for (const float invalid : {-1.f, nan, infinity}) {
                attr.Set(invalid);
                Reject(body);
            }
            Material(body, .75f, .5f);
            attr.Set(0.f, pxr::UsdTimeCode(1));
            Reject(body);
            attr.Clear();
            Reject(body); // Declaration without a value.
            attr.Block();
            Reject(body);
            body.RemoveProperty(attr.GetName());
            Reject(body); // Partial pair.
            for (const auto& type : {pxr::SdfValueTypeNames->Int, pxr::SdfValueTypeNames->Double}) {
                const auto wrong = body.CreateAttribute(pxr::TfToken(name), type);
                if (type == pxr::SdfValueTypeNames->Int) wrong.Set(1);
                else wrong.Set(1.0);
                Reject(body);
                body.RemoveProperty(pxr::TfToken(name));
            }
        }
        Coefficient(body, "mmd:physics:friction").Set(.75f);
        Coefficient(body, "mmd:physics:restitution").Set(1.01f);
        Reject(body);
        Material(body, .75f, .5f);
        std::string before, after;
        stage->GetRootLayer()->ExportToString(&before);
        ReadContactMaterial(body);
        stage->GetRootLayer()->ExportToString(&after);
        Check(before == after, "contact conversion must not edit stage");
#ifdef MMD_PHYSICS_EXECUTION
        const auto free = Response(body, 0, 0, true);
        const auto rough = Response(body, 1, 0, true);
        const auto dead = Response(body, 0, 0, false);
        const auto live = Response(body, 0, .8f, false);
        Check(free > 4.7 && std::abs(rough) < .5 && dead < .5 && live > 4,
              "MMD coefficients must change sliding and bounce response");
        std::cout << "contact responses: free=" << free << " rough=" << rough
                  << " dead=" << dead << " live=" << live << '\n';
#endif
        std::cout << "MMD contact material controls passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
