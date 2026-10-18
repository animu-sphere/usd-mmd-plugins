// SPDX-License-Identifier: Apache-2.0
#include "body_damping.h"
#include <pxr/usd/usd/stage.h>
#ifdef MMD_PHYSICS_EXECUTION
#include <usd_physics/jolt/availability.h>
#endif
#include <iostream>
#include <limits>
#include <utility>

namespace {
namespace core = usd_physics::core;
using mmd_physics_probe::ReadBodyDamping;

void
Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

pxr::UsdAttribute
Coefficient(const pxr::UsdPrim& body, const char* name)
{
    return body.CreateAttribute(
        pxr::TfToken(name), pxr::SdfValueTypeNames->Float, true, pxr::SdfVariabilityUniform);
}

core::BodyDamping
Damping(const pxr::UsdPrim& body, float linear, float angular)
{
    Coefficient(body, "mmd:physics:linearDamping").Set(linear);
    Coefficient(body, "mmd:physics:angularDamping").Set(angular);
    return ReadBodyDamping(body);
}

void
Reject(const pxr::UsdPrim& body)
{
    bool rejected = false;
    try {
        ReadBodyDamping(body);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Check(rejected, "unsupported or invalid MMD damping must reject");
}

#ifdef MMD_PHYSICS_EXECUTION
double
LinearResponse(const pxr::UsdPrim& prim, float linear, float angular, int hz)
{
    auto world = usd_physics::jolt::createWorld();
    const auto shape = world->createShape({});
    const auto body = world->createBody(
        {shape, core::MotionType::dynamicBody, {}, 1, {}, {}, Damping(prim, linear, angular)});
    Check(world->setLinearVelocity(body, {10, 0, 0}), "start linear control");
    for (int i = 0; i != hz; ++i)
        world->step(core::PhysicsWorld::Duration{1.0 / hz});
    return world->bodyState(body).linearVelocity.x / 10;
}

double
RotationSpeed(core::Quaternion a, core::Quaternion b, double dt)
{
    const double x = a.w * b.x - a.x * b.w - a.y * b.z + a.z * b.y;
    const double y = a.w * b.y + a.x * b.z - a.y * b.w - a.z * b.x;
    const double z = a.w * b.z - a.x * b.y + a.y * b.x - a.z * b.w;
    const double w = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
    return 2 * std::atan2(std::sqrt(x * x + y * y + z * z), std::abs(w)) / dt;
}

double
AngularResponse(const pxr::UsdPrim& prim, float linear, float angular, int hz)
{
    auto world = usd_physics::jolt::createWorld();
    const auto floorShape = world->createShape({core::ShapeType::box, {20, .5, 5}});
    const auto floor = world->createBody(
        {floorShape, core::MotionType::staticBody, {{0, -.5, 0}, {}}, 0, {}, {1, 0}});
    core::ShapeDescriptor sphere;
    sphere.type = core::ShapeType::sphere;
    const auto shape = world->createShape(sphere);
    const auto body = world->createBody({shape,
                                         core::MotionType::dynamicBody,
                                         {{0, .51, 0}, {}},
                                         1,
                                         {},
                                         {1, 0},
                                         Damping(prim, linear, angular)});
    Check(world->setLinearVelocity(body, {5, 0, 0}), "start rolling control");
    // Contact seeds spin; removing the floor leaves a torque-free sphere.
    for (int i = 0; i != 120; ++i)
        world->step(core::PhysicsWorld::Duration{1.0 / 240});
    Check(world->destroyBody(floor), "remove rolling floor");
    const double dt = 1.0 / hz;
    auto previous = world->bodyState(body).transform.rotation;
    world->step(core::PhysicsWorld::Duration{dt});
    auto current = world->bodyState(body).transform.rotation;
    const double initial = RotationSpeed(previous, current, dt);
    Check(std::isfinite(initial) && initial > 1, "must seed measurable angular speed");
    for (int i = 0; i != hz; ++i) {
        previous = current;
        world->step(core::PhysicsWorld::Duration{dt});
        current = world->bodyState(body).transform.rotation;
    }
    return RotationSpeed(previous, current, dt) / initial;
}
#endif
} // namespace

int
main()
{
    try {
        const auto stage = pxr::UsdStage::CreateInMemory();
        auto body = stage->DefinePrim(pxr::SdfPath("/Body"));
        const auto generic = ReadBodyDamping(body);
        Check(generic.linear == .05 && generic.angular == .05, "retain neutral defaults");
        for (const float linear : {0.f, .5f, .99f, std::nextafter(1.f, 0.f)}) {
            for (const float angular : {0.f, .25f, .75f}) {
                const auto value = Damping(body, linear, angular);
                Check(std::abs(std::exp(-value.linear) - (1 - double(linear))) < 1e-12 &&
                          std::abs(std::exp(-value.angular) - (1 - double(angular))) < 1e-12,
                      "convert source retention to independent inverse-second rates");
            }
        }
        for (const auto* name : {"mmd:physics:linearDamping", "mmd:physics:angularDamping"}) {
            Damping(body, .5f, .25f);
            auto attr = Coefficient(body, name);
            for (const float invalid : {-1.f,
                                        1.f,
                                        1.01f,
                                        std::numeric_limits<float>::quiet_NaN(),
                                        std::numeric_limits<float>::infinity()}) {
                attr.Set(invalid);
                Reject(body);
            }
            Damping(body, .5f, .25f);
            attr.Set(0.f, pxr::UsdTimeCode(1));
            Reject(body);
            attr.Clear();
            Reject(body);
            attr.Block();
            Reject(body);
            body.RemoveProperty(attr.GetName());
            Reject(body);
            for (const auto& type : {pxr::SdfValueTypeNames->Int, pxr::SdfValueTypeNames->Double}) {
                const auto wrong = body.CreateAttribute(pxr::TfToken(name), type);
                if (type == pxr::SdfValueTypeNames->Int)
                    wrong.Set(0);
                else
                    wrong.Set(0.0);
                Reject(body);
                body.RemoveProperty(pxr::TfToken(name));
            }
        }
        Damping(body, .5f, .25f);
        std::string before, after;
        stage->GetRootLayer()->ExportToString(&before);
        ReadBodyDamping(body);
        stage->GetRootLayer()->ExportToString(&after);
        Check(before == after, "damping reader must not edit stage");
#ifdef MMD_PHYSICS_EXECUTION
        for (const int hz : {60, 240}) {
            for (const auto pair :
                 {std::pair<float, float>{0, 0}, {.5f, 0}, {0, .75f}, {.5f, .75f}}) {
                const auto linear = LinearResponse(body, pair.first, pair.second, hz);
                const auto angular = AngularResponse(body, pair.first, pair.second, hz);
                Check(std::abs(linear - (1 - pair.first)) < .006 &&
                          std::abs(angular - (1 - pair.second)) < .006,
                      "source damping must approximate one-second retention independently");
                std::cout << hz << " Hz source=" << pair.first << ',' << pair.second
                          << " retention=" << linear << ',' << angular << '\n';
            }
        }
#endif
        std::cout << "MMD damping controls passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
