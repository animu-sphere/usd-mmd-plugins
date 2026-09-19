// SPDX-License-Identifier: Apache-2.0
//
// The double-precision vector and quaternion arithmetic evaluation is written
// in (docs/design/MOTION_CONTRACT.md §11). Quaternions are (x, y, z, w),
// Hamilton products acting on column vectors: Multiply(a, b) applies b first.
#pragma once

#include "mmdControl/Sample.h"

#include <cmath>

namespace mmd::control::detail {

using Vec3 = Double3;

inline Vec3
Add(const Vec3& a, const Vec3& b)
{
    return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}

inline Vec3
Sub(const Vec3& a, const Vec3& b)
{
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

inline Vec3
Scale(const Vec3& v, double s)
{
    return {v[0] * s, v[1] * s, v[2] * s};
}

inline double
Dot(const Vec3& a, const Vec3& b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

inline Vec3
Cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

inline double
Length(const Vec3& v)
{
    return std::sqrt(Dot(v, v));
}

inline Quat
Multiply(const Quat& a, const Quat& b)
{
    return {a[3] * b[0] + b[3] * a[0] + (a[1] * b[2] - a[2] * b[1]),
            a[3] * b[1] + b[3] * a[1] + (a[2] * b[0] - a[0] * b[2]),
            a[3] * b[2] + b[3] * a[2] + (a[0] * b[1] - a[1] * b[0]),
            a[3] * b[3] - (a[0] * b[0] + a[1] * b[1] + a[2] * b[2])};
}

/// The inverse of a unit quaternion.
inline Quat
Conjugate(const Quat& q)
{
    return {-q[0], -q[1], -q[2], q[3]};
}

/// `v` rotated by the unit quaternion `q`.
inline Vec3
Rotate(const Quat& q, const Vec3& v)
{
    const Vec3 u{q[0], q[1], q[2]};
    const Vec3 t = Scale(Cross(u, v), 2.0);
    return Add(Add(v, Scale(t, q[3])), Cross(u, t));
}

/// `q` scaled to unit length; a zero quaternion is the identity.
inline Quat
Normalize(const Quat& q)
{
    const double length = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (length == 0.0) {
        return kIdentity;
    }
    return {q[0] / length, q[1] / length, q[2] / length, q[3] / length};
}

/// The rotation by `angle` radians about the unit vector `axis`.
inline Quat
AxisAngle(const Vec3& axis, double angle)
{
    const double s = std::sin(0.5 * angle);
    return {axis[0] * s, axis[1] * s, axis[2] * s, std::cos(0.5 * angle)};
}

/// The rotation about `q`'s axis by `ratio` times its angle, the angle taken
/// in [0, pi]: slerp(1, q, ratio), extended to any ratio
/// (MOTION_CONTRACT.md §11.3, §11.6).
inline Quat
ScaleRotation(const Quat& q, double ratio)
{
    const double sign = q[3] < 0.0 ? -1.0 : 1.0;
    const Vec3 v{sign * q[0], sign * q[1], sign * q[2]};
    const double s = Length(v);
    if (s == 0.0) {
        return kIdentity;
    }
    const double angle = 2.0 * std::atan2(s, sign * q[3]);
    return AxisAngle(Scale(v, 1.0 / s), ratio * angle);
}

/// The shortest-path spherical interpolation from `a` to `b` by `t`,
/// normalized.
inline Quat
Slerp(const Quat& a, Quat b, double t)
{
    double cosine = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    if (cosine < 0.0) {
        cosine = -cosine;
        b = {-b[0], -b[1], -b[2], -b[3]};
    }
    double wa = 1.0 - t;
    double wb = t;
    if (cosine < 1.0 - 1e-9) {
        const double theta = std::acos(cosine);
        const double sine = std::sin(theta);
        wa = std::sin((1.0 - t) * theta) / sine;
        wb = std::sin(t * theta) / sine;
    }
    return Normalize({wa * a[0] + wb * b[0],
                      wa * a[1] + wb * b[1],
                      wa * a[2] + wb * b[2],
                      wa * a[3] + wb * b[3]});
}

/// A rigid transform: a point p maps to rotation * p + translation.
struct Transform {
    Vec3 translation{};
    Quat rotation = kIdentity;
};

/// `parent` composed with `local`: the local transform applied first.
inline Transform
Compose(const Transform& parent, const Transform& local)
{
    return {Add(parent.translation, Rotate(parent.rotation, local.translation)),
            Multiply(parent.rotation, local.rotation)};
}

/// The model-space point `p` in the frame `frame`.
inline Vec3
ToFrame(const Transform& frame, const Vec3& p)
{
    return Rotate(Conjugate(frame.rotation), Sub(p, frame.translation));
}

} // namespace mmd::control::detail
