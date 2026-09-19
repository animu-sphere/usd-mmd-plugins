// SPDX-License-Identifier: Apache-2.0
//
// MMD control evaluation (docs/design/MOTION_CONTRACT.md §11): a motion bound
// to a model, evaluated at an explicit time over the model's control rig --
// bone morphs, appends and IK, in MMD's evaluation order -- into the local
// transform of every deformation joint.
//
// This is the one place in the ecosystem that implements MMD IK and append
// semantics (docs/design/DESIGN_POLICY.md §5.6). A runtime schedules it; it
// owns no clock, no thread and no update loop, and nothing is simulated. No
// OpenUSD type appears here, and nothing of usd-motion-plugins: normalizing
// the result into the shared motion core is mmdMotionAdapter's.
#pragma once

#include "mmdControl/Sample.h"

#include <mmdModel/CanonicalDocument.h>
#include <mmdMotionBinding/Bind.h>
#include <mmdPmx/Result.h>

#include <cstdint>
#include <vector>

namespace mmd::control {

/// A joint's transform: local to its parent in a Pose, in model space from
/// Evaluator::World. Meters, USD basis; the rotation is relative to the
/// joint's identity rest rotation.
struct JointTransform {
    Double3 translation{};
    Quat rotation = kIdentity;

    bool operator==(const JointTransform&) const = default;
};

/// A morph that is not evaluated into the pose, with its sampled weight
/// (MOTION_CONTRACT.md §11.3): a channel for the shared motion core.
struct MorphChannel {
    std::int32_t morph = kNone; ///< canonical morph index
    double weight = 0.0;

    bool operator==(const MorphChannel&) const = default;
};

/// The evaluated state of a model at one time.
struct Pose {
    /// One per canonical joint, in canonical joint order: the rest
    /// translation from the parent plus what the motion adds, and the
    /// rotation after bone morphs, appends and IK.
    std::vector<JointTransform> joints;
    /// Ascending morph index; never a bone morph.
    std::vector<MorphChannel> channels;
    bool visible = true;

    bool operator==(const Pose&) const = default;
};

/// The IK iteration bound (MOTION_CONTRACT.md §11.7): a chain's loop count is
/// clamped into [0, kMaxIkIterations].
inline constexpr std::int32_t kMaxIkIterations = 256;

/// What evaluation needs from one model, prepared once and reused for every
/// time. It does not refer to the model it was prepared from.
class Evaluator {
public:
    /// Prepares `model`. Never fails; what the evaluation cannot honor is
    /// reported here, once per element (MOTION_CONTRACT.md §11.8):
    ///
    ///   * an external parent, ignored: MMD_MOTION_EXTERNAL_PARENT_IGNORED
    ///     (info);
    ///   * a local append, evaluated as a global one:
    ///     MMD_MOTION_LOCAL_APPEND_APPROXIMATED (info);
    ///   * a loop count outside [0, kMaxIkIterations], clamped:
    ///     MMD_MOTION_IK_LOOP_CLAMPED (warning).
    static Result<Evaluator> Prepare(const CanonicalDocument& model);

    /// `motion` at `frame` (MMD frames, 30 per second, fractional allowed).
    /// `motion` must be bound to the prepared model; a track whose index
    /// names nothing here is ignored. Deterministic and stateless: the same
    /// inputs give the same bits, whatever was evaluated before.
    Pose Evaluate(const binding::BoundMotion& motion, double frame) const;

    /// The same, refilling `pose`, whose storage a caller may reuse from
    /// frame to frame.
    void Evaluate(const binding::BoundMotion& motion, double frame, Pose& pose) const;

    /// Each joint's transform in model space, from a pose this evaluator
    /// produced: the parent's world transform composed with the joint's
    /// local one.
    std::vector<JointTransform> World(const Pose& pose) const;

    /// Canonical joint indices in MMD's evaluation order: ascending
    /// (after-physics flag, transform layer, source index)
    /// (MOTION_CONTRACT.md §11.5).
    const std::vector<std::int32_t>& Order() const noexcept { return _order; }

    std::size_t JointCount() const noexcept { return _joints.size(); }

private:
    struct Joint {
        std::int32_t parent = kNone;
        Double3 rest{};
        std::int32_t appendSource = kNone;
        double appendRatio = 0.0;
        bool appendRotation = false;
        bool appendTranslation = false;
        std::int32_t chain = kNone; ///< the chain this joint is the IK bone of
    };
    struct Link {
        std::int32_t joint = kNone;
        bool hasLimits = false;
        Double3 lower{};
        Double3 upper{};
        std::int32_t planeAxis = -1; ///< 0-2 for a plane link, else -1
    };
    struct Chain {
        std::int32_t goal = kNone;
        std::int32_t effector = kNone;
        std::int32_t iterations = 0;
        double limitAngle = 0.0;
        std::vector<Link> links;
    };
    struct BoneOffset {
        std::int32_t joint = kNone;
        Double3 translation{};
        Quat rotation = kIdentity;
    };
    struct GroupMember {
        std::int32_t morph = kNone;
        double weight = 0.0;
    };
    struct Morph {
        MorphType type = MorphType::Group;
        std::vector<BoneOffset> boneOffsets;  ///< a bone morph's
        std::vector<GroupMember> members;     ///< a group morph's
    };
    struct State;

    Evaluator() = default;

    void _Solve(const Chain& chain, State& state) const;

    std::vector<Joint> _joints;
    std::vector<std::int32_t> _order;
    std::vector<Chain> _chains;
    std::vector<Morph> _morphs;
    /// Group morphs, every group before the groups it lists.
    std::vector<std::int32_t> _groupOrder;
    /// Bone morphs, ascending.
    std::vector<std::int32_t> _boneMorphs;
};

} // namespace mmd::control
