// SPDX-License-Identifier: Apache-2.0
#include "mmdControl/Evaluator.h"

#include "Algebra.h"
#include "mmdControl/Codes.h"

#include <mmdPmx/DiagnosticList.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <numbers>
#include <queue>
#include <string>
#include <tuple>

namespace mmd::control {

using detail::Vec3;

namespace {

constexpr double kMinLength = 1e-12;
constexpr double kMinAngle = 1e-3 * std::numbers::pi / 180.0;
constexpr double kGimbal = 1e-6;

Vec3
ToVec3(const Float3& v)
{
    return {v[0], v[1], v[2]};
}

Vec3
UnitAxis(std::int32_t axis)
{
    Vec3 v{};
    v[static_cast<std::size_t>(axis)] = 1.0;
    return v;
}

/// `d` wrapped into (-pi, pi].
double
Wrap(double d)
{
    const double r = std::remainder(d, 2.0 * std::numbers::pi);
    return r == -std::numbers::pi ? std::numbers::pi : r;
}

/// R = Rx(x) * Ry(y) * Rz(z).
Quat
FromEuler(const Vec3& e)
{
    return detail::Multiply(detail::Multiply(detail::AxisAngle(UnitAxis(0), e[0]),
                                             detail::AxisAngle(UnitAxis(1), e[1])),
                            detail::AxisAngle(UnitAxis(2), e[2]));
}

/// The Euler angles (x, y, z) of `q` with q = Rx(x) * Ry(y) * Rz(z) that are
/// nearest `previous` (MOTION_CONTRACT.md §11.7).
Vec3
ToEuler(const Quat& q, const Vec3& previous)
{
    const double x = q[0], y = q[1], z = q[2], w = q[3];
    const double m00 = 1.0 - 2.0 * (y * y + z * z);
    const double m01 = 2.0 * (x * y - z * w);
    const double m02 = 2.0 * (x * z + y * w);
    const double m10 = 2.0 * (x * y + z * w);
    const double m11 = 1.0 - 2.0 * (x * x + z * z);
    const double m12 = 2.0 * (y * z - x * w);
    const double m20 = 2.0 * (x * z - y * w);
    const double m21 = 2.0 * (y * z + x * w);
    const double m22 = 1.0 - 2.0 * (x * x + y * y);

    Vec3 e{};
    e[1] = std::asin(std::clamp(m02, -1.0, 1.0));
    if (std::abs(std::cos(e[1])) < kGimbal) {
        // Only x + z or x - z is determined: keep x, and solve Rx(x)^-1 * R =
        // Ry(y) * Rz(z) for z.
        e[0] = previous[0];
        const double cx = std::cos(e[0]);
        const double sx = std::sin(e[0]);
        e[2] = std::atan2(cx * m10 + sx * m20, cx * m11 + sx * m21);
    } else {
        e[0] = std::atan2(-m12, m22);
        e[2] = std::atan2(-m01, m00);
    }

    const auto error = [&](const Vec3& c) {
        return std::abs(Wrap(c[0] - previous[0])) + std::abs(Wrap(c[1] - previous[1])) +
               std::abs(Wrap(c[2] - previous[2]));
    };
    const double pi = std::numbers::pi;
    Vec3 best = e;
    double bestError = error(e);
    for (const double sx : {pi, -pi}) {
        for (const double sy : {pi, -pi}) {
            for (const double sz : {pi, -pi}) {
                const Vec3 candidate{e[0] + sx, sy - e[1], e[2] + sz};
                const double candidateError = error(candidate);
                if (candidateError < bestError) {
                    best = candidate;
                    bestError = candidateError;
                }
            }
        }
    }
    return best;
}

/// `value` into [lower, upper]; `upper` wins when the two cross, as a
/// malformed model's limits may.
double
Clamp(double value, double lower, double upper)
{
    return std::min(std::max(value, lower), upper);
}

} // namespace

/// One evaluation's working values, per canonical joint (MOTION_CONTRACT.md
/// §11.4): the motion translation `a` and rotation `rho`, the IK rotation,
/// and the append.
struct Evaluator::State {
    const std::vector<Joint>* joints = nullptr;
    std::vector<Vec3> motionTranslation;
    std::vector<Quat> motionRotation;
    std::vector<Quat> ik;
    std::vector<Vec3> appendTranslation;
    std::vector<Quat> appendRotation;

    detail::Transform Local(std::int32_t j) const
    {
        const auto i = static_cast<std::size_t>(j);
        return {detail::Add(detail::Add((*joints)[i].rest, motionTranslation[i]),
                            appendTranslation[i]),
                detail::Multiply(detail::Multiply(ik[i], motionRotation[i]), appendRotation[i])};
    }

    detail::Transform World(std::int32_t j) const
    {
        detail::Transform world = Local(j);
        for (std::int32_t p = (*joints)[static_cast<std::size_t>(j)].parent; p != kNone;
             p = (*joints)[static_cast<std::size_t>(p)].parent) {
            world = detail::Compose(Local(p), world);
        }
        return world;
    }
};

Result<Evaluator>
Evaluator::Prepare(const CanonicalDocument& model)
{
    Evaluator evaluator;
    DiagnosticList diagnostics;

    const std::vector<Bone>& bones = model.skeleton.bones;
    const auto count = static_cast<std::int32_t>(bones.size());
    const auto valid = [&](std::int32_t joint) { return joint >= 0 && joint < count; };

    evaluator._joints.resize(bones.size());
    for (std::size_t j = 0; j < bones.size(); ++j) {
        Joint& joint = evaluator._joints[j];
        // Canonical parents come first (STAGE_CONTRACT.md §9.1); anything
        // else would make a world transform loop, and is a root instead.
        const std::int32_t parent = bones[j].parent;
        joint.parent = parent >= 0 && static_cast<std::size_t>(parent) < j ? parent : kNone;
        joint.rest = bones[j].localTranslation;
        if (j >= model.rig.bones.size()) {
            continue;
        }
        const BoneControl& control = model.rig.bones[j];
        Location where;
        where.table = "bones";
        where.index = bones[j].sourceIndex;
        if (control.hasExternalParent) {
            diagnostics.Add(codes::MotionExternalParentIgnored,
                            "bone '" + bones[j].name.source + "' has an external parent (key " +
                                std::to_string(control.externalParentKey) +
                                "); with one model it is ignored",
                            where);
        }
        if (valid(control.appendSource) && (control.appendRotation || control.appendTranslation)) {
            joint.appendSource = control.appendSource;
            joint.appendRatio = control.appendRatio;
            joint.appendRotation = control.appendRotation;
            joint.appendTranslation = control.appendTranslation;
            if (control.appendLocal) {
                diagnostics.Add(codes::MotionLocalAppendApproximated,
                                "bone '" + bones[j].name.source +
                                    "' has a local append; it is evaluated as a global one",
                                where);
            }
        }
    }

    for (const IkChain& source : model.rig.ikChains) {
        if (!valid(source.joint) || !valid(source.effector) ||
            evaluator._joints[static_cast<std::size_t>(source.joint)].chain != kNone) {
            continue;
        }
        Chain chain;
        chain.goal = source.joint;
        chain.effector = source.effector;
        chain.iterations = std::clamp(source.loopCount, 0, kMaxIkIterations);
        chain.limitAngle = source.limitAngle;
        if (chain.iterations != source.loopCount) {
            const Bone& bone = bones[static_cast<std::size_t>(source.joint)];
            Location where;
            where.table = "bones";
            where.index = bone.sourceIndex;
            where.field = "ik.loopCount";
            diagnostics.Add(codes::MotionIkLoopClamped,
                            "IK bone '" + bone.name.source + "' loops " +
                                std::to_string(source.loopCount) + " times; clamped to " +
                                std::to_string(chain.iterations),
                            where);
        }
        for (const IkLink& source_link : source.links) {
            if (!valid(source_link.joint)) {
                continue;
            }
            Link link;
            link.joint = source_link.joint;
            link.hasLimits = source_link.hasLimits;
            link.lower = ToVec3(source_link.lowerLimit);
            link.upper = ToVec3(source_link.upperLimit);
            if (link.hasLimits) {
                int limited = 0;
                for (int axis = 0; axis < 3; ++axis) {
                    if (link.lower[axis] != 0.0 || link.upper[axis] != 0.0) {
                        ++limited;
                        link.planeAxis = axis;
                    }
                }
                if (limited != 1) {
                    link.planeAxis = -1;
                }
            }
            chain.links.push_back(link);
        }
        evaluator._joints[static_cast<std::size_t>(source.joint)].chain =
            static_cast<std::int32_t>(evaluator._chains.size());
        evaluator._chains.push_back(std::move(chain));
    }

    evaluator._order.resize(bones.size());
    for (std::int32_t j = 0; j < count; ++j) {
        evaluator._order[static_cast<std::size_t>(j)] = j;
    }
    const auto key = [&](std::int32_t j) {
        const auto i = static_cast<std::size_t>(j);
        const bool afterPhysics = i < model.rig.bones.size() && model.rig.bones[i].deformAfterPhysics;
        const std::int32_t layer = i < model.rig.bones.size() ? model.rig.bones[i].transformLayer : 0;
        return std::tuple(afterPhysics, layer, bones[i].sourceIndex, j);
    };
    std::sort(evaluator._order.begin(), evaluator._order.end(), [&](std::int32_t a, std::int32_t b) {
        return key(a) < key(b);
    });

    const auto morphCount = static_cast<std::int32_t>(model.morphs.size());
    evaluator._morphs.resize(model.morphs.size());
    std::vector<std::int32_t> listedBy(model.morphs.size(), 0);
    for (std::size_t m = 0; m < model.morphs.size(); ++m) {
        const mmd::Morph& source = model.morphs[m];
        Morph& morph = evaluator._morphs[m];
        morph.type = source.type;
        if (source.type == MorphType::Bone) {
            evaluator._boneMorphs.push_back(static_cast<std::int32_t>(m));
            for (const MorphBoneOffset& offset : source.boneOffsets) {
                if (valid(offset.joint)) {
                    morph.boneOffsets.push_back(BoneOffset{
                        offset.joint,
                        ToVec3(offset.translation),
                        {offset.rotation[0], offset.rotation[1], offset.rotation[2], offset.rotation[3]}});
                }
            }
        } else if (source.type == MorphType::Group) {
            for (const MorphMember& member : source.members) {
                if (member.morph >= 0 && member.morph < morphCount) {
                    morph.members.push_back(GroupMember{member.morph, member.weight});
                    if (model.morphs[static_cast<std::size_t>(member.morph)].type == MorphType::Group) {
                        ++listedBy[static_cast<std::size_t>(member.morph)];
                    }
                }
            }
        }
    }
    // Groups before the groups they list (Kahn's algorithm, lowest index
    // first). Canonicalization leaves no cycle; a group on one would never be
    // reached and is left out.
    std::priority_queue<std::int32_t, std::vector<std::int32_t>, std::greater<>> ready;
    for (std::int32_t m = 0; m < morphCount; ++m) {
        if (evaluator._morphs[static_cast<std::size_t>(m)].type == MorphType::Group &&
            listedBy[static_cast<std::size_t>(m)] == 0) {
            ready.push(m);
        }
    }
    while (!ready.empty()) {
        const std::int32_t g = ready.top();
        ready.pop();
        evaluator._groupOrder.push_back(g);
        for (const GroupMember& member : evaluator._morphs[static_cast<std::size_t>(g)].members) {
            const auto i = static_cast<std::size_t>(member.morph);
            if (evaluator._morphs[i].type == MorphType::Group && --listedBy[i] == 0) {
                ready.push(member.morph);
            }
        }
    }

    return Result<Evaluator>::Success(std::move(evaluator), diagnostics.Take());
}

Pose
Evaluator::Evaluate(const binding::BoundMotion& motion, double frame) const
{
    Pose pose;
    Evaluate(motion, frame, pose);
    return pose;
}

void
Evaluator::Evaluate(const binding::BoundMotion& motion, double frame, Pose& pose) const
{
    const std::size_t count = _joints.size();
    State state;
    state.joints = &_joints;
    state.motionTranslation.assign(count, Vec3{});
    state.motionRotation.assign(count, kIdentity);
    state.ik.assign(count, kIdentity);
    state.appendTranslation.assign(count, Vec3{});
    state.appendRotation.assign(count, kIdentity);

    // Keys (§11.2).
    for (const binding::BoneTrack& track : motion.bones) {
        if (track.joint < 0 || static_cast<std::size_t>(track.joint) >= count) {
            continue;
        }
        const BoneSample sample = SampleBone(track.keys, frame);
        state.motionTranslation[static_cast<std::size_t>(track.joint)] = sample.translation;
        state.motionRotation[static_cast<std::size_t>(track.joint)] = sample.rotation;
    }

    // Morphs (§11.3): effective weights, groups first, then bone morphs.
    pose.channels.clear();
    std::vector<double> weights(_morphs.size(), 0.0);
    for (const binding::MorphTrack& track : motion.morphs) {
        if (track.morph < 0 || static_cast<std::size_t>(track.morph) >= _morphs.size()) {
            continue;
        }
        const double weight = SampleMorph(track.keys, frame);
        weights[static_cast<std::size_t>(track.morph)] += weight;
        if (_morphs[static_cast<std::size_t>(track.morph)].type != MorphType::Bone) {
            pose.channels.push_back(MorphChannel{track.morph, weight});
        }
    }
    for (const std::int32_t g : _groupOrder) {
        const double weight = weights[static_cast<std::size_t>(g)];
        if (weight == 0.0) {
            continue;
        }
        for (const GroupMember& member : _morphs[static_cast<std::size_t>(g)].members) {
            weights[static_cast<std::size_t>(member.morph)] += weight * member.weight;
        }
    }
    for (const std::int32_t m : _boneMorphs) {
        const double weight = weights[static_cast<std::size_t>(m)];
        if (weight == 0.0) {
            continue;
        }
        for (const BoneOffset& offset : _morphs[static_cast<std::size_t>(m)].boneOffsets) {
            const auto j = static_cast<std::size_t>(offset.joint);
            state.motionTranslation[j] =
                detail::Add(state.motionTranslation[j], detail::Scale(offset.translation, weight));
            state.motionRotation[j] = detail::Multiply(detail::ScaleRotation(offset.rotation, weight),
                                                       state.motionRotation[j]);
        }
    }

    // IK enable states (§11.2).
    std::vector<bool> enabled(_chains.size(), true);
    for (const binding::IkTrack& track : motion.ik) {
        if (track.joint < 0 || static_cast<std::size_t>(track.joint) >= count) {
            continue;
        }
        const std::int32_t chain = _joints[static_cast<std::size_t>(track.joint)].chain;
        if (chain != kNone) {
            enabled[static_cast<std::size_t>(chain)] = SampleIk(track.keys, frame);
        }
    }

    // The pass (§11.5): appends (§11.6), then IK (§11.7), in MMD's order.
    for (const std::int32_t j : _order) {
        const Joint& joint = _joints[static_cast<std::size_t>(j)];
        if (joint.appendSource != kNone) {
            const auto i = static_cast<std::size_t>(j);
            const auto s = static_cast<std::size_t>(joint.appendSource);
            if (joint.appendRotation) {
                const Quat whole = detail::Multiply(
                    detail::Multiply(state.ik[s], state.motionRotation[s]), state.appendRotation[s]);
                state.appendRotation[i] = detail::ScaleRotation(whole, joint.appendRatio);
            }
            if (joint.appendTranslation) {
                state.appendTranslation[i] = detail::Scale(
                    detail::Add(state.motionTranslation[s], state.appendTranslation[s]), joint.appendRatio);
            }
        }
        if (joint.chain != kNone && enabled[static_cast<std::size_t>(joint.chain)]) {
            _Solve(_chains[static_cast<std::size_t>(joint.chain)], state);
        }
    }

    pose.joints.resize(count);
    for (std::size_t j = 0; j < count; ++j) {
        const detail::Transform local = state.Local(static_cast<std::int32_t>(j));
        pose.joints[j] = JointTransform{local.translation, local.rotation};
    }
    pose.visible = SampleVisibility(motion.visibility, frame);
}

void
Evaluator::_Solve(const Chain& chain, State& state) const
{
    const std::size_t linkCount = chain.links.size();
    std::vector<double> planeAngle(linkCount, 0.0);
    std::vector<Vec3> previousEuler(linkCount, Vec3{});
    std::vector<Quat> saved(linkCount, kIdentity);
    for (const Link& link : chain.links) {
        state.ik[static_cast<std::size_t>(link.joint)] = kIdentity;
    }

    double best = std::numeric_limits<double>::infinity();
    for (std::int32_t iteration = 0; iteration < chain.iterations; ++iteration) {
        for (std::size_t k = 0; k < linkCount; ++k) {
            const Link& link = chain.links[k];
            if (link.joint == chain.effector) {
                continue;
            }
            const auto i = static_cast<std::size_t>(link.joint);
            const detail::Transform frame = state.World(link.joint);
            Vec3 toEffector = detail::ToFrame(frame, state.World(chain.effector).translation);
            Vec3 toGoal = detail::ToFrame(frame, state.World(chain.goal).translation);
            const double effectorLength = detail::Length(toEffector);
            const double goalLength = detail::Length(toGoal);
            if (!(effectorLength >= kMinLength) || !(goalLength >= kMinLength)) {
                continue;
            }
            toEffector = detail::Scale(toEffector, 1.0 / effectorLength);
            toGoal = detail::Scale(toGoal, 1.0 / goalLength);
            double angle = std::acos(std::clamp(detail::Dot(toEffector, toGoal), -1.0, 1.0));
            if (!(angle >= kMinAngle)) {
                continue;
            }
            angle = std::min(angle, chain.limitAngle);
            const Quat inverseMotion = detail::Conjugate(state.motionRotation[i]);

            if (link.planeAxis >= 0) {
                const auto a = static_cast<std::size_t>(link.planeAxis);
                const Vec3 axis = UnitAxis(link.planeAxis);
                const double forward =
                    detail::Dot(detail::Rotate(detail::AxisAngle(axis, angle), toEffector), toGoal);
                const double backward =
                    detail::Dot(detail::Rotate(detail::AxisAngle(axis, -angle), toEffector), toGoal);
                double theta = planeAngle[k] + (forward > backward ? angle : -angle);
                const double lower = link.lower[a];
                const double upper = link.upper[a];
                if (iteration == 0 && (theta < lower || theta > upper)) {
                    const double middle = 0.5 * (lower + upper);
                    if ((-theta >= lower && -theta <= upper) ||
                        std::abs(middle + theta) < std::abs(middle - theta)) {
                        theta = -theta;
                    }
                }
                theta = Clamp(theta, lower, upper);
                planeAngle[k] = theta;
                state.ik[i] = detail::Multiply(detail::AxisAngle(axis, theta), inverseMotion);
                continue;
            }

            const Vec3 cross = detail::Cross(toEffector, toGoal);
            const double crossLength = detail::Length(cross);
            if (!(crossLength >= kMinLength)) {
                continue;
            }
            Quat rotation = detail::Multiply(
                detail::Multiply(state.ik[i], state.motionRotation[i]),
                detail::AxisAngle(detail::Scale(cross, 1.0 / crossLength), angle));
            if (link.hasLimits) {
                Vec3 euler = ToEuler(detail::Normalize(rotation), previousEuler[k]);
                for (std::size_t axis = 0; axis < 3; ++axis) {
                    const double limited = Clamp(euler[axis], link.lower[axis], link.upper[axis]);
                    euler[axis] = previousEuler[k][axis] +
                                  Clamp(limited - previousEuler[k][axis], -chain.limitAngle, chain.limitAngle);
                }
                previousEuler[k] = euler;
                rotation = FromEuler(euler);
            }
            state.ik[i] = detail::Normalize(detail::Multiply(rotation, inverseMotion));
        }

        const double distance = detail::Length(detail::Sub(state.World(chain.effector).translation,
                                                           state.World(chain.goal).translation));
        if (distance < best) {
            best = distance;
            for (std::size_t k = 0; k < linkCount; ++k) {
                saved[k] = state.ik[static_cast<std::size_t>(chain.links[k].joint)];
            }
        } else {
            for (std::size_t k = 0; k < linkCount; ++k) {
                state.ik[static_cast<std::size_t>(chain.links[k].joint)] = saved[k];
            }
            break;
        }
    }
}

std::vector<JointTransform>
Evaluator::World(const Pose& pose) const
{
    std::vector<JointTransform> world(pose.joints.size());
    for (std::size_t j = 0; j < pose.joints.size() && j < _joints.size(); ++j) {
        const detail::Transform local{pose.joints[j].translation, pose.joints[j].rotation};
        const std::int32_t parent = _joints[j].parent;
        const detail::Transform composed =
            parent == kNone
                ? local
                : detail::Compose({world[static_cast<std::size_t>(parent)].translation,
                                   world[static_cast<std::size_t>(parent)].rotation},
                                  local);
        world[j] = JointTransform{composed.translation, composed.rotation};
    }
    return world;
}

} // namespace mmd::control
