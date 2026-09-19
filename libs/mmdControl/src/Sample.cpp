// SPDX-License-Identifier: Apache-2.0
#include "mmdControl/Sample.h"

#include "Algebra.h"

#include <algorithm>

namespace mmd::control {

namespace {

double
Cubic(double p1, double p2, double s)
{
    const double r = 1.0 - s;
    return 3.0 * r * r * s * p1 + 3.0 * r * s * s * p2 + s * s * s;
}

Quat
ToQuat(const Float4& q)
{
    return detail::Normalize({q[0], q[1], q[2], q[3]});
}

/// The index of the first key whose frame is after `frame`: keys.size() when
/// none is, 0 when every one is.
template <class Key>
std::size_t
UpperBound(std::span<const Key> keys, double frame)
{
    const auto it = std::upper_bound(
        keys.begin(), keys.end(), frame, [](double f, const Key& key) {
            return f < static_cast<double>(key.frame);
        });
    return static_cast<std::size_t>(it - keys.begin());
}

/// A step track (MOTION_CONTRACT.md §11.2): the last key at or before
/// `frame`, else the first.
template <class Key, class Value>
Value
Step(std::span<const Key> keys, double frame, Value Key::*member, Value empty)
{
    if (keys.empty()) {
        return empty;
    }
    const std::size_t upper = UpperBound(keys, frame);
    return keys[upper == 0 ? 0 : upper - 1].*member;
}

} // namespace

double
BezierProgress(const motionVmd::Bezier& curve, double u)
{
    u = std::clamp(u, 0.0, 1.0);
    const double x1 = curve.x1 / 127.0;
    const double y1 = curve.y1 / 127.0;
    const double x2 = curve.x2 / 127.0;
    const double y2 = curve.y2 / 127.0;
    double lo = 0.0;
    double hi = 1.0;
    for (int i = 0; i < 32; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (Cubic(x1, x2, mid) < u) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return Cubic(y1, y2, 0.5 * (lo + hi));
}

BoneSample
SampleBone(std::span<const binding::BoneKey> keys, double frame)
{
    if (keys.empty()) {
        return {};
    }
    const auto at = [](const binding::BoneKey& key) {
        return BoneSample{{key.translation[0], key.translation[1], key.translation[2]},
                          ToQuat(key.rotation)};
    };
    const std::size_t upper = UpperBound(keys, frame);
    if (upper == 0) {
        return at(keys.front());
    }
    const binding::BoneKey& k0 = keys[upper - 1];
    if (upper == keys.size() || frame == static_cast<double>(k0.frame)) {
        return at(k0);
    }
    const binding::BoneKey& k1 = keys[upper];
    const double f0 = k0.frame;
    const double u = (frame - f0) / (static_cast<double>(k1.frame) - f0);

    BoneSample sample;
    for (int axis = 0; axis < 3; ++axis) {
        const double p = BezierProgress(k1.curves[axis], u);
        sample.translation[axis] =
            k0.translation[axis] + (static_cast<double>(k1.translation[axis]) - k0.translation[axis]) * p;
    }
    sample.rotation = detail::Slerp(ToQuat(k0.rotation),
                                    ToQuat(k1.rotation),
                                    BezierProgress(k1.curves[motionVmd::kRotation], u));
    return sample;
}

double
SampleMorph(std::span<const motionVmd::MorphKey> keys, double frame)
{
    if (keys.empty()) {
        return 0.0;
    }
    const std::size_t upper = UpperBound(keys, frame);
    if (upper == 0) {
        return keys.front().weight;
    }
    const motionVmd::MorphKey& k0 = keys[upper - 1];
    if (upper == keys.size() || frame == static_cast<double>(k0.frame)) {
        return k0.weight;
    }
    const motionVmd::MorphKey& k1 = keys[upper];
    const double f0 = k0.frame;
    const double u = (frame - f0) / (static_cast<double>(k1.frame) - f0);
    return k0.weight + (static_cast<double>(k1.weight) - k0.weight) * u;
}

bool
SampleIk(std::span<const motionVmd::IkKey> keys, double frame)
{
    return Step(keys, frame, &motionVmd::IkKey::enabled, true);
}

bool
SampleVisibility(std::span<const motionVmd::VisibilityKey> keys, double frame)
{
    return Step(keys, frame, &motionVmd::VisibilityKey::visible, true);
}

} // namespace mmd::control
