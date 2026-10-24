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
#include <memory>
#include <span>
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

/// A non-bone morph scalar (MOTION_CONTRACT.md §11.3). Pose::channels
/// carries raw sampled weights for the shared motion core;
/// ResolveMorphChannels returns accumulated leaf weights using this type.
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
    /// Immutable, copyable snapshot of one evaluation at the physics barrier.
    /// It owns its storage and survives evaluator moves. Only the evaluator
    /// that created it (or a copy of that evaluator) may resume it.
    class Continuation {
    public:
        Continuation() = default;
        const Pose& BeforePhysics() const noexcept { return _pose; }

    private:
        friend class Evaluator;
        struct Data;
        std::shared_ptr<const Data> _data;
        Pose _pose;
    };

    /// Sparse joint-local feedback, canonical basis/metres, rest included.
    /// Each joint may occur once; rotations must be finite unit quaternions.
    struct PhysicsFeedback {
        std::int32_t joint = kNone;
        JointTransform local;
    };

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

    /// Resolve raw Pose::channels through nested groups, summing direct and
    /// group contributions without clamping. Returns reached non-bone,
    /// non-group morphs in ascending canonical index, including zero weights.
    /// Bone effects are already in the pose. Other leaf types (including
    /// flip/impulse) remain scalar outputs, not evaluated effects; consumers
    /// must reject types they cannot realize. Do not combine these weights
    /// with the raw input channels. Invalid indices and bone inputs are ignored.
    std::vector<MorphChannel> ResolveMorphChannels(std::span<const MorphChannel> channels) const;

    /// Sample once and run only pre-physics controls (MOTION §11.10).
    Continuation BeginPhysics(const binding::BoundMotion& motion, double frame) const;

    /// Apply sparse local feedback, then run only post-physics controls.
    /// Does not mutate the snapshot: repeated resumes do not accumulate.
    /// No world stepping or pose-space conversion occurs here. Invalid or
    /// foreign snapshots and malformed feedback return fatal diagnostics.
    Result<Pose> ResumePhysics(const Continuation& continuation,
                               const std::vector<PhysicsFeedback>& feedback = {}) const;

    /// The model's appends and IK over a pose another solver produced, such as
    /// a retarget onto the model (MOTION_CONTRACT.md §11.9). `given` holds one
    /// local transform per joint, rest translation included; a joint it does
    /// not reach is at rest. A joint `held` names keeps its given transform
    /// exactly and takes no append and no IK. A chain is solved only when no
    /// link is held and no link feeds a held joint's append, directly or
    /// through other appends; otherwise it is off. No morphs, no channels,
    /// visible. Deterministic and stateless, as Evaluate is.
    Pose Complete(const Pose& given, const std::vector<bool>& held) const;

    /// The same, refilling `pose`.
    void Complete(const Pose& given, const std::vector<bool>& held, Pose& pose) const;

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
        std::vector<BoneOffset> boneOffsets; ///< a bone morph's
        std::vector<GroupMember> members;    ///< a group morph's
    };
    struct State;

    Evaluator() = default;

    void _Solve(const Chain& chain, State& state) const;
    void _Sample(const binding::BoundMotion& motion, double frame, State& state, Pose& pose,
                 std::vector<bool>& enabled) const;
    void _WritePose(const State& state, Pose& pose) const;
    void _ExpandGroups(std::vector<double>& weights, std::vector<bool>* reached = nullptr) const;
    /// Appends then IK, in MMD's order (§11.5); `held` joints take neither.
    void _Pass(State& state, const std::vector<bool>& enabled, const std::vector<bool>* held,
               std::size_t begin, std::size_t end) const;

    std::shared_ptr<const int> _identity = std::make_shared<const int>(0);
    std::size_t _physicsBoundary = 0;
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
