// SPDX-License-Identifier: Apache-2.0
//
// The diagnostic codes mmdModel raises -- the events canonicalization, and
// not the parser or the importer, detects (docs/reference/DIAGNOSTICS.md §5).
// Same rules as mmdPmx/Codes.h: a code is never renamed, reused, or given
// another severity, and scripts/check_docs.py fails when this list and
// DIAGNOSTICS.md §5 disagree.
#pragma once

#include <mmdPmx/Diagnostic.h>

namespace mmd::codes {

// Text -- docs/design/TEXT_ENCODING_POLICY.md §3.
inline constexpr Code TextTrailingNul{"MMD_TEXT_TRAILING_NUL", Severity::Info};

// Paths -- docs/design/TEXT_ENCODING_POLICY.md §7.2.
inline constexpr Code PathUnsafeTexturePath{"MMD_PATH_UNSAFE_TEXTURE_PATH", Severity::Warning};

// Skeleton and skinning -- docs/design/STAGE_CONTRACT.md §9.1, §9.5.
inline constexpr Code SkelInvalidParent{"MMD_SKEL_INVALID_PARENT", Severity::Error};
inline constexpr Code SkelParentCycle{"MMD_SKEL_PARENT_CYCLE", Severity::Error};
inline constexpr Code SkelJointsReordered{"MMD_SKEL_JOINTS_REORDERED", Severity::Info};
inline constexpr Code SkelWeightsNormalized{"MMD_SKEL_WEIGHTS_NORMALIZED", Severity::Info};
inline constexpr Code SkelZeroWeights{"MMD_SKEL_ZERO_WEIGHTS", Severity::Warning};

// Materials -- docs/design/PMX_CONTRACT.md §8.
inline constexpr Code MaterialUnsupportedSphereMode{
	"MMD_MATERIAL_UNSUPPORTED_SPHERE_MODE", Severity::Warning};

// The source -> USD boundary -- docs/design/TEXT_ENCODING_POLICY.md §6.1.
inline constexpr Code UsdIdentifierCollision{"MMD_USD_IDENTIFIER_COLLISION", Severity::Info};

}  // namespace mmd::codes
