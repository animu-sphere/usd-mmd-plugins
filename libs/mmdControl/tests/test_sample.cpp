// SPDX-License-Identifier: Apache-2.0
//
// Sampling one track at a time (MOTION_CONTRACT.md §11.2), against values
// worked out by hand: the curve progress, which key's curve a segment
// follows, holding outside the keys, and the step tracks.
#include "mmdControl/Sample.h"

#include "TestSupport.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <vector>

namespace {

using namespace mmd;
using namespace mmd::control;
using mmd::control::test::Near;
using mmd::control::test::SameRotation;

constexpr motionVmd::Bezier kLinear{20, 20, 107, 107};
// x(s) = 3s^2 - 2s^3, y(s) = 3s - 6s^2 + 4s^3: at s = 1/4, x = 0.15625 and
// y = 0.4375.
constexpr motionVmd::Bezier kSteep{0, 127, 127, 0};

void
TestBezierProgress()
{
    assert(Near(BezierProgress(kLinear, 0.25), 0.25, 1e-9));
    assert(Near(BezierProgress(kLinear, 0.0), 0.0, 1e-9));
    assert(Near(BezierProgress(kLinear, 1.0), 1.0, 1e-9));
    assert(Near(BezierProgress(kSteep, 0.15625), 0.4375, 1e-9));
    // Outside [0, 1], clamped. This curve's x is flat at s = 1 (x'(1) = 0),
    // so there double precision pins s only to about 1e-8.
    assert(Near(BezierProgress(kSteep, -3.0), 0.0, 1e-9));
    assert(Near(BezierProgress(kSteep, 7.0), 1.0, 1e-7));
    std::puts("ok  Bezier progress");
}

binding::BoneKey
Key(std::uint32_t frame, Float3 translation, Float4 rotation)
{
    binding::BoneKey key;
    key.frame = frame;
    key.translation = translation;
    key.rotation = rotation;
    key.curves = {kLinear, kLinear, kLinear, kLinear};
    return key;
}

void
TestSampleBone()
{
    const double half = std::sqrt(0.5);
    // Identity, then 90 degrees about Y.
    std::vector<binding::BoneKey> keys{Key(0, {0, 0, 0}, {0, 0, 0, 1}),
                                       Key(10, {1, 2, 3}, {0, static_cast<float>(half), 0,
                                                           static_cast<float>(half)})};

    BoneSample s = SampleBone(keys, 5.0);
    assert(Near(s.translation, {0.5, 1.0, 1.5}, 1e-9));
    assert(SameRotation(s.rotation, test::AboutY(std::numbers::pi / 4), 1e-6));

    // Held outside the keys, exact at a key.
    s = SampleBone(keys, -3.0);
    assert(Near(s.translation, {0, 0, 0}, 0.0) && s.rotation == kIdentity);
    s = SampleBone(keys, 20.0);
    assert(Near(s.translation, {1, 2, 3}, 1e-7));
    s = SampleBone(keys, 10.0);
    assert(Near(s.translation, {1, 2, 3}, 1e-7));

    // The segment follows the *later* key's curve, per channel.
    keys[1].curves[motionVmd::kX] = kSteep;
    keys[0].curves[motionVmd::kY] = kSteep; // the earlier key's: not used
    s = SampleBone(keys, 1.5625);
    assert(Near(s.translation[0], 0.4375, 1e-8));
    assert(Near(s.translation[1], 0.15625 * 2.0, 1e-8));

    // A rotation is normalized; a zero one is identity.
    std::vector<binding::BoneKey> scaled{Key(0, {0, 0, 0}, {0, 0, 0, 2})};
    assert(SampleBone(scaled, 0.0).rotation == kIdentity);
    scaled[0].rotation = {0, 0, 0, 0};
    assert(SampleBone(scaled, 0.0).rotation == kIdentity);

    // No keys, no motion.
    s = SampleBone({}, 3.0);
    assert(s.translation == Double3{} && s.rotation == kIdentity);
    std::puts("ok  bone tracks");
}

void
TestSampleMorph()
{
    const std::vector<motionVmd::MorphKey> keys{{10, 0.0f}, {20, 1.0f}};
    assert(Near(SampleMorph(keys, 15.0), 0.5, 1e-12));
    assert(SampleMorph(keys, 0.0) == 0.0);
    assert(SampleMorph(keys, 25.0) == 1.0);
    assert(SampleMorph({}, 5.0) == 0.0);
    std::puts("ok  morph tracks");
}

void
TestSteps()
{
    const std::vector<motionVmd::IkKey> ik{{10, false}, {20, true}};
    assert(!SampleIk(ik, 0.0)); // before the first key: the first key's
    assert(!SampleIk(ik, 10.0));
    assert(!SampleIk(ik, 19.9));
    assert(SampleIk(ik, 20.0));
    assert(SampleIk({}, 5.0));

    const std::vector<motionVmd::VisibilityKey> visibility{{0, true}, {30, false}};
    assert(SampleVisibility(visibility, 29.0));
    assert(!SampleVisibility(visibility, 30.0));
    assert(SampleVisibility({}, 5.0));
    std::puts("ok  IK and visibility steps");
}

} // namespace

void
RunSampleTests()
{
    TestBezierProgress();
    TestSampleBone();
    TestSampleMorph();
    TestSteps();
}
