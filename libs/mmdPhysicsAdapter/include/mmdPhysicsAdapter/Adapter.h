// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <mmdControl/Evaluator.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdSkel/skeleton.h>
#include <span>

namespace mmd::physics {

/// Caller resolves this composed-stage path to a body in its own world.
/// World transforms are rigid, Y-up, metres; quaternions are x,y,z,w.
struct BodyTransform {
    pxr::SdfPath body;
    control::JointTransform world;
    bool operator==(const BodyTransform&) const = default;
};

/// Owned, stateless coupling binding. No world, body handles, clock or solver
/// is borrowed. Rebind after changing stage declarations or placement.
class Adapter {
public:
    /// Explicit body set in caller order; all must belong to skeleton's stage.
    /// Validates uniform constant MMD declarations, canonical joint topology,
    /// rigid rest/placement frames and mode/kinematic agreement. Transforms
    /// with scale, shear, reflection or animation reject. -1 associations are
    /// retained with a diagnostic and produce no updates/feedback. Multiple
    /// dynamicWithBone bodies driving one joint reject as ambiguous.
    static Result<Adapter> Bind(const pxr::UsdSkelSkeleton& skeleton,
                                std::span<const pxr::UsdPrim> bodies);

    /// Pose must have exactly the bound skeleton's joint order/rest space.
    /// Joint-local transforms are in metres, including rest translations.
    Result<std::vector<BodyTransform>> UpdateKinematicBodies(const control::Pose& before) const;

    /// Convert the immutable world snapshot to sparse joint-local feedback for
    /// Evaluator::ResumePhysics. Dynamic bodies never write bones. For
    /// dynamicWithBone only rotation changes: local translation is preserved.
    /// Parents are resolved before children using the newly computed feedback.
    /// Every associated dynamicWithBone body requires one snapshot row; other
    /// declared bodies may be present. Unknown/duplicate/malformed rows reject.
    Result<std::vector<control::Evaluator::PhysicsFeedback>>
    ApplyPhysicsToPose(std::span<const BodyTransform> simulated, const control::Pose& before) const;

    /// No history exists to clear; binding/rest offsets remain available.
    /// World reset and replay of input are the caller's responsibility.
    void Reset() noexcept {}

private:
    struct Body {
        pxr::SdfPath path;
        int joint = -1;
        bool follow = false;
        bool feedback = false;
        pxr::GfMatrix4d offset{1.0};
    };
    pxr::GfMatrix4d _placement{1.0};
    std::vector<int> _parents;
    std::vector<Body> _bodies;
};

} // namespace mmd::physics
