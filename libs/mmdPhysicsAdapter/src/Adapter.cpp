// SPDX-License-Identifier: Apache-2.0
#include "mmdPhysicsAdapter/Adapter.h"
#include "mmdPhysicsAdapter/Codes.h"

#include <mmdPmx/DiagnosticList.h>
#include <pxr/base/gf/rotation.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/xformCache.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdPhysics/rigidBodyAPI.h>
#include <pxr/usd/usdSkel/topology.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace mmd::physics {
namespace {
using namespace pxr;
using Transform = control::JointTransform;
using Feedback = control::Evaluator::PhysicsFeedback;

template <class T>
Result<T>
Fail(const Code& code, const std::string& message)
{
    return Result<T>::Failure(MakeDiagnostic(code, message));
}

bool
Valid(const Transform& t)
{
    double length = 0;
    for (double v : t.translation)
        if (!std::isfinite(v))
            return false;
    for (double v : t.rotation) {
        if (!std::isfinite(v))
            return false;
        length += v * v;
    }
    return std::abs(length - 1) <= 1e-6;
}

GfMatrix4d
Matrix(const Transform& t)
{
    GfMatrix4d m(1.0);
    m.SetRotate(GfQuatd(t.rotation[3], t.rotation[0], t.rotation[1], t.rotation[2]));
    m.SetTranslateOnly(GfVec3d(t.translation[0], t.translation[1], t.translation[2]));
    return m;
}

Transform
TransformOf(const GfMatrix4d& m)
{
    const auto q = m.ExtractRotationQuat().GetNormalized();
    const auto v = q.GetImaginary();
    const auto t = m.ExtractTranslation();
    return {{t[0], t[1], t[2]}, {v[0], v[1], v[2], q.GetReal()}};
}

bool
Rigid(const GfMatrix4d& m)
{
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            if (!std::isfinite(m[i][j]))
                return false;
    for (int i = 0; i < 3; ++i) {
        if (std::abs(m[i][3]) > 1e-8)
            return false;
        for (int j = 0; j < 3; ++j) {
            double dot = 0;
            for (int k = 0; k < 3; ++k)
                dot += m[i][k] * m[j][k];
            if (std::abs(dot - (i == j ? 1 : 0)) > 1e-6)
                return false;
        }
    }
    return std::abs(m[3][3] - 1) <= 1e-8 && std::abs(m.GetDeterminant() - 1) <= 1e-6;
}

GfMatrix4d
Metres(GfMatrix4d m, double unit)
{
    m.SetTranslateOnly(m.ExtractTranslation() * unit);
    return m;
}

bool
Constant(const UsdAttribute& a, const SdfValueTypeName& type)
{
    return a && a.GetTypeName() == type && a.GetNumTimeSamples() == 0 &&
           a.GetVariability() == SdfVariabilityUniform;
}

bool
StaticPlacement(UsdPrim prim)
{
    for (; prim && !prim.IsPseudoRoot(); prim = prim.GetParent()) {
        const UsdGeomXformable xf(prim);
        if (xf) {
            bool reset = false;
            const auto order = prim.GetAttribute(TfToken("xformOpOrder"));
            if (order && order.GetNumTimeSamples() != 0)
                return false;
            const auto ops = xf.GetOrderedXformOps(&reset);
            if (order && order.HasAuthoredValueOpinion()) {
                VtTokenArray names;
                if (!order.Get(&names))
                    return false;
                const auto count =
                    std::count_if(names.begin(), names.end(), [](const TfToken& name) {
                        return name != TfToken("!resetXformStack!");
                    });
                if (static_cast<std::size_t>(count) != ops.size())
                    return false;
            }
            for (const auto& op : ops) {
                VtValue value;
                if (op.GetAttr().GetNumTimeSamples() != 0 || !op.GetAttr().Get(&value))
                    return false;
            }
            if (reset)
                break;
        }
    }
    return true;
}

bool
PoseWorld(const control::Pose& pose, const std::vector<int>& parents, const GfMatrix4d& placement,
          std::vector<GfMatrix4d>& world)
{
    if (pose.joints.size() != parents.size())
        return false;
    for (std::size_t i = 0; i < parents.size(); ++i) {
        if (!Valid(pose.joints[i]))
            return false;
        const int p = parents[i];
        const auto m = Matrix(pose.joints[i]) * (p < 0 ? placement : world[p]);
        if (!Rigid(m))
            return false;
        world.push_back(m);
    }
    return true;
}
} // namespace

Result<Adapter>
Adapter::Bind(const pxr::UsdSkelSkeleton& skeleton, std::span<const pxr::UsdPrim> bodies)
{
    using namespace pxr;
    if (!skeleton)
        return Fail<Adapter>(codes::PhysicsBindingInvalid, "A valid USD skeleton is required");
    const auto stage = skeleton.GetPrim().GetStage();
    const double unit = UsdGeomGetStageMetersPerUnit(stage);
    if (!std::isfinite(unit) || unit <= 0 || UsdGeomGetStageUpAxis(stage) != UsdGeomTokens->y)
        return Fail<Adapter>(codes::PhysicsBindingInvalid,
                             "Coupling requires Y-up and positive metresPerUnit");
    VtTokenArray joints;
    VtMatrix4dArray rest;
    const auto ja = skeleton.GetJointsAttr();
    const auto ra = skeleton.GetRestTransformsAttr();
    if (!Constant(ja, SdfValueTypeNames->TokenArray) || !ja.Get(&joints) ||
        !Constant(ra, SdfValueTypeNames->Matrix4dArray) || !ra.Get(&rest) ||
        rest.size() != joints.size())
        return Fail<Adapter>(codes::PhysicsBindingInvalid,
                             "Skeleton requires constant joints and matching restTransforms");
    const UsdSkelTopology topology(joints);
    std::string reason;
    if (!topology.Validate(&reason) ||
        std::set<TfToken>(joints.begin(), joints.end()).size() != joints.size())
        return Fail<Adapter>(codes::PhysicsBindingInvalid, "Invalid skeleton topology: " + reason);
    Adapter result;
    result._parents.assign(topology.GetParentIndices().begin(), topology.GetParentIndices().end());
    UsdGeomXformCache cache;
    const auto placement = cache.GetLocalToWorldTransform(skeleton.GetPrim());
    if (!StaticPlacement(skeleton.GetPrim()) || !Rigid(placement))
        return Fail<Adapter>(codes::PhysicsTransformUnsupported,
                             "Skeleton placement must be static and rigid (unit scale)");
    result._placement = Metres(placement, unit);
    std::vector<GfMatrix4d> world;
    for (std::size_t i = 0; i < rest.size(); ++i) {
        if (!Rigid(rest[i]))
            return Fail<Adapter>(codes::PhysicsTransformUnsupported,
                                 "Skeleton rest transforms must be rigid (unit scale)");
        const auto p = result._parents[i];
        const auto m = Metres(rest[i], unit) * (p < 0 ? result._placement : world[p]);
        if (!Rigid(m))
            return Fail<Adapter>(codes::PhysicsTransformUnsupported,
                                 "Skeleton rest transform overflows world space");
        world.push_back(m);
    }
    std::set<SdfPath> paths;
    std::set<int> driven;
    DiagnosticList diagnostics;
    for (const auto& prim : bodies) {
        if (!prim || prim.GetStage() != stage || !paths.insert(prim.GetPath()).second)
            return Fail<Adapter>(codes::PhysicsBindingInvalid,
                                 "Bodies must be unique valid prims on the skeleton stage");
        const std::string path = prim.GetPath().GetString();
        Body body;
        body.path = prim.GetPath();
        TfToken mode;
        const auto boneAttr = prim.GetAttribute(TfToken("mmd:physics:bone"));
        const auto modeAttr = prim.GetAttribute(TfToken("mmd:physics:mode"));
        if (!Constant(boneAttr, SdfValueTypeNames->Int) || !boneAttr.Get(&body.joint) ||
            body.joint < -1 ||
            (body.joint >= 0 && static_cast<std::size_t>(body.joint) >= joints.size()) ||
            !Constant(modeAttr, SdfValueTypeNames->Token) || !modeAttr.Get(&mode))
            return Fail<Adapter>(codes::PhysicsBindingInvalid,
                                 path + ": invalid constant bone/mode declaration");
        body.follow = mode == TfToken("followBone");
        body.keepTranslation = mode == TfToken("dynamicWithBone");
        body.feedback = body.keepTranslation || mode == TfToken("dynamic");
        if (!body.follow && !body.feedback)
            return Fail<Adapter>(codes::PhysicsBindingInvalid, path + ": unknown MMD physics mode");
        bool kinematic = false;
        const UsdPhysicsRigidBodyAPI api(prim);
        const auto ka = api.GetKinematicEnabledAttr();
        bool enabled = false;
        const auto ea = api.GetRigidBodyEnabledAttr();
        if (!prim.HasAPI<UsdPhysicsRigidBodyAPI>() || !ka || ka.GetNumTimeSamples() != 0 ||
            !ka.Get(&kinematic) || kinematic != body.follow || !ea || ea.GetNumTimeSamples() != 0 ||
            !ea.Get(&enabled) || !enabled)
            return Fail<Adapter>(codes::PhysicsBindingInvalid,
                                 path + ": rigid body enablement/kinematic mode mismatch");
        const auto frame = cache.GetLocalToWorldTransform(prim);
        if (!StaticPlacement(prim) || !Rigid(frame))
            return Fail<Adapter>(codes::PhysicsTransformUnsupported,
                                 path + ": body rest frame must be static and rigid (unit scale)");
        if (body.joint < 0) {
            Location location;
            location.table = "rigidBodies";
            diagnostics.Add(codes::PhysicsBoneUnassociated,
                            path + ": -1 association cannot drive or receive a bone pose",
                            location);
        } else {
            if (body.feedback && !driven.insert(body.joint).second)
                return Fail<Adapter>(codes::PhysicsBindingInvalid,
                                     path + ": multiple simulated bodies drive one joint");
            body.offset = Metres(frame, unit) * world[body.joint].GetInverse();
            if (!Rigid(body.offset))
                return Fail<Adapter>(codes::PhysicsTransformUnsupported,
                                     path + ": bone/body offset is not finite and rigid");
        }
        result._bodies.push_back(body);
    }
    return Result<Adapter>::Success(std::move(result), diagnostics.Take());
}

Result<std::vector<BodyTransform>>
Adapter::UpdateKinematicBodies(const control::Pose& before) const
{
    std::vector<pxr::GfMatrix4d> world;
    if (!PoseWorld(before, _parents, _placement, world))
        return Fail<std::vector<BodyTransform>>(
            codes::PhysicsPoseInvalid,
            "Pose must match the skeleton and contain finite rigid transforms");
    std::vector<BodyTransform> result;
    for (const auto& body : _bodies) {
        if (body.follow && body.joint >= 0) {
            const auto m = body.offset * world[body.joint];
            if (!Rigid(m))
                return Fail<std::vector<BodyTransform>>(
                    codes::PhysicsPoseInvalid, "Kinematic world transform is not finite and rigid");
            result.push_back({body.path, TransformOf(m)});
        }
    }
    return Result<std::vector<BodyTransform>>::Success(std::move(result));
}

Result<std::vector<control::Evaluator::PhysicsFeedback>>
Adapter::ApplyPhysicsToPose(std::span<const BodyTransform> simulated,
                            const control::Pose& before) const
{
    std::vector<pxr::GfMatrix4d> world;
    if (!PoseWorld(before, _parents, _placement, world))
        return Fail<std::vector<Feedback>>(
            codes::PhysicsPoseInvalid,
            "Pose must match the skeleton and contain finite rigid transforms");
    std::map<pxr::SdfPath, pxr::GfMatrix4d> snapshot;
    for (const auto& row : simulated) {
        if (!Valid(row.world) ||
            std::none_of(_bodies.begin(),
                         _bodies.end(),
                         [&](const Body& b) { return b.path == row.body; }) ||
            !snapshot.emplace(row.body, Matrix(row.world)).second)
            return Fail<std::vector<Feedback>>(
                codes::PhysicsSnapshotInvalid,
                row.body.GetString() + ": unknown, duplicate or malformed world body transform");
    }
    std::map<int, const Body*> driven;
    for (const auto& body : _bodies)
        if (body.feedback && body.joint >= 0) {
            if (!snapshot.contains(body.path))
                return Fail<std::vector<Feedback>>(codes::PhysicsSnapshotInvalid,
                                                   body.path.GetString() +
                                                       ": missing simulated body transform");
            driven.emplace(body.joint, &body);
        }
    std::vector<Feedback> result;
    for (std::size_t i = 0; i < _parents.size(); ++i) {
        const int p = _parents[i];
        const auto& parent = p < 0 ? _placement : world[p];
        auto local = before.joints[i];
        if (const auto found = driven.find(static_cast<int>(i)); found != driven.end()) {
            const auto& body = *found->second;
            const auto m = body.offset.GetInverse() * snapshot.at(body.path) * parent.GetInverse();
            if (!Rigid(m))
                return Fail<std::vector<Feedback>>(
                    codes::PhysicsSnapshotInvalid,
                    body.path.GetString() + ": feedback transform is not finite and rigid");
            local = TransformOf(m);
            if (body.keepTranslation)
                local.translation = before.joints[i].translation;
            result.push_back({static_cast<int>(i), local});
        }
        // Also rebuild intermediate, non-driven ancestors after parent feedback.
        world[i] = Matrix(local) * parent;
        if (!Rigid(world[i]))
            return Fail<std::vector<Feedback>>(codes::PhysicsSnapshotInvalid,
                                               "Feedback overflows world space");
    }
    return Result<std::vector<Feedback>>::Success(std::move(result));
}
} // namespace mmd::physics
