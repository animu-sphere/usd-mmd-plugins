// SPDX-License-Identifier: Apache-2.0
//
// The single source-to-USD conversion (docs/design/STAGE_CONTRACT.md §6). PMX
// is left-handed, +Y up, facing -Z, in MMD units; the stage is right-handed,
// +Y up, facing +Z, in meters. Mirroring Z converts one to the other, and
// every function here is that mirror applied to one kind of quantity.
// Canonicalization calls these once; nothing downstream flips an axis or
// scales a value.
#pragma once

#include "mmdModel/CanonicalDocument.h"

#include <cstdint>

namespace mmd::basis {

/// Meters per MMD unit: the de facto convention that one unit is about 8 cm
/// (STAGE_CONTRACT.md §6.2). Part of the stage contract; changing it bumps
/// the contract version.
inline constexpr double kMetersPerUnit = 0.08;

/// A position: (x, y, z) -> s * (x, y, -z).
Float3 Point(const Float3& source);

/// A displacement -- a morph offset, a translation: the same as a point.
Float3 Displacement(const Float3& source);

/// A point, or a displacement, kept in double: for a value the stage authors
/// as a double (a joint's translation), so it is rounded once, not twice.
Double3 PointD(const Double3& source);

/// A direction or normal: (x, y, z) -> (x, y, -z), renormalized. A zero or
/// non-finite vector is returned mirrored but not normalized.
Float3 Direction(const Float3& source);

/// A unit quaternion (x, y, z, w) -> (-x, -y, z, w).
Float4 Quaternion(const Float4& source);

/// A PMX UV (origin top-left) -> USD `st` (origin bottom-left): (u, 1 - v).
Float2 St(const Float2& source);

/// A triangle's vertex indices with the winding reversed: (i0, i1, i2) ->
/// (i0, i2, i1). The mirror changes handedness, so a front face would face
/// away otherwise.
std::array<std::int32_t, 3> Triangle(std::int32_t i0, std::int32_t i1, std::int32_t i2);

} // namespace mmd::basis
