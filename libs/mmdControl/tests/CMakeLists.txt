# SPDX-License-Identifier: Apache-2.0

# Each suite checks with `assert()`, which NDEBUG compiles away -- and Release
# is the default build type -- so NDEBUG is undefined for these targets only.
# Models are PMX documents stated as data and canonicalized, and motions are
# bound data: no PMX or VMD bytes are involved.
function(_mmd_control_test target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE mmdControl::mmdControl)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    usdmmd_target_defaults(${target})
    target_compile_options(${target}
        PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
endfunction()

# Synthetic rigs with known answers (docs/design/MOTION_CONTRACT.md §11).
_mmd_control_test(mmdControl_tests test_sample.cpp test_evaluate.cpp test_main.cpp)
add_test(NAME mmdControl_unit COMMAND mmdControl_tests)

# Thousands of generated rigs and motions, each evaluated twice, bit for bit.
_mmd_control_test(mmdControl_robustness_tests test_robustness.cpp)
add_test(NAME mmdControl_robustness COMMAND mmdControl_robustness_tests)

# The link line exactly as CMake resolved it: WORKSPACE.md §2.1 allows
# mmdControl two edges, mmdMotionBinding and mmdModel.
file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mmdControl_link_$<CONFIG>.txt"
    CONTENT "LINK_LIBRARIES=$<TARGET_PROPERTY:mmdControl,LINK_LIBRARIES>
INTERFACE_LINK_LIBRARIES=$<TARGET_PROPERTY:mmdControl,INTERFACE_LINK_LIBRARIES>
")

if(NOT DEFINED USDMMD_TEST_PYTHON)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        set(USDMMD_TEST_PYTHON "${Python3_EXECUTABLE}")
    endif()
endif()
if(USDMMD_TEST_PYTHON)
    add_test(NAME mmdControl_boundaries
        COMMAND "${USDMMD_TEST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../scripts/check_library_boundaries.py"
            --name mmdControl
            --source "${CMAKE_CURRENT_SOURCE_DIR}/.."
            --link-file "${CMAKE_CURRENT_BINARY_DIR}/mmdControl_link_$<CONFIG>.txt"
            --binary "$<TARGET_FILE:mmdControl_tests>"
            --allow mmdMotionBinding::mmdMotionBinding
            --allow mmdModel::mmdModel
            --forbid-include motionCore/)
else()
    message(WARNING
        "mmdControl: no Python 3 interpreter; the boundary check is not registered")
endif()
