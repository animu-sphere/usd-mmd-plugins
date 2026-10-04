// SPDX-License-Identifier: Apache-2.0
//
// Comparisons and rotations the mmdControl tests share.
#pragma once

#include "mmdControl/Sample.h"

#include <cmath>

namespace mmd::control::test {

inline bool
Near(double a, double b, double tolerance)
{
    return std::abs(a - b) <= tolerance;
}

inline bool
Near(const Double3& a, const Double3& b, double tolerance)
{
    return Near(a[0], b[0], tolerance) && Near(a[1], b[1], tolerance) &&
           Near(a[2], b[2], tolerance);
}

/// q and -q are one rotation.
inline bool
SameRotation(const Quat& a, const Quat& b, double tolerance)
{
    const double dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    return std::abs(std::abs(dot) - 1.0) <= tolerance;
}

inline Quat
About(double x, double y, double z, double angle)
{
    const double s = std::sin(0.5 * angle);
    return {x * s, y * s, z * s, std::cos(0.5 * angle)};
}

inline Quat AboutX(double angle) { return About(1, 0, 0, angle); }
inline Quat AboutY(double angle) { return About(0, 1, 0, angle); }
inline Quat AboutZ(double angle) { return About(0, 0, 1, angle); }

inline Float4
ToFloat4(const Quat& q)
{
    return {static_cast<float>(q[0]), static_cast<float>(q[1]), static_cast<float>(q[2]),
            static_cast<float>(q[3])};
}

/// `a · b`: `b` applied first.
inline Quat
Multiply(const Quat& a, const Quat& b)
{
    return {a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
            a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
            a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
            a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
}

inline Quat
Normalized(const Quat& q)
{
    const double length = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    return {q[0] / length, q[1] / length, q[2] / length, q[3] / length};
}

/// A key's float rotation, as sampling reads it.
inline Quat
ToQuat(const Float4& q)
{
    return {q[0], q[1], q[2], q[3]};
}

} // namespace mmd::control::test
