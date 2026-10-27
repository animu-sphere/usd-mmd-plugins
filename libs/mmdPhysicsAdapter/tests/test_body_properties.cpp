// SPDX-License-Identifier: Apache-2.0
#include <mmdPhysicsAdapter/BodyProperties.h>
#include <mmdPhysicsAdapter/Codes.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/stage.h>
#include <cassert>
#include <algorithm>
#include <cmath>
#include <limits>

using namespace pxr;
using namespace mmd;
namespace {
struct Fixture {
    UsdStageRefPtr stage = UsdStage::CreateInMemory();
    UsdPrim body = stage->DefinePrim(SdfPath("/Body"));
    template <class T> UsdAttribute Attr(const char* name, const SdfValueTypeName& type, T value)
    {
        auto a = body.CreateAttribute(
            TfToken(std::string("mmd:physics:") + name), type, true, SdfVariabilityUniform);
        a.Set(value);
        return a;
    }
    void Filter(int group = 3, int mask = 65535)
    {
        Attr("collisionGroup", SdfValueTypeNames->Int, group);
        Attr("collisionMask", SdfValueTypeNames->Int, mask);
    }
    void Contact(float friction = 2, float restitution = .5f)
    {
        Attr("friction", SdfValueTypeNames->Float, friction);
        Attr("restitution", SdfValueTypeNames->Float, restitution);
    }
    void Damping(float linear = .5f, float angular = .75f)
    {
        Attr("linearDamping", SdfValueTypeNames->Float, linear);
        Attr("angularDamping", SdfValueTypeNames->Float, angular);
    }
    void Refused(const Code& code = codes::PhysicsMetadataInvalid)
    {
        auto result = physics::ReadBodyProperties(body);
        assert(!result && result.fatal()->code == code.id);
        assert(result.fatal()->message.find("/Body") != std::string::npos);
    }
};
} // namespace
int
main()
{
    // The source-policy resolver is separate from the existing strict
    // finite-rate stage reader. Endpoints must never become a 0.99 fallback.
    for (float source : {-2.f, 0.f, .25f, .5f, .99f, std::nextafter(1.f, 0.f), 1.f, 2.f}) {
        const auto resolved = physics::ResolveSourceDamping(source);
        assert(resolved);
        const auto& value = resolved.value();
        const double effective = std::max(0., std::min(1., double(source)));
        assert(value.coefficient == effective);
        assert(value.clamped == (source < 0 || source > 1));
        if (effective == 1) {
            assert(value.mode == physics::SourceDampingMode::ClearPriorVelocity);
            assert(value.rate == 0);
        } else {
            assert(value.mode == physics::SourceDampingMode::FiniteRate && std::isfinite(value.rate));
            for (double dt : {1./60, 1./120, 1./240})
                assert(std::abs(std::exp(-value.rate*dt) - std::pow(1-effective, dt)) < 1e-12);
        }
    }
    for (float source : {std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::quiet_NaN()})
        assert(!physics::ResolveSourceDamping(source));
    assert(!physics::ReadBodyProperties({}));
    Fixture empty;
    auto conventional = physics::ReadBodyProperties(empty.body);
    assert(conventional && !conventional.value().filter && !conventional.value().contact &&
           !conventional.value().damping);
    for (int group = 0; group < 16; ++group)
        for (int mask : {0, 1, 32768, 65535}) {
            Fixture f;
            f.Filter(group, mask);
            f.Contact();
            f.Damping();
            std::string before, after;
            f.stage->GetRootLayer()->ExportToString(&before);
            auto result = physics::ReadBodyProperties(f.body);
            assert(result);
            const auto& p = result.value();
            assert(p.filter->category == (uint32_t{1} << group) &&
                   p.filter->mask == uint32_t(mask));
            assert(p.contact->friction == 2 && p.contact->restitution == .5);
            assert(std::abs(std::exp(-p.damping->linear) - .5) < 1e-12);
            assert(std::abs(std::exp(-p.damping->angular) - .25) < 1e-12);
            f.stage->GetRootLayer()->ExportToString(&after);
            assert(before == after);
        }
    for (int g : {-1, 16}) {
        Fixture f;
        f.Filter(g);
        f.Refused();
    }
    for (int m : {-1, 65536}) {
        Fixture f;
        f.Filter(0, m);
        f.Refused();
    }
    for (float bad :
         {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        Fixture f;
        f.Contact(bad, 0);
        f.Refused();
        Fixture r;
        r.Contact(0, bad);
        r.Refused();
        Fixture l;
        l.Damping(bad, 0);
        l.Refused();
        Fixture a;
        a.Damping(0, bad);
        a.Refused();
    }
    {
        Fixture f;
        f.Contact(0, 1.1f);
        f.Refused();
    }
    {
        Fixture f;
        f.Damping(1.1f, 0);
        f.Refused();
    }
    {
        Fixture f;
        f.Damping(0, 1.1f);
        f.Refused();
    }
    for (bool angular : {false, true}) {
        Fixture f;
        f.Damping(angular ? 0.f : 1.f, angular ? 1.f : 0.f);
        f.Refused(codes::PhysicsDampingUnsupported);
    }
    {
        Fixture f;
        f.Damping(0, std::nextafter(1.f, 0.f));
        auto r = physics::ReadBodyProperties(f.body);
        assert(r && r.value().damping->linear == 0 && std::isfinite(r.value().damping->angular));
    }
    // Every field: partial, missing/blocked, wrong type, sampled and varying.
    for (const char* name : {"collisionGroup",
                             "collisionMask",
                             "friction",
                             "restitution",
                             "linearDamping",
                             "angularDamping"}) {
        const TfToken token(std::string("mmd:physics:") + name);
        for (int mode = 0; mode < 6; ++mode) {
            Fixture f;
            f.Filter();
            f.Contact();
            f.Damping();
            auto a = f.body.GetAttribute(token);
            if (mode == 0)
                f.body.RemoveProperty(token);
            if (mode == 1)
                a.Clear();
            if (mode == 2)
                a.Block();
            if (mode == 3) {
                f.body.RemoveProperty(token);
                f.body.CreateAttribute(token, SdfValueTypeNames->String).Set(std::string("bad"));
            }
            if (mode == 4) {
                if (a.GetTypeName() == SdfValueTypeNames->Int)
                    a.Set(0, 1);
                else
                    a.Set(0.f, 1);
            }
            if (mode == 5)
                a.SetVariability(SdfVariabilityVarying);
            f.Refused();
        }
    }
    // Pairs are independently optional; no owner defaults leak into conventional bodies.
    {
        Fixture f;
        f.Filter();
        auto r = physics::ReadBodyProperties(f.body);
        assert(r && r.value().filter && !r.value().contact && !r.value().damping);
    }
    {
        Fixture f;
        f.Contact(0, 1);
        auto r = physics::ReadBodyProperties(f.body);
        assert(r && !r.value().filter && r.value().contact && !r.value().damping);
    }
    {
        Fixture f;
        f.Damping(0, 0);
        auto r = physics::ReadBodyProperties(f.body);
        assert(r && !r.value().filter && !r.value().contact && r.value().damping);
    }
}
