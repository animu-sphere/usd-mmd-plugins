// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <mmdPmx/Result.h>
#include <pxr/usd/usd/prim.h>

namespace mmd::physics {
enum class JointAdmission { Execute, Inert };

/// Classify an enabled standard MMD spring6Dof joint. Both endpoints must
/// resolve to enabled MMD rigid bodies with constant, consistent mode/kinematic
/// declarations. Two followBone endpoints are inert: preserve the joint in
/// USD but omit its solver constraint. World/non-MMD endpoints refuse here.
/// Generic USD frames/limits and spring metadata require their own validation,
/// including for inert joints. No stage edits or backend calls occur.
/// Absent/other well-formed type metadata retains Execute without opting into
/// inert omission; it does not establish execution support for that type.
Result<JointAdmission> ReadJointAdmission(const pxr::UsdPrim& joint);
} // namespace mmd::physics
