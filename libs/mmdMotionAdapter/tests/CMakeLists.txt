# SPDX-License-Identifier: Apache-2.0

include("${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/UsdMmdTesting.cmake")

usdmmd_test_executable(mmdMotionAdapter_tests test_adapter.cpp)
target_link_libraries(mmdMotionAdapter_tests PRIVATE mmdMotionAdapter::mmdMotionAdapter)
add_test(NAME mmdMotionAdapter_unit COMMAND mmdMotionAdapter_tests)

# The Phase 9 acceptance executable is the one place motionRetarget and the
# stage-level motionUsd are linked directly (WORKSPACE.md §2.1): it declares
# both itself, as installed packages. Neither is in the adapter's link
# interface, which the boundary test below holds to.
if(NOT TARGET motionRetarget::motionRetarget)
    find_package(motionRetarget 0.5 CONFIG REQUIRED)
endif()
if(NOT TARGET motionUsd::motionUsd)
    find_package(motionUsd 0.5 CONFIG REQUIRED)
endif()

usdmmd_test_executable(mmdMotionAdapter_acceptance
    test_acceptance.cpp
    generic_retarget.cpp)
target_link_libraries(mmdMotionAdapter_acceptance PRIVATE
    mmdMotionAdapter::mmdMotionAdapter
    motionRetarget::motionRetarget
    motionUsd::motionUsd)
add_test(NAME mmdMotionAdapter_acceptance COMMAND mmdMotionAdapter_acceptance)
usdmmd_openusd_test_environment(mmdMotionAdapter_unit mmdMotionAdapter_acceptance)

# WORKSPACE.md §2.1 allows mmdMotionAdapter mmdControl, mmdModel,
# mmdSkeletonAdapter and motionCore, and through it the OpenUSD foundation
# closure only.
usdmmd_add_boundary_test(mmdMotionAdapter
    TARGET mmdMotionAdapter BINARY mmdMotionAdapter_tests
    ARGS --allow mmdControl::mmdControl
         --allow mmdModel::mmdModel
         --allow mmdSkeletonAdapter::mmdSkeletonAdapter
         --allow motionCore::motionCore
         --allow-openusd-foundation)
