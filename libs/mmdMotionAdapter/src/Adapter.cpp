// SPDX-License-Identifier: Apache-2.0

#include "mmdMotionAdapter/Adapter.h"

#include "mmdMotionAdapter/Codes.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace mmd::motion {
namespace {

using openstrata::motion::HumanJoint;
using openstrata::motion::HumanJointCount;
using openstrata::motion::MotionClip;
using openstrata::motion::MotionPose;
using openstrata::motion::SourceMetadata;

control::Quat
Multiply(const control::Quat& a, const control::Quat& b)
{
    return {
        a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
        a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
        a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
        a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2],
    };
}

control::Quat
Inverse(const control::Quat& q)
{
    const double norm = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
    if (!(norm > 0.0) || !std::isfinite(norm)) {
        return control::kIdentity;
    }
    return {-q[0] / norm, -q[1] / norm, -q[2] / norm, q[3] / norm};
}

bool
IsUsableRotation(const control::Quat& q)
{
    const double norm = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
    return norm > 0.0 && std::isfinite(norm);
}

pxr::GfQuatf
ToGf(const control::Quat& source)
{
    const double norm = std::sqrt(source[0] * source[0] + source[1] * source[1] +
                                  source[2] * source[2] + source[3] * source[3]);
    const float x = static_cast<float>(source[0] / norm);
    const float y = static_cast<float>(source[1] / norm);
    const float z = static_cast<float>(source[2] / norm);
    const float w = static_cast<float>(source[3] / norm);
    return pxr::GfQuatf(w, pxr::GfVec3f(x, y, z));
}

SourceMetadata
Metadata(const binding::BoundMotion& bound)
{
    SourceMetadata source;
    source.kind = openstrata::motion::MotionSourceKind::Clip;
    source.provider = "usd-mmd-plugins";
    source.protocol = "vmd";
    source.sourceId = bound.sourceModelName;
    return source;
}

std::vector<double>
SampleTimes(const ClipOptions& options)
{
    std::vector<double> times;
    if (options.startTime == options.endTime) {
        times.push_back(options.startTime);
        return times;
    }

    const double step = 1.0 / options.samplesPerSecond;
    const double duration = options.endTime - options.startTime;
    const std::size_t regular = static_cast<std::size_t>(std::floor(duration / step));
    times.reserve(regular + 2);
    for (std::size_t i = 0; i <= regular; ++i) {
        const double time = options.startTime + static_cast<double>(i) * step;
        if (time <= options.endTime) {
            times.push_back(time);
        }
    }
    const double tolerance =
        std::numeric_limits<double>::epsilon() * std::max(1.0, std::abs(options.endTime)) * 8.0;
    if (times.empty() || options.endTime - times.back() > tolerance) {
        times.push_back(options.endTime);
    } else {
        times.back() = options.endTime;
    }
    return times;
}

} // namespace

Result<openstrata::motion::MotionClip>
BuildClip(const CanonicalDocument& model, const binding::BoundMotion& bound,
          const control::Evaluator& evaluator, const skeleton::AdaptedSkeleton& skeleton,
          ClipOptions options)
{
    if (!std::isfinite(options.startTime) || !std::isfinite(options.endTime) ||
        !std::isfinite(options.samplesPerSecond) || options.samplesPerSecond <= 0.0 ||
        options.endTime < options.startTime) {
        return Result<MotionClip>::Failure(MakeDiagnostic(
            codes::MotionInvalidSampleRange,
            "clip range must have finite ordered endpoints and a finite positive sample rate"));
    }

    const double sampleEstimate = (options.endTime - options.startTime) * options.samplesPerSecond;
    if (!std::isfinite(sampleEstimate) ||
        sampleEstimate > static_cast<double>(std::numeric_limits<std::size_t>::max() - 2)) {
        return Result<MotionClip>::Failure(MakeDiagnostic(codes::MotionInvalidSampleRange,
                                                          "clip range contains too many samples"));
    }

    std::vector<Diagnostic> diagnostics;
    for (const HumanJoint required : skeleton.requiredJoints) {
        if (skeleton.SourceJoint(required) != openstrata::motion::RetargetMap::kUnmapped) {
            continue;
        }
        Location where;
        where.table = "bones";
        where.field = std::string(openstrata::motion::HumanJointName(required));
        diagnostics.push_back(
            MakeDiagnostic(codes::MotionMissingRequiredJoint,
                           "MMD source skeleton has no role for '" + where.field + "'",
                           std::move(where)));
    }

    MotionClip clip;
    clip.startTime = options.startTime;
    clip.endTime = options.endTime;
    clip.nominalFrameRate = options.samplesPerSecond;
    clip.source = Metadata(bound);

    const std::vector<double> times = SampleTimes(options);
    clip.samples.reserve(times.size());
    for (std::size_t sampleIndex = 0; sampleIndex < times.size(); ++sampleIndex) {
        const double time = times[sampleIndex];
        const control::Pose evaluated = evaluator.Evaluate(bound, time * 30.0);
        const std::vector<control::JointTransform> world = evaluator.World(evaluated);

        const auto nonFinite = [&](std::string field) {
            Location where;
            where.table = "samples";
            where.index = sampleIndex;
            where.field = std::move(field);
            return Result<MotionClip>::Failure(
                MakeDiagnostic(codes::MotionNonFiniteSample,
                               "evaluated motion cannot be represented as a finite shared sample",
                               std::move(where)),
                diagnostics);
        };

        MotionPose pose;
        pose.timestamp = time;
        pose.metadata = clip.source;

        for (std::size_t i = 0; i < HumanJointCount; ++i) {
            if (!skeleton.sourcePresent.test(i)) {
                continue;
            }
            const HumanJoint role = static_cast<HumanJoint>(i);
            const int joint = skeleton.SourceJoint(role);
            if (joint < 0 || static_cast<std::size_t>(joint) >= world.size()) {
                continue;
            }
            control::Quat local = world[static_cast<std::size_t>(joint)].rotation;
            if (!IsUsableRotation(local)) {
                return nonFinite(std::string(openstrata::motion::HumanJointName(role)) +
                                 ".rotation");
            }
            const auto parent =
                openstrata::motion::NearestPresentAncestor(role, skeleton.sourcePresent);
            if (parent) {
                const int parentJoint = skeleton.SourceJoint(*parent);
                if (parentJoint >= 0 && static_cast<std::size_t>(parentJoint) < world.size()) {
                    const control::Quat& parentRotation =
                        world[static_cast<std::size_t>(parentJoint)].rotation;
                    if (!IsUsableRotation(parentRotation)) {
                        return nonFinite(std::string(openstrata::motion::HumanJointName(*parent)) +
                                         ".rotation");
                    }
                    local = Multiply(Inverse(parentRotation), local);
                }
            }
            if (!IsUsableRotation(local)) {
                return nonFinite(std::string(openstrata::motion::HumanJointName(role)) +
                                 ".rotation");
            }
            pose.localRotations[i] = ToGf(local);
            pose.validRotations.set(i);
        }

        const int hips = skeleton.SourceJoint(HumanJoint::Hips);
        if (hips >= 0 && static_cast<std::size_t>(hips) < world.size()) {
            const control::JointTransform& root = world[static_cast<std::size_t>(hips)];
            const pxr::GfVec3f position(static_cast<float>(root.translation[0]),
                                        static_cast<float>(root.translation[1]),
                                        static_cast<float>(root.translation[2]));
            if (!std::isfinite(position[0]) || !std::isfinite(position[1]) ||
                !std::isfinite(position[2])) {
                return nonFinite("root.position");
            }
            if (!IsUsableRotation(root.rotation)) {
                return nonFinite("root.orientation");
            }
            pose.root.worldPosition = position;
            pose.root.worldOrientation = ToGf(root.rotation);
            pose.root.hasPosition = true;
            pose.root.hasOrientation = true;
        }

        for (const control::MorphChannel& channel : evaluated.channels) {
            if (channel.morph < 0 ||
                static_cast<std::size_t>(channel.morph) >= model.morphs.size()) {
                continue;
            }
            const float weight = static_cast<float>(channel.weight);
            if (!std::isfinite(weight)) {
                return nonFinite("channels");
            }
            pose.channels.Set("mmd:morph:" +
                                  model.morphs[static_cast<std::size_t>(channel.morph)].name.source,
                              weight);
        }
        pose.channels.Set("mmd:model:visibility", evaluated.visible ? 1.0f : 0.0f);
        clip.samples.push_back(std::move(pose));
    }

    return Result<MotionClip>::Success(std::move(clip), std::move(diagnostics));
}

} // namespace mmd::motion
