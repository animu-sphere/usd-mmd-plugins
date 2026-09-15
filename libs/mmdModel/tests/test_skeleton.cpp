// SPDX-License-Identifier: Apache-2.0
//
// The canonical joint order against STAGE_CONTRACT.md §9.1.
#include "Skeleton.h"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using mmd::detail::OrderJoints;
using Order = std::vector<std::size_t>;
using Parents = std::vector<std::int32_t>;

std::vector<std::string>
Codes(mmd::DiagnosticList& list)
{
    std::vector<std::string> codes;
    for (const mmd::Diagnostic& d : list.Take()) {
        codes.push_back(d.code);
    }
    return codes;
}

void
TestSourceOrderKept()
{
    mmd::DiagnosticList list;
    const auto order = OrderJoints({-1, 0, 1, 0}, list);
    assert((order.order == Order{0, 1, 2, 3}));
    assert((order.parents == Parents{-1, 0, 1, 0}));
    assert(Codes(list).empty());

    // Two roots, and no bones at all.
    assert((OrderJoints({-1, -1, 0}, list).order == Order{0, 1, 2}));
    assert(OrderJoints({}, list).order.empty());
    assert(Codes(list).empty());
}

void
TestReordered()
{
    mmd::DiagnosticList list;
    // Bone 1's parent is bone 3; bone 2's is bone 1. The lowest-indexed bone
    // whose parent has been emitted goes next: 0, 3, 1, 2.
    const auto order = OrderJoints({-1, 3, 1, 0}, list);
    assert((order.order == Order{0, 3, 1, 2}));
    const auto diagnostics = list.Take();
    assert(diagnostics.size() == 1);
    assert(diagnostics[0].code == "MMD_SKEL_JOINTS_REORDERED");
    assert(diagnostics[0].severity == mmd::Severity::Info);

    // Among ready bones, the lowest index first, not depth first.
    assert((OrderJoints({2, 2, -1, 0}, list).order == Order{2, 0, 1, 3}));
    assert(Codes(list) == std::vector<std::string>{"MMD_SKEL_JOINTS_REORDERED"});
}

void
TestRepairs()
{
    mmd::DiagnosticList list;
    // Bone 1 is its own parent: a root.
    auto order = OrderJoints({-1, 1, 1}, list);
    assert((order.parents == Parents{-1, -1, 1}));
    assert((order.order == Order{0, 1, 2}));
    auto diagnostics = list.Take();
    assert(diagnostics.size() == 1);
    assert(diagnostics[0].code == "MMD_SKEL_INVALID_PARENT");
    assert(diagnostics[0].location.index == 1u);
    assert(diagnostics[0].location.field == "parent");

    // Out of range, which the parser never leaves but the function handles.
    order = OrderJoints({5, -7}, list);
    assert((order.parents == Parents{-1, -1}));
    assert(Codes(list) ==
           (std::vector<std::string>{"MMD_SKEL_INVALID_PARENT", "MMD_SKEL_INVALID_PARENT"}));

    // A cycle 3 -> 1 -> 2 -> 3 is broken at its lowest index, 1, wherever the
    // walk entered it; bone 4 hangs off the cycle.
    order = OrderJoints({-1, 2, 3, 1, 3}, list);
    assert((order.parents == Parents{-1, -1, 3, 1, 3}));
    assert((order.order == Order{0, 1, 3, 2, 4}));
    diagnostics = list.Take();
    assert(diagnostics.size() == 2);
    assert(diagnostics[0].code == "MMD_SKEL_PARENT_CYCLE");
    assert(diagnostics[0].location.index == 1u);
    assert(diagnostics[1].code == "MMD_SKEL_JOINTS_REORDERED");

    // Two separate cycles, each broken once.
    order = OrderJoints({1, 0, 3, 2}, list);
    assert((order.parents == Parents{-1, 0, -1, 2}));
    assert((order.order == Order{0, 1, 2, 3}));
    assert(Codes(list) ==
           (std::vector<std::string>{"MMD_SKEL_PARENT_CYCLE", "MMD_SKEL_PARENT_CYCLE"}));
}

} // namespace

void
TestSkeleton()
{
    TestSourceOrderKept();
    TestReordered();
    TestRepairs();
}
