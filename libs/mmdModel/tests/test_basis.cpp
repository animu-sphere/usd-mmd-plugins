// SPDX-License-Identifier: Apache-2.0
//
// The conversion functions of STAGE_CONTRACT.md §6.3, one asymmetric value
// through each, so a swapped axis or a missing sign cannot cancel out.
#include "mmdModel/Basis.h"

#include <cassert>
#include <cmath>
#include <cstring>

namespace {

using namespace mmd;

/// `v` in MMD units, as meters: the one scaling rule, restated.
float
M(double v)
{
    return static_cast<float>(v * 0.08);
}

bool
PositiveZero(float v)
{
    return v == 0.0f && !std::signbit(v);
}

void
TestPointsAndDisplacements()
{
    assert(basis::kMetersPerUnit == 0.08);
    const Float3 p = basis::Point({1.0f, 2.0f, 3.0f});
    assert(p[0] == M(1.0) && p[1] == M(2.0) && p[2] == -M(3.0));
    // A 20-unit model is 1.6 m tall: the reason for the scale.
    assert(std::abs(basis::Point({0.0f, 20.0f, 0.0f})[1] - 1.6f) < 1e-6f);
    // The mirror of zero is zero, not negative zero, so "-0" is never authored.
    assert(PositiveZero(basis::Point({0.0f, 0.0f, 0.0f})[2]));
    assert(PositiveZero(basis::Point({-0.0f, 0.0f, 0.0f})[0]));
    assert(basis::Displacement({1.0f, 2.0f, 3.0f}) == p);
}

void
TestDirections()
{
    // A model faces -Z in PMX and +Z on the stage.
    const Float3 front = basis::Direction({0.0f, 0.0f, -1.0f});
    assert(front[0] == 0.0f && front[1] == 0.0f && front[2] == 1.0f);
    assert(PositiveZero(front[0]));
    // Renormalized, and not scaled.
    const Float3 d = basis::Direction({3.0f, 0.0f, 4.0f});
    assert(d[0] == 0.6f && d[1] == 0.0f && d[2] == -0.8f);
    // A zero normal stays zero rather than becoming NaN.
    const Float3 zero = basis::Direction({0.0f, 0.0f, 0.0f});
    assert(zero[0] == 0.0f && zero[1] == 0.0f && PositiveZero(zero[2]));
}

void
TestRotationsAndTexCoords()
{
    const Float4 q = basis::Quaternion({0.1f, 0.2f, 0.3f, 0.9f});
    assert(q[0] == -0.1f && q[1] == -0.2f && q[2] == 0.3f && q[3] == 0.9f);

    // An axial vector -- a torque -- reverses about X and Y, keeps Z, and is
    // never scaled: it is not a length.
    const Float3 torque = basis::AxialVector({0.1f, 0.2f, 0.3f});
    assert(torque[0] == -0.1f && torque[1] == -0.2f && torque[2] == 0.3f);
    assert(PositiveZero(basis::AxialVector({0.0f, 0.0f, 0.0f})[0]));

    // A local frame converts as the rotation it makes up: X as a direction,
    // Z as an axial vector, neither scaled nor normalized -- so the identity
    // frame stays the identity, and Y = Z x X still holds.
    const basis::LocalAxes frame = basis::Frame({2.0f, 0.5f, 0.25f}, {0.1f, 0.2f, 0.3f});
    assert(frame.x[0] == 2.0f && frame.x[1] == 0.5f && frame.x[2] == -0.25f);
    assert(frame.z[0] == -0.1f && frame.z[1] == -0.2f && frame.z[2] == 0.3f);
    const basis::LocalAxes identity = basis::Frame({1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
    assert(identity.x[0] == 1.0f && PositiveZero(identity.x[2]));
    assert(PositiveZero(identity.z[0]) && PositiveZero(identity.z[1]) && identity.z[2] == 1.0f);

    // Rotation limits: about X and Y [min, max] -> [-max, -min]; Z unchanged.
    const basis::RotationLimits limits =
        basis::Limits({-3.0f, -2.0f, -1.0f}, {0.5f, 0.25f, 0.125f});
    assert(limits.lower[0] == -0.5f && limits.lower[1] == -0.25f && limits.lower[2] == -1.0f);
    assert(limits.upper[0] == 3.0f && limits.upper[1] == 2.0f && limits.upper[2] == 0.125f);
    assert(PositiveZero(basis::Limits({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}).lower[0]));

    const Float2 st = basis::St({0.25f, 0.125f});
    assert(st[0] == 0.25f && st[1] == 0.875f);
    assert(PositiveZero(basis::St({0.0f, 1.0f})[1]));

    const auto tri = basis::Triangle(4, 5, 6);
    assert(tri[0] == 4 && tri[1] == 6 && tri[2] == 5);
}

} // namespace

void
TestBasis()
{
    TestPointsAndDisplacements();
    TestDirections();
    TestRotationsAndTexCoords();
}
