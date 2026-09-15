// SPDX-License-Identifier: Apache-2.0
//
// The canonical joint order (docs/design/STAGE_CONTRACT.md §9.1).
#pragma once

#include <mmdPmx/DiagnosticList.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace mmd::detail {

struct JointOrder {
    /// Each source bone's parent after repair, by source index: another
    /// bone's index or -1, never itself, and never on a cycle.
    std::vector<std::int32_t> parents;
    /// Canonical position -> source index. Every parent precedes its children.
    std::vector<std::size_t> order;
};

/// Repairs the hierarchy and orders it. A parent out of range, or a bone that
/// is its own parent, makes that bone a root (MMD_SKEL_INVALID_PARENT); a
/// cycle is broken by making its lowest-source-index bone a root
/// (MMD_SKEL_PARENT_CYCLE). The order is then the stable topological order
/// that repeatedly emits the lowest-source-index bone whose parent has been
/// emitted: the source order whenever every parent already precedes its
/// children, and MMD_SKEL_JOINTS_REORDERED when not.
JointOrder OrderJoints(const std::vector<std::int32_t>& sourceParents, DiagnosticList& diagnostics);

} // namespace mmd::detail
