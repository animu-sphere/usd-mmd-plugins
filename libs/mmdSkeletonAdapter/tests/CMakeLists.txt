# SPDX-License-Identifier: Apache-2.0

include("${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/UsdMmdTesting.cmake")

usdmmd_test_executable(mmdSkeletonAdapter_tests test_adapter.cpp)
target_link_libraries(mmdSkeletonAdapter_tests PRIVATE
    mmdSkeletonAdapter::mmdSkeletonAdapter)
add_test(NAME mmdSkeletonAdapter_unit COMMAND mmdSkeletonAdapter_tests)
usdmmd_openusd_test_environment(mmdSkeletonAdapter_unit)

# WORKSPACE.md §2.1 allows mmdSkeletonAdapter mmdModel and motionRetarget,
# and through it the OpenUSD foundation closure only.
usdmmd_add_boundary_test(mmdSkeletonAdapter
    TARGET mmdSkeletonAdapter BINARY mmdSkeletonAdapter_tests
    ARGS --allow mmdModel::mmdModel
         --allow motionRetarget::motionRetarget
         --allow-openusd-foundation)
