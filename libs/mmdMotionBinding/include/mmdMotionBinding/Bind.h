// SPDX-License-Identifier: Apache-2.0
//
// Binding a VMD motion to a PMX model (docs/design/MOTION_CONTRACT.md §8).
//
// A VMD names bones and morphs; it does not index them. Binding is the
// separate, explicit operation that takes a motion source representation and
// a canonical model and returns the motion addressed to that model: each
// track matched to a joint or morph by MMD's own rule, and every value
// converted into the USD basis and meters with the model's conversion
// functions (mmd::basis, MOT-O1).
//
// It is not a bake. Nothing is sampled, no curve is evaluated, and no IK or
// append transform is solved: the keys still drive MMD's control rig -- the
// IK targets, the append sources -- exactly as the file does, and turning
// them into a pose of the deformation skeleton is mmdControl's
// (MOTION_CONTRACT.md §8.2, §11).
#pragma once

#include <mmdModel/CanonicalDocument.h>
#include <mmdPmx/Diagnostic.h>
#include <mmdPmx/Result.h>
#include <motionVmd/Diagnostic.h>
#include <motionVmd/Motion.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mmd::binding {

/// A bone keyframe addressed to a joint. `translation` is a displacement from
/// the joint's rest position, in meters; `rotation` is relative to its
/// world-aligned rest orientation; both in the USD basis
/// (MOTION_CONTRACT.md §7). The curves are the source's: a mirror negates a
/// value, never the progress a curve describes.
struct BoneKey {
    std::uint32_t frame = 0;
    Float3 translation{};
    Float4 rotation{0.0f, 0.0f, 0.0f, 1.0f};
    std::array<motionVmd::Bezier, 4> curves{};

    bool operator==(const BoneKey&) const = default;
};

struct BoneTrack {
    std::int32_t joint = kNone; ///< canonical joint index
    std::vector<BoneKey> keys;  ///< strictly increasing frames

    bool operator==(const BoneTrack&) const = default;
};

struct MorphTrack {
    std::int32_t morph = kNone; ///< canonical morph index
    std::vector<motionVmd::MorphKey> keys;

    bool operator==(const MorphTrack&) const = default;
};

/// IK-enable keys for the joint the IK state names: runtime input to the
/// evaluator that solves that joint's chain (MOTION_CONTRACT.md §8.3).
struct IkTrack {
    std::int32_t joint = kNone;
    std::vector<motionVmd::IkKey> keys;

    bool operator==(const IkTrack&) const = default;
};

struct BoundMotion {
    std::vector<BoneTrack> bones;   ///< ascending joint index
    std::vector<MorphTrack> morphs; ///< ascending morph index
    std::vector<IkTrack> ik;        ///< ascending joint index
    /// Model visibility, bound to the model as a whole.
    std::vector<motionVmd::VisibilityKey> visibility;

    bool operator==(const BoundMotion&) const = default;
};

/// Widths of the VMD name fields a model name is compared in.
inline constexpr std::size_t kBoneNameField = 15;
inline constexpr std::size_t kMorphNameField = 15;
inline constexpr std::size_t kIkNameField = 20;

/// The bytes MMD compares a model name as: its source name encoded to CP932,
/// cut to `width` bytes -- inside a character if that is where the width
/// ends -- and cut again at a NUL. Nothing when the name is not CP932
/// (MOTION_CONTRACT.md §8.1).
std::optional<std::string> FieldBytes(std::string_view sourceName, std::size_t width);

/// Binds `motion` to `model` by source name. Never fails; every track that
/// does not bind is reported (MOTION_CONTRACT.md §8.1):
///
///   * a bone, IK or morph name no model element matches:
///     MMD_MOTION_UNMATCHED_BONE or MMD_MOTION_UNMATCHED_MORPH (info), once
///     per name and section, and the track is dropped;
///   * a name two or more elements match: the lowest source index wins, with
///     MMD_MOTION_AMBIGUOUS_NAME (warning);
///   * model names CP932 cannot encode, which can never match:
///     MMD_MOTION_UNENCODABLE_NAME (info), once per table.
///
/// A model element with an empty source name matches nothing: an empty name
/// is what a name the parser could not decode becomes.
Result<BoundMotion> Bind(const motionVmd::Motion& motion, const CanonicalDocument& model);

/// A motionVmd diagnostic as the workspace's record, field for field: the
/// section is the table.
Diagnostic ToDiagnostic(const motionVmd::Diagnostic& diagnostic);

} // namespace mmd::binding
