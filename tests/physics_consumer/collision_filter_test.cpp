// SPDX-License-Identifier: Apache-2.0
#include "collision_filter.h"
#include <pxr/usd/usd/stage.h>
#ifdef MMD_PHYSICS_EXECUTION
#include <usd_physics/jolt/availability.h>
#endif
#include <cmath>
#include <iostream>

namespace {
namespace core = usd_physics::core;
using mmd_physics_probe::ReadCollisionFilter;

void
Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

pxr::UsdAttribute
Group(const pxr::UsdPrim& body)
{
    return body.CreateAttribute(pxr::TfToken("mmd:physics:collisionGroup"),
                                pxr::SdfValueTypeNames->Int,
                                true,
                                pxr::SdfVariabilityUniform);
}

pxr::UsdAttribute
Mask(const pxr::UsdPrim& body)
{
    return body.CreateAttribute(pxr::TfToken("mmd:physics:collisionMask"),
                                pxr::SdfValueTypeNames->Int,
                                true,
                                pxr::SdfVariabilityUniform);
}

core::CollisionFilter
Filter(const pxr::UsdPrim& body, int group, int mask)
{
    Group(body).Set(group);
    Mask(body).Set(mask);
    return ReadCollisionFilter(body);
}

void
Reject(const pxr::UsdPrim& body)
{
    bool rejected = false;
    try {
        ReadCollisionFilter(body);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Check(rejected, "invalid MMD collision metadata must reject");
}

#ifdef MMD_PHYSICS_EXECUTION
// Contact is observed independently of the mask formula: an admitted body
// rests on a floor; a rejected one falls below it. No joint can hide filtering.
void
Drop(core::CollisionFilter floorFilter, core::CollisionFilter fallingFilter, bool shouldCollide)
{
    auto world = usd_physics::jolt::createWorld();
    core::ShapeDescriptor floorShape;
    floorShape.halfExtents = {3, 0.5, 3};
    const auto floor = world->createShape(floorShape);
    const auto box = world->createShape({});
    world->createBody({floor, core::MotionType::staticBody, {}, 0, floorFilter});
    const auto body =
        world->createBody({box, core::MotionType::dynamicBody, {{0, 3, 0}, {}}, 1, fallingFilter});
    for (int step = 0; step != 240; ++step) {
        world->step(core::PhysicsWorld::Duration{1.0 / 120.0});
        world->takeChangedBodyStates();
    }
    const auto state = world->bodyState(body);
    Check(std::isfinite(state.transform.translation.y) && std::isfinite(state.linearVelocity.y),
          "drop control must remain finite");
    if (shouldCollide)
        Check(std::abs(state.transform.translation.y - 1) < 0.03 &&
                  std::abs(state.linearVelocity.y) < 0.05,
              "admitted body must settle on floor");
    else
        Check(state.transform.translation.y < -1, "filtered body must fall through floor");
}

void
MixedMasks(core::CollisionFilter floorFilter, core::CollisionFilter admittedFilter,
           core::CollisionFilter rejectedFilter)
{
    auto world = usd_physics::jolt::createWorld();
    core::ShapeDescriptor geometry;
    geometry.halfExtents = {5, 0.5, 3};
    const auto floor = world->createShape(geometry);
    const auto box = world->createShape({});
    world->createBody({floor, core::MotionType::staticBody, {}, 0, floorFilter});
    const auto admitted = world->createBody(
        {box, core::MotionType::dynamicBody, {{-2, 3, 0}, {}}, 1, admittedFilter});
    const auto rejected =
        world->createBody({box, core::MotionType::dynamicBody, {{2, 3, 0}, {}}, 1, rejectedFilter});
    for (int step = 0; step != 240; ++step) {
        world->step(core::PhysicsWorld::Duration{1.0 / 120.0});
        world->takeChangedBodyStates();
    }
    const double admittedY = world->bodyState(admitted).transform.translation.y;
    const double rejectedY = world->bodyState(rejected).transform.translation.y;
    Check(std::isfinite(admittedY) && std::abs(admittedY - 1) < 0.03 && std::isfinite(rejectedY) &&
              rejectedY < -1,
          "same-group bodies must retain distinct masks in one world");
}
#endif
} // namespace

int
main()
{
    try {
        const auto stage = pxr::UsdStage::CreateInMemory();
        auto a = stage->DefinePrim(pxr::SdfPath("/A"));
        auto b = stage->DefinePrim(pxr::SdfPath("/B"));
        const auto generic = ReadCollisionFilter(a);
        Check(generic.categories == 1 && generic.collidesWith == ~core::CollisionCategories{0},
              "conventional USD body must retain default filter");
        for (int group = 0; group != 16; ++group) {
            const auto filter = Filter(a, group, 0xffff);
            Check(filter.categories == (core::CollisionCategories{1} << group) &&
                      filter.collidesWith == 0xffff,
                  "all sixteen groups must map without inversion");
        }
        const auto high = Filter(a, 15, 1);
        const auto low = Filter(b, 0, 0x8000);
        Check(core::collisionFiltersAllowPair(high, low), "group 15 must not sign extend");
        Check(!core::collisionFiltersAllowPair(high, Filter(b, 0, 0)),
              "second body's mask alone must reject pair");
        Check(!core::collisionFiltersAllowPair(Filter(a, 15, 0), low),
              "first body's mask alone must reject pair");
        const auto sameGroup = Filter(a, 3, 8);
        Check(core::collisionFiltersAllowPair(sameGroup, Filter(b, 3, 8)) &&
                  !core::collisionFiltersAllowPair(sameGroup, Filter(b, 3, 0)),
              "bodies in the same group may carry different masks");

        for (const int group : {-1, 16}) {
            Group(a).Set(group);
            Reject(a);
        }
        Group(a).Set(0);
        for (const int mask : {-1, 65536}) {
            Mask(a).Set(mask);
            Reject(a);
        }
        Mask(a).Set(1);
        a.RemoveProperty(Group(a).GetName());
        Reject(a);
        Group(a).Set(0);
        a.RemoveProperty(Mask(a).GetName());
        Reject(a);
        const auto wrong = a.CreateAttribute(pxr::TfToken("mmd:physics:collisionMask"),
                                             pxr::SdfValueTypeNames->Float);
        wrong.Set(1.0f);
        Reject(a);
        a.RemoveProperty(wrong.GetName());
        Mask(a);
        Reject(a); // Declared with no value.
        Mask(a).Set(1);
        Group(a).Set(0, pxr::UsdTimeCode(1));
        Reject(a);
        Group(a).ClearAtTime(pxr::UsdTimeCode(1));
        Mask(a).Set(1, pxr::UsdTimeCode(1));
        Reject(a);
        Mask(a).ClearAtTime(pxr::UsdTimeCode(1));
        std::string before, after;
        stage->GetRootLayer()->ExportToString(&before);
        ReadCollisionFilter(a);
        stage->GetRootLayer()->ExportToString(&after);
        Check(before == after, "conversion must not edit the stage");

#ifdef MMD_PHYSICS_EXECUTION
        Drop(Filter(a, 15, 1), Filter(b, 0, 0x8000), true);
        Drop(Filter(a, 15, 0), Filter(b, 0, 0x8000), false);
        Drop(Filter(a, 15, 1), Filter(b, 0, 0), false);
        Drop(Filter(a, 3, 8), Filter(b, 3, 8), true);
        Drop(Filter(a, 3, 8), Filter(b, 3, 0), false);
        MixedMasks(Filter(a, 15, 8), Filter(b, 3, 0x8000), Filter(b, 3, 0));
        std::cout << "contact controls: 6 passed\n";
#endif
        std::cout << "MMD collision filter controls passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
