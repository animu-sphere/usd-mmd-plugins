# SPDX-License-Identifier: Apache-2.0

# Each suite checks with `assert()`, which NDEBUG compiles away -- and Release
# is the default build type -- so NDEBUG is undefined for these targets only.
function(_mmd_vmd_test target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE motionVmd::motionVmd)
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../src"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    usdmmd_target_defaults(${target})
    target_compile_options(${target}
        PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
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

# The link line exactly as CMake resolved it: WORKSPACE.md §2.1 allows
# motionVmd no edge at all.
file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/motionVmd_link_$<CONFIG>.txt"
    CONTENT "LINK_LIBRARIES=$<TARGET_PROPERTY:motionVmd,LINK_LIBRARIES>
INTERFACE_LINK_LIBRARIES=$<TARGET_PROPERTY:motionVmd,INTERFACE_LINK_LIBRARIES>
")

if(NOT DEFINED USDMMD_TEST_PYTHON)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        set(USDMMD_TEST_PYTHON "${Python3_EXECUTABLE}")
    endif()
endif()
if(USDMMD_TEST_PYTHON)
    # No OpenUSD, and no header of the model libraries: motionVmd is
    # extraction-ready (WORKSPACE.md §2.2).
    add_test(NAME motionVmd_boundaries
        COMMAND "${USDMMD_TEST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../scripts/check_library_boundaries.py"
            --name motionVmd
            --source "${CMAKE_CURRENT_SOURCE_DIR}/.."
            --link-file "${CMAKE_CURRENT_BINARY_DIR}/motionVmd_link_$<CONFIG>.txt"
            --binary "$<TARGET_FILE:motionVmd_tests>"
            --forbid-include mmdPmx/
            --forbid-include mmdModel/)
    # The committed CP932 table is exactly what its generator writes.
    add_test(NAME motionVmd_cp932_table
        COMMAND "${USDMMD_TEST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../tools/generate_cp932_table.py" --check)
else()
    message(WARNING
        "motionVmd: no Python 3 interpreter; the boundary and table checks are not registered")
endif()
