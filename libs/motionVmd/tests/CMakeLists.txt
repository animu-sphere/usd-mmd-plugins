# SPDX-License-Identifier: Apache-2.0

include("${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/UsdMmdTesting.cmake")

function(_mmd_vmd_test target)
    usdmmd_test_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE motionVmd::motionVmd)
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../src"
        "${CMAKE_CURRENT_SOURCE_DIR}")
endfunction()

_mmd_vmd_test(motionVmd_tests
    test_cp932.cpp
    test_reader.cpp
    test_motion.cpp
    test_main.cpp)
add_test(NAME motionVmd_unit COMMAND motionVmd_tests)

# Every byte of the sample motions overwritten and read back, and every prefix.
_mmd_vmd_test(motionVmd_robustness_tests test_robustness.cpp)
add_test(NAME motionVmd_robustness COMMAND motionVmd_robustness_tests)

# WORKSPACE.md §2.1 allows motionVmd no edge at all, and no header of the
# model libraries: motionVmd is extraction-ready (WORKSPACE.md §2.2).
usdmmd_add_boundary_test(motionVmd TARGET motionVmd BINARY motionVmd_tests
    ARGS --forbid-include mmdPmx/
         --forbid-include mmdModel/)

# The committed CP932 table is exactly what its generator writes.
usdmmd_test_python()
if(USDMMD_TEST_PYTHON)
    add_test(NAME motionVmd_cp932_table
        COMMAND "${USDMMD_TEST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../tools/generate_cp932_table.py" --check)
else()
    message(WARNING
        "motionVmd: no Python 3 interpreter; the table check is not registered")
endif()
