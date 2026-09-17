// SPDX-License-Identifier: Apache-2.0
//
// The motion source representation: a VMD's records grouped into tracks,
// each sorted by frame, with the interpolation curves decoded
// (docs/design/MOTION_CONTRACT.md §5, §6).
//
// Still source facts, in the source basis and units. What this adds over the
// document is only what every consumer would otherwise redo, and redo the
// same way: one track per name, ordered keys, one key per frame, and the
// Bézier control points read out of their bytes. Nothing is sampled, and
// nothing is bound to a model -- binding is mmdMotionBinding's (§8).
#pragma once

#include "motionVmd/Document.h"
#include "motionVmd/Result.h"

#include <array>
#include <cstdint>
#include <vector>

namespace motionVmd {

/// A cubic Bézier from (0, 0) through (x1, y1) and (x2, y2) to (127, 127):
/// time across, progress up, both on MMD's 0-127 grid. The value MMD stores
/// is kept; dividing by 127 is a consumer's choice.
struct Bezier {
    std::uint8_t x1 = 20;
    std::uint8_t y1 = 20;
    std::uint8_t x2 = 107;
    std::uint8_t y2 = 107;

    bool operator==(const Bezier&) const = default;
};

/// The four curves of a bone keyframe. The 64 bytes are four rows of 16, each
/// row the one before shifted left by a byte; row 0 is, for channel c (X, Y,
/// Z, rotation), x1 = b[c], y1 = b[4 + c], x2 = b[8 + c], y2 = b[12 + c].
/// Later MMD versions overwrite b[2] and b[3] with a physics toggle, so the
/// x1 of Z and of rotation are read from row 1 instead: b[17] and b[18]
/// (MOTION_CONTRACT.md §6).
std::array<Bezier, 4> BoneCurves(const std::array<std::uint8_t, 64>& interpolation);

/// The six curves of a camera keyframe: for channel c (X, Y, Z, rotation,
/// distance, view angle), x1 = b[4c], x2 = b[4c + 1], y1 = b[4c + 2],
/// y2 = b[4c + 3].
std::array<Bezier, 6> CameraCurves(const std::array<std::uint8_t, 24>& interpolation);

enum BoneChannel : std::uint8_t { kX, kY, kZ, kRotation };
enum CameraChannel : std::uint8_t {
    kCameraX,
    kCameraY,
    kCameraZ,
    kCameraRotation,
    kDistance,
    kViewAngle
};

struct BoneKey {
    std::uint32_t frame = 0;
    Float3 translation{};
    Float4 rotation{0.0f, 0.0f, 0.0f, 1.0f};
    std::array<Bezier, 4> curves{}; ///< indexed by BoneChannel

    bool operator==(const BoneKey&) const = default;
};

struct BoneTrack {
    Name bone;
    std::vector<BoneKey> keys; ///< strictly increasing frames

    bool operator==(const BoneTrack&) const = default;
};

struct MorphKey {
    std::uint32_t frame = 0;
    float weight = 0.0f;

    bool operator==(const MorphKey&) const = default;
};

struct MorphTrack {
    Name morph;
    std::vector<MorphKey> keys;

    bool operator==(const MorphTrack&) const = default;
};

struct CameraKey {
    std::uint32_t frame = 0;
    float distance = 0.0f;
    Float3 position{};
    Float3 rotation{};
    std::array<Bezier, 6> curves{}; ///< indexed by CameraChannel
    std::uint32_t viewAngle = 0;
    bool orthographic = false; ///< the stored byte is non-zero

    bool operator==(const CameraKey&) const = default;
};

struct LightKey {
    std::uint32_t frame = 0;
    Float3 color{};
    Float3 direction{};

    bool operator==(const LightKey&) const = default;
};

struct SelfShadowKey {
    std::uint32_t frame = 0;
    std::uint8_t mode = 0;
    float distance = 0.0f;

    bool operator==(const SelfShadowKey&) const = default;
};

struct VisibilityKey {
    std::uint32_t frame = 0;
    bool visible = false;

    bool operator==(const VisibilityKey&) const = default;
};

struct IkKey {
    std::uint32_t frame = 0;
    bool enabled = false;

    bool operator==(const IkKey&) const = default;
};

struct IkTrack {
    Name bone;
    std::vector<IkKey> keys;

    bool operator==(const IkTrack&) const = default;
};

struct Motion {
    Header header;
    /// One track per distinct name *bytes*, in the order each name first
    /// appears in the file. Two fields that decode alike but differ in bytes
    /// are different tracks, as they are to MMD.
    std::vector<BoneTrack> bones;
    std::vector<MorphTrack> morphs;
    std::vector<IkTrack> ik;
    /// The model visibility track, from the IK section's records.
    std::vector<VisibilityKey> visibility;
    /// Scene tracks: recorded, not mapped (MOTION_CONTRACT.md §1).
    std::vector<CameraKey> camera;
    std::vector<LightKey> light;
    std::vector<SelfShadowKey> selfShadow;

    bool operator==(const Motion&) const = default;
};

/// Groups a document's records into tracks. Every track's keys are sorted by
/// frame; where one track holds several records for one frame, the last in
/// file order is kept and MMD_MOTION_DUPLICATE_KEYFRAME (warning) is raised
/// once per track, with how many were dropped. Never fails.
Result<Motion> BuildMotion(const Document& document);

} // namespace motionVmd
