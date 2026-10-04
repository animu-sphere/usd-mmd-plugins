# SPDX-License-Identifier: Apache-2.0

include("${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/UsdMmdTesting.cmake")

usdmmd_test_executable(mmdSkeletonAdapter_tests test_adapter.cpp)
target_link_libraries(mmdSkeletonAdapter_tests PRIVATE
    mmdSkeletonAdapter::mmdSkeletonAdapter)
add_test(NAME mmdSkeletonAdapter_unit COMMAND mmdSkeletonAdapter_tests)

# Phase 8's generic clips onto a PMX read a semantic motion stage, so this
# executable links the stage-level motionUsd itself, as an installed package.
# It is not in the adapter's link interface, which the boundary test below
# holds to, and nothing that evaluates a VMD is linked.
if(NOT TARGET motionUsd::motionUsd)
    find_package(motionUsd 0.5.2 CONFIG REQUIRED)
endif()

usdmmd_test_executable(mmdSkeletonAdapter_generic_clips test_generic_clips.cpp)
target_link_libraries(mmdSkeletonAdapter_generic_clips PRIVATE
    mmdSkeletonAdapter::mmdSkeletonAdapter
    motionUsd::motionUsd)
add_test(NAME mmdSkeletonAdapter_generic_clips COMMAND mmdSkeletonAdapter_generic_clips)
usdmmd_openusd_test_environment(mmdSkeletonAdapter_unit mmdSkeletonAdapter_generic_clips)

# WORKSPACE.md §2.1 allows mmdSkeletonAdapter mmdModel, motionRetarget and
# motionSource, and through them the OpenUSD foundation closure only.
usdmmd_add_boundary_test(mmdSkeletonAdapter
    TARGET mmdSkeletonAdapter BINARY mmdSkeletonAdapter_tests
    ARGS --allow mmdModel::mmdModel
         --allow motionRetarget::motionRetarget
         --allow motionSource::motionSource
         --allow-openusd-foundation)
