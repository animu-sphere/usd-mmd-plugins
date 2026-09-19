// SPDX-License-Identifier: Apache-2.0
//
// Evaluation is total over every model canonicalization can return and every
// motion binding can: generated rigs whose appends and IK chains name any
// joint -- themselves, their descendants, one another -- with loop counts,
// angle limits and link limits that are negative, huge or NaN; group morphs
// that nest and cycle before canonicalization drops the cycles; and motions
// whose keys hold any float. Each case is evaluated at several times, twice,
// and the two poses must agree bit for bit (MOTION_CONTRACT.md §11.1). A
// tame subset -- finite, moderate values -- must also give finite
// translations and unit rotations. The generator is a fixed-seed PRNG with
// its own arithmetic, so the run is the same on every platform.
#include "mmdControl/Evaluator.h"

#include <mmdModel/Canonicalize.h>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

namespace {

using namespace mmd;
using namespace mmd::control;
namespace pmx = mmd::pmx;

/// SplitMix64, as mmdModel's robustness suite uses.
class Random {
public:
    explicit Random(std::uint64_t seed) : _state(seed) {}

    std::uint64_t Next()
    {
        std::uint64_t z = (_state += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    std::size_t Below(std::size_t n) { return n == 0 ? 0 : static_cast<std::size_t>(Next() % n); }
    bool OneIn(std::size_t n) { return Below(n) == 0; }

    /// [-1, 1), in steps of 2^-20.
    float Unit() { return static_cast<float>(static_cast<std::int64_t>(Below(1u << 21)) - (1 << 20)) / (1 << 20); }

private:
    std::uint64_t _state;
};

float
Float(Random& r, bool tame)
{
    if (!tame) {
        switch (r.Below(12)) {
        case 0:
            return std::numeric_limits<float>::quiet_NaN();
        case 1:
            return std::numeric_limits<float>::infinity();
        case 2:
            return -std::numeric_limits<float>::max();
        case 3:
            return 1e30f;
        case 4:
            return 0.0f;
        default:
            break;
        }
    }
    return r.Unit() * (tame ? 4.0f : 1000.0f);
}

pmx::Vec3
Vec3(Random& r, bool tame)
{
    return {Float(r, tame), Float(r, tame), Float(r, tame)};
}

std::int32_t
Index(Random& r, std::size_t size)
{
    return r.OneIn(5) || size == 0 ? pmx::kNoIndex : static_cast<std::int32_t>(r.Below(size));
}

pmx::Document
Generate(Random& r, bool tame)
{
    using pmx::BoneFlag;
    pmx::Document doc;
    const std::size_t bones = 1 + r.Below(24);
    for (std::size_t i = 0; i < bones; ++i) {
        pmx::Bone b;
        b.name = "b" + std::to_string(i);
        b.position = Vec3(r, tame);
        b.parent = Index(r, bones);
        b.transformLayer = static_cast<std::int32_t>(r.Below(4)) - (tame ? 0 : 1);
        b.flags = static_cast<std::uint16_t>(r.Next() & 0x3FFF);
        if (b.flags & (BoneFlag::AppendRotation | BoneFlag::AppendTranslation)) {
            b.appendParent = Index(r, bones);
            b.appendRatio = tame ? r.Unit() * 2.0f : Float(r, false);
        }
        if (b.flags & BoneFlag::Ik) {
            b.ik.target = Index(r, bones);
            b.ik.loopCount = tame ? static_cast<std::int32_t>(r.Below(60))
                                  : static_cast<std::int32_t>(r.Next());
            b.ik.limitAngle = tame ? std::abs(r.Unit()) * 3.0f : Float(r, false);
            const std::size_t links = r.Below(5);
            for (std::size_t k = 0; k < links; ++k) {
                pmx::IkLink link;
                link.bone = Index(r, bones);
                link.hasLimits = r.OneIn(2);
                if (link.hasLimits) {
                    link.lowerLimit = Vec3(r, tame);
                    link.upperLimit = Vec3(r, tame);
                    if (r.OneIn(2)) { // a plane link
                        const std::size_t keep = r.Below(3);
                        for (std::size_t a = 0; a < 3; ++a) {
                            if (a != keep) {
                                link.lowerLimit[a] = link.upperLimit[a] = 0.0f;
                            }
                        }
                    }
                }
                b.ik.links.push_back(link);
            }
        }
        if (b.flags & BoneFlag::ExternalParent) {
            b.externalParentKey = static_cast<std::int32_t>(r.Below(8));
        }
        doc.bones.push_back(std::move(b));
    }
    const std::size_t morphs = r.Below(10);
    for (std::size_t i = 0; i < morphs; ++i) {
        pmx::Morph m;
        m.name = "m" + std::to_string(i);
        m.panel = 1;
        switch (r.Below(4)) {
        case 0:
            m.type = pmx::MorphType::Bone;
            for (std::size_t k = r.Below(4); k > 0; --k) {
                const pmx::Vec3 v = Vec3(r, tame);
                m.boneOffsets.push_back(pmx::BoneOffset{
                    static_cast<std::int32_t>(r.Below(bones)), Vec3(r, tame), {v[0], v[1], v[2], Float(r, tame)}});
            }
            break;
        case 1:
        case 2:
            m.type = pmx::MorphType::Group;
            for (std::size_t k = r.Below(4); k > 0; --k) {
                m.groupOffsets.push_back(pmx::GroupOffset{static_cast<std::int32_t>(r.Below(morphs)),
                                                          tame ? r.Unit() : Float(r, false)});
            }
            break;
        default:
            m.type = r.OneIn(2) ? pmx::MorphType::Vertex : pmx::MorphType::Flip;
            break;
        }
        doc.morphs.push_back(std::move(m));
    }
    return doc;
}

binding::BoundMotion
Motion(Random& r, const CanonicalDocument& model, bool tame)
{
    binding::BoundMotion motion;
    const std::size_t joints = model.skeleton.bones.size();
    for (std::size_t j = 0; j < joints; ++j) {
        if (r.OneIn(3)) {
            continue;
        }
        binding::BoneTrack track;
        track.joint = static_cast<std::int32_t>(j);
        std::uint32_t frame = static_cast<std::uint32_t>(r.Below(5));
        for (std::size_t k = 1 + r.Below(4); k > 0; --k) {
            binding::BoneKey key;
            key.frame = frame;
            frame += 1 + static_cast<std::uint32_t>(r.Below(20));
            const pmx::Vec3 t = Vec3(r, tame);
            key.translation = {t[0], t[1], t[2]};
            key.rotation = {Float(r, tame), Float(r, tame), Float(r, tame), Float(r, tame)};
            for (motionVmd::Bezier& curve : key.curves) {
                curve = {static_cast<std::uint8_t>(r.Below(128)), static_cast<std::uint8_t>(r.Below(128)),
                         static_cast<std::uint8_t>(r.Below(128)), static_cast<std::uint8_t>(r.Below(128))};
            }
            track.keys.push_back(key);
        }
        motion.bones.push_back(std::move(track));
    }
    for (std::size_t m = 0; m < model.morphs.size(); ++m) {
        if (r.OneIn(2)) {
            motion.morphs.push_back(binding::MorphTrack{
                static_cast<std::int32_t>(m), {{0, tame ? r.Unit() : Float(r, false)}, {10, r.Unit()}}});
        }
    }
    for (std::size_t j = 0; j < joints; ++j) {
        if (r.OneIn(4)) {
            motion.ik.push_back(binding::IkTrack{static_cast<std::int32_t>(j),
                                                 {{0, r.OneIn(2)}, {7, r.OneIn(2)}}});
        }
    }
    return motion;
}

/// The same bits: NaN compares equal to the same NaN.
bool
SameBits(double a, double b)
{
    return std::memcmp(&a, &b, sizeof(double)) == 0;
}

/// Bit-for-bit equality, field by field: a struct's padding bytes are
/// indeterminate, so whole structs are never compared as bytes.
bool
Identical(const Pose& a, const Pose& b)
{
    if (a.joints.size() != b.joints.size() || a.channels.size() != b.channels.size() ||
        a.visible != b.visible) {
        return false;
    }
    for (std::size_t j = 0; j < a.joints.size(); ++j) {
        for (std::size_t i = 0; i < 3; ++i) {
            if (!SameBits(a.joints[j].translation[i], b.joints[j].translation[i])) {
                return false;
            }
        }
        for (std::size_t i = 0; i < 4; ++i) {
            if (!SameBits(a.joints[j].rotation[i], b.joints[j].rotation[i])) {
                return false;
            }
        }
    }
    for (std::size_t c = 0; c < a.channels.size(); ++c) {
        if (a.channels[c].morph != b.channels[c].morph ||
            !SameBits(a.channels[c].weight, b.channels[c].weight)) {
            return false;
        }
    }
    return true;
}

const char*
Violation(const CanonicalDocument& model, const Pose& pose, bool tame)
{
    if (pose.joints.size() != model.skeleton.bones.size()) {
        return "a pose has one transform per joint";
    }
    for (std::size_t c = 0; c < pose.channels.size(); ++c) {
        const MorphChannel& channel = pose.channels[c];
        if (channel.morph < 0 || static_cast<std::size_t>(channel.morph) >= model.morphs.size() ||
            model.morphs[static_cast<std::size_t>(channel.morph)].type == MorphType::Bone) {
            return "a channel names a morph that is not a bone morph";
        }
        if (c > 0 && pose.channels[c - 1].morph >= channel.morph) {
            return "channels ascend by morph";
        }
    }
    if (!tame) {
        return nullptr;
    }
    for (const JointTransform& joint : pose.joints) {
        for (const double v : joint.translation) {
            if (!std::isfinite(v)) {
                return "a tame model and motion give finite translations";
            }
        }
        const Quat& q = joint.rotation;
        const double norm = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
        if (!(std::abs(norm - 1.0) < 1e-6)) {
            return "a tame model and motion give unit rotations";
        }
    }
    return nullptr;
}

} // namespace

int
main()
{
    Random r(0x6D6D64436F6E7472ull); // "mmdContr"
    int failures = 0;
    std::size_t evaluated = 0;
    for (int i = 0; i < 20000; ++i) {
        const bool tame = i % 2 == 0;
        auto canonical = Canonicalize(Generate(r, tame));
        if (!canonical.ok()) {
            continue;
        }
        const CanonicalDocument& model = canonical.value();
        auto prepared = Evaluator::Prepare(model);
        assert(prepared.ok());
        const Evaluator& evaluator = prepared.value();
        const binding::BoundMotion motion = Motion(r, model, tame);
        for (const double frame : {0.0, 3.5, 11.0, -2.0, 1e9}) {
            const Pose first = evaluator.Evaluate(motion, frame);
            const Pose second = evaluator.Evaluate(motion, frame);
            ++evaluated;
            if (!Identical(first, second)) {
                std::printf("FAIL case %d frame %g: two evaluations differ\n", i, frame);
                ++failures;
            }
            if (const char* violation = Violation(model, first, tame)) {
                std::printf("FAIL case %d frame %g: %s\n", i, frame, violation);
                ++failures;
            }
            (void)evaluator.World(first);
        }
    }
    std::printf("mmdControl_robustness: %zu evaluations, %d failures\n", evaluated, failures);
    return failures == 0 ? 0 : 1;
}
