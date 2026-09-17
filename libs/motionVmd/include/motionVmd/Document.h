// SPDX-License-Identifier: Apache-2.0
//
// What a VMD file says, record by record, in file order
// (docs/design/MOTION_CONTRACT.md §3).
//
// Source facts only. Values are in the file's own basis and units -- MMD's
// left-handed, +Y-up space, in MMD units -- and nothing here converts them:
// this library knows no model and no stage, and the conversion is applied
// where a motion meets a model (MOTION_CONTRACT.md §7, MOT-O1). Records are
// not sorted, merged or de-duplicated either; that is BuildMotion's
// (motionVmd/Motion.h).
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace motionVmd {

using Float3 = std::array<float, 3>;
using Float4 = std::array<float, 4>;

/// The two signatures MMD has written (MOTION_CONTRACT.md §3).
enum class Version : std::uint8_t {
    V1, ///< "Vocaloid Motion Data file", with a 10-byte model name
    V2, ///< "Vocaloid Motion Data 0002", with a 20-byte model name
};

std::string_view ToString(Version version);

/// A fixed-length CP932 name field (MOTION_CONTRACT.md §4).
struct Name {
    /// The field's bytes before its first NUL: what MMD compares when it
    /// binds a name to a model (§8.1). Whatever follows the NUL is padding,
    /// and some writers fill it with leftover memory, so it is not kept.
    std::string bytes;
    /// `bytes` decoded to UTF-8. A trailing lead byte whose second byte the
    /// field cut off is dropped (MMD_TEXT_TRUNCATED_CP932); bytes CP932 does
    /// not map leave it empty (MMD_TEXT_INVALID_CP932).
    std::string text;

    bool operator==(const Name&) const = default;
};

struct Header {
    Version version = Version::V2;
    Name modelName;

    bool operator==(const Header&) const = default;
};

struct BoneKeyframe {
    Name bone; ///< 15-byte field
    std::uint32_t frame = 0;
    /// Relative to the bone's rest position, in MMD units.
    Float3 translation{};
    /// (x, y, z, w), relative to the bone's world-aligned rest orientation;
    /// kept as stored, normalized or not.
    Float4 rotation{0.0f, 0.0f, 0.0f, 1.0f};
    /// The 64 bytes as stored. BuildMotion reads the four curves from the
    /// first 16 (MOTION_CONTRACT.md §6).
    std::array<std::uint8_t, 64> interpolation{};

    bool operator==(const BoneKeyframe&) const = default;
};

struct MorphKeyframe {
    Name morph; ///< 15-byte field
    std::uint32_t frame = 0;
    float weight = 0.0f;

    bool operator==(const MorphKeyframe&) const = default;
};

struct CameraKeyframe {
    std::uint32_t frame = 0;
    float distance = 0.0f;
    Float3 position{}; ///< the point the camera looks at
    Float3 rotation{}; ///< Euler angles, radians
    std::array<std::uint8_t, 24> interpolation{};
    std::uint32_t viewAngle = 0; ///< degrees
    /// 0 when perspective is on; MMD writes 1 when it is off.
    std::uint8_t orthographic = 0;

    bool operator==(const CameraKeyframe&) const = default;
};

struct LightKeyframe {
    std::uint32_t frame = 0;
    Float3 color{};
    Float3 direction{};

    bool operator==(const LightKeyframe&) const = default;
};

struct SelfShadowKeyframe {
    std::uint32_t frame = 0;
    std::uint8_t mode = 0;
    float distance = 0.0f;

    bool operator==(const SelfShadowKeyframe&) const = default;
};

struct IkState {
    Name bone; ///< 20-byte field
    std::uint8_t enabled = 0;

    bool operator==(const IkState&) const = default;
};

/// One record of the IK-enable and visibility section.
struct IkKeyframe {
    std::uint32_t frame = 0;
    std::uint8_t visible = 0;
    std::vector<IkState> ik;

    bool operator==(const IkKeyframe&) const = default;
};

/// The six keyframe sections, in file order.
enum class Section : std::uint8_t { Bone, Morph, Camera, Light, SelfShadow, Ik };
inline constexpr std::size_t kSectionCount = 6;

/// "boneKeyframes", "morphKeyframes", ... -- the names diagnostics use.
std::string_view ToString(Section section);

struct Document {
    Header header;
    std::vector<BoneKeyframe> boneKeyframes;
    std::vector<MorphKeyframe> morphKeyframes;
    std::vector<CameraKeyframe> cameraKeyframes;
    std::vector<LightKeyframe> lightKeyframes;
    std::vector<SelfShadowKeyframe> selfShadowKeyframes;
    std::vector<IkKeyframe> ikKeyframes;
    /// How many sections the file holds, 0-6: older files end after any
    /// section, and a section that is absent reads as empty. This keeps the
    /// difference, which is a fact about the writer.
    std::size_t sectionsPresent = 0;

    bool operator==(const Document&) const = default;
};

} // namespace motionVmd
