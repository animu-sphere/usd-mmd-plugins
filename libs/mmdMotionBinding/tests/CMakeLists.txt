# SPDX-License-Identifier: Apache-2.0

# `assert()` checks, so NDEBUG is undefined for the test target only.
add_executable(mmdMotionBinding_tests test_bind.cpp)
target_link_libraries(mmdMotionBinding_tests PRIVATE mmdMotionBinding::mmdMotionBinding)
usdmmd_target_defaults(mmdMotionBinding_tests)
target_compile_options(mmdMotionBinding_tests
    PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
add_test(NAME mmdMotionBinding_unit COMMAND mmdMotionBinding_tests)

# The link line exactly as CMake resolved it: WORKSPACE.md §2.1 allows
# mmdMotionBinding two edges, mmdModel and motionVmd.
file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mmdMotionBinding_link_$<CONFIG>.txt"
    CONTENT "LINK_LIBRARIES=$<TARGET_PROPERTY:mmdMotionBinding,LINK_LIBRARIES>
INTERFACE_LINK_LIBRARIES=$<TARGET_PROPERTY:mmdMotionBinding,INTERFACE_LINK_LIBRARIES>
")

if(NOT DEFINED USDMMD_TEST_PYTHON)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        set(USDMMD_TEST_PYTHON "${Python3_EXECUTABLE}")
    endif()
endif()
if(USDMMD_TEST_PYTHON)
    add_test(NAME mmdMotionBinding_boundaries
        COMMAND "${USDMMD_TEST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../scripts/check_library_boundaries.py"
            --name mmdMotionBinding
            --source "${CMAKE_CURRENT_SOURCE_DIR}/.."
            --link-file "${CMAKE_CURRENT_BINARY_DIR}/mmdMotionBinding_link_$<CONFIG>.txt"
            --binary "$<TARGET_FILE:mmdMotionBinding_tests>"
            --allow mmdModel::mmdModel
            --allow motionVmd::motionVmd)
else()
    message(WARNING
        "mmdMotionBinding: no Python 3 interpreter; the boundary check is not registered")
endif()
