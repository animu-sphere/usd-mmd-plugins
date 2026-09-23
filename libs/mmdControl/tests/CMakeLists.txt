# SPDX-License-Identifier: Apache-2.0

include("${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/UsdMmdTesting.cmake")

# Models are PMX documents stated as data and canonicalized, and motions are
# bound data: no PMX or VMD bytes are involved.
function(_mmd_control_test target)
    usdmmd_test_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE mmdControl::mmdControl)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
endfunction()

# Synthetic rigs with known answers (docs/design/MOTION_CONTRACT.md §11).
_mmd_control_test(mmdControl_tests test_sample.cpp test_evaluate.cpp test_main.cpp)
add_test(NAME mmdControl_unit COMMAND mmdControl_tests)

# Thousands of generated rigs and motions, each evaluated twice, bit for bit.
_mmd_control_test(mmdControl_robustness_tests test_robustness.cpp)
add_test(NAME mmdControl_robustness COMMAND mmdControl_robustness_tests)

# WORKSPACE.md §2.1 allows mmdControl two edges, mmdMotionBinding and
# mmdModel, and nothing of usd-motion-plugins.
usdmmd_add_boundary_test(mmdControl TARGET mmdControl BINARY mmdControl_tests
    ARGS --allow mmdMotionBinding::mmdMotionBinding
         --allow mmdModel::mmdModel
         --forbid-include motionCore/)
