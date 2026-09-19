// SPDX-License-Identifier: Apache-2.0
//
// Sampling a bound motion's tracks at an explicit time
// (docs/design/MOTION_CONTRACT.md §11.2). Each function samples one track on
// its own, at a frame of MMD's 30-per-second grid, fractional allowed; the
// Evaluator composes them.
#pragma once

#include <mmdMotionBinding/Bind.h>
#include <motionVmd/Motion.h>

#include <array>
#include <span>

namespace mmd::control {

/// A unit quaternion (x, y, z, w), in double: a rotation acting on column
/// vectors, where a * b applies b first.
using Quat = std::array<double, 4>;

inline constexpr Quat kIdentity{0.0, 0.0, 0.0, 1.0};

/// The progress a VMD curve gives at time fraction `u` in [0, 1]: its control
/// points divided by 127, the parameter s with x(s) = u found by 32
/// bisections of [0, 1], and y(s). `u` outside [0, 1] is clamped into it.
double BezierProgress(const motionVmd::Bezier& curve, double u);

/// A bone track's motion at `frame`: the translation from the joint's rest
/// position and the rotation relative to its rest orientation. Before the
/// first key or after the last the track holds that key; between two keys
/// each channel follows the later key's curve for it -- translation per axis
/// linearly, rotation by the shortest-path slerp, normalized. An empty track
/// is no motion.
struct BoneSample {
    Double3 translation{};
    Quat rotation = kIdentity;
};
BoneSample SampleBone(std::span<const binding::BoneKey> keys, double frame);

/// A morph track's weight at `frame`: linear between keys, held outside them.
/// An empty track weighs 0.
double SampleMorph(std::span<const motionVmd::MorphKey> keys, double frame);

/// An IK track's state at `frame`: the last key at or before it, else the
/// first key. An empty track is enabled.
bool SampleIk(std::span<const motionVmd::IkKey> keys, double frame);

/// The visibility track at `frame`, as an IK track is stepped. An empty track
/// is visible.
bool SampleVisibility(std::span<const motionVmd::VisibilityKey> keys, double frame);

} // namespace mmd::control
