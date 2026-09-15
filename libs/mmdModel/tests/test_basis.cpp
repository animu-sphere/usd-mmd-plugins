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

    const Float2 st = basis::St({0.25f, 0.125f});
    assert(st[0] == 0.25f && st[1] == 0.875f);
    assert(PositiveZero(basis::St({0.0f, 1.0f})[1]));

    const auto tri = basis::Triangle(4, 5, 6);
    assert(tri[0] == 4 && tri[1] == 6 && tri[2] == 5);
}

}  // namespace

void
TestBasis()
{
    TestPointsAndDisplacements();
    TestDirections();
    TestRotationsAndTexCoords();
}
