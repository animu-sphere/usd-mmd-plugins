// SPDX-License-Identifier: Apache-2.0
#include "Skeleton.h"

#include "mmdModel/Codes.h"

#include <algorithm>
#include <functional>
#include <queue>
#include <string>

namespace mmd::detail {

namespace {

Location
BoneAt(std::size_t index, const char* field)
{
    Location where;
    where.table = "bones";
    where.index = index;
    where.field = field;
    return where;
}

} // namespace

JointOrder
OrderJoints(const std::vector<std::int32_t>& sourceParents, DiagnosticList& diagnostics)
{
    const std::size_t n = sourceParents.size();
    JointOrder result;
    result.parents = sourceParents;
    std::vector<std::int32_t>& parents = result.parents;

    // A parent that names no other bone. The parser has already turned an
    // out-of-range index into -1, so in practice this is a bone that is its
    // own parent; the range check keeps the function total on its own.
    for (std::size_t i = 0; i < n; ++i) {
        const std::int32_t p = parents[i];
        if (p == -1) {
            continue;
        }
        if (p < 0 || static_cast<std::size_t>(p) >= n || static_cast<std::size_t>(p) == i) {
            diagnostics.Add(codes::SkelInvalidParent,
                            static_cast<std::size_t>(p) == i
                                ? std::string("the bone is its own parent; it is made a root")
                                : "parent " + std::to_string(p) +
                                      " names no bone; the bone is made a root",
                            BoneAt(i, "parent"));
            parents[i] = -1;
        }
    }

    // Cycles. Each walk follows parents from an unvisited bone until it reaches
    // a root or a bone already settled; reaching a bone on the walk itself
    // closes a cycle, which is broken at its lowest source index. The cycle a
    // walk finds does not depend on where it entered, so the result is unique.
    enum : std::uint8_t { kUnvisited, kOnWalk, kSettled };
    std::vector<std::uint8_t> state(n, kUnvisited);
    std::vector<std::size_t> walk;
    for (std::size_t start = 0; start < n; ++start) {
        walk.clear();
        std::int32_t current = static_cast<std::int32_t>(start);
        while (current != -1 && state[static_cast<std::size_t>(current)] == kUnvisited) {
            state[static_cast<std::size_t>(current)] = kOnWalk;
            walk.push_back(static_cast<std::size_t>(current));
            current = parents[static_cast<std::size_t>(current)];
        }
        if (current != -1 && state[static_cast<std::size_t>(current)] == kOnWalk) {
            const auto entry =
                std::find(walk.begin(), walk.end(), static_cast<std::size_t>(current));
            const std::size_t lowest = *std::min_element(entry, walk.end());
            diagnostics.Add(
                codes::SkelParentCycle,
                "a cycle of " + std::to_string(walk.end() - entry) +
                    " bones through their parents; this one, the lowest-indexed, is made a root",
                BoneAt(lowest, "parent"));
            parents[lowest] = -1;
        }
        for (std::size_t bone : walk) {
            state[bone] = kSettled;
        }
    }

    // The stable topological order: always the lowest-indexed bone whose
    // parent has been emitted.
    std::vector<std::vector<std::size_t>> children(n);
    std::priority_queue<std::size_t, std::vector<std::size_t>, std::greater<>> ready;
    for (std::size_t i = 0; i < n; ++i) {
        if (parents[i] == -1) {
            ready.push(i);
        } else {
            children[static_cast<std::size_t>(parents[i])].push_back(i);
        }
    }
    result.order.reserve(n);
    while (!ready.empty()) {
        const std::size_t bone = ready.top();
        ready.pop();
        result.order.push_back(bone);
        for (std::size_t child : children[bone]) {
            ready.push(child);
        }
    }

    std::size_t moved = 0;
    for (std::size_t i = 0; i < n; ++i) {
        moved += result.order[i] != i ? 1 : 0;
    }
    if (moved > 0) {
        Location where;
        where.table = "bones";
        diagnostics.Add(codes::SkelJointsReordered,
                        "a bone precedes its parent in the bone table; " + std::to_string(moved) +
                            " of " + std::to_string(n) + " joints are in a different position",
                        std::move(where));
    }
    return result;
}

} // namespace mmd::detail
