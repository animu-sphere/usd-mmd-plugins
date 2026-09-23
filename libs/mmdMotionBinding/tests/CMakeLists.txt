# SPDX-License-Identifier: Apache-2.0

include("${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/UsdMmdTesting.cmake")

usdmmd_test_executable(mmdMotionBinding_tests test_bind.cpp)
target_link_libraries(mmdMotionBinding_tests PRIVATE mmdMotionBinding::mmdMotionBinding)
add_test(NAME mmdMotionBinding_unit COMMAND mmdMotionBinding_tests)

# WORKSPACE.md §2.1 allows mmdMotionBinding two edges, mmdModel and motionVmd.
usdmmd_add_boundary_test(mmdMotionBinding
    TARGET mmdMotionBinding BINARY mmdMotionBinding_tests
    ARGS --allow mmdModel::mmdModel
         --allow motionVmd::motionVmd)
