// SPDX-License-Identifier: Apache-2.0
//
// Unit quaternions in double, for the rotations canonicalization composes:
// a rigid body's or a joint's frame from its Euler angles, and a joint's
// frame relative to each body's (docs/design/STAGE_CONTRACT.md §6.3, §13).
// Every value is rounded to float once, at the end.
#pragma once

#include "mmdModel/CanonicalDocument.h"

#include <array>

namespace mmd::detail {

/// (x, y, z, w), Hamilton product, acting on column vectors: q * p applies p
/// first.
using QuatD = std::array<double, 4>;

inline QuatD
Multiply(const QuatD& a, const QuatD& b)
{
    return {a[3] * b[0] + b[3] * a[0] + (a[1] * b[2] - a[2] * b[1]),
            a[3] * b[1] + b[3] * a[1] + (a[2] * b[0] - a[0] * b[2]),
            a[3] * b[2] + b[3] * a[2] + (a[0] * b[1] - a[1] * b[0]),
            a[3] * b[3] - (a[0] * b[0] + a[1] * b[1] + a[2] * b[2])};
}

inline QuatD
Conjugate(const QuatD& q)
{
    return {-q[0], -q[1], -q[2], q[3]};
}

/// `v` rotated by the unit quaternion `q`.
inline Double3
Rotate(const QuatD& q, const Double3& v)
{
    const QuatD rotated = Multiply(Multiply(q, {v[0], v[1], v[2], 0.0}), Conjugate(q));
    return {rotated[0], rotated[1], rotated[2]};
}

/// `q` rounded to float, with w made non-negative -- q and -q are the same
/// rotation, and one of them is chosen so the stage is the same for both --
/// and every zero positive.
inline Float4
Rounded(const QuatD& q)
{
    const double sign = q[3] < 0.0 ? -1.0 : 1.0;
    return {static_cast<float>(sign * q[0]) + 0.0f,
            static_cast<float>(sign * q[1]) + 0.0f,
            static_cast<float>(sign * q[2]) + 0.0f,
            static_cast<float>(sign * q[3]) + 0.0f};
}

} // namespace mmd::detail
