// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <mmdPmx/Diagnostic.h>
namespace mmd::codes {
inline constexpr Code PhysicsMetadataInvalid{"MMD_PHYSICS_METADATA_INVALID", Severity::Fatal};
inline constexpr Code PhysicsDampingUnsupported{"MMD_PHYSICS_DAMPING_UNSUPPORTED", Severity::Fatal};
inline constexpr Code PhysicsBindingInvalid{"MMD_PHYSICS_BINDING_INVALID", Severity::Fatal};
inline constexpr Code PhysicsTransformUnsupported{"MMD_PHYSICS_TRANSFORM_UNSUPPORTED",
                                                  Severity::Fatal};
inline constexpr Code PhysicsBoneUnassociated{"MMD_PHYSICS_BONE_UNASSOCIATED", Severity::Info};
inline constexpr Code PhysicsPoseInvalid{"MMD_PHYSICS_POSE_INVALID", Severity::Fatal};
inline constexpr Code PhysicsSnapshotInvalid{"MMD_PHYSICS_SNAPSHOT_INVALID", Severity::Fatal};
} // namespace mmd::codes
