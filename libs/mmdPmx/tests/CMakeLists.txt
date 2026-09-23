# SPDX-License-Identifier: Apache-2.0

include("${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/UsdMmdTesting.cmake")

# The suites see the library's private headers, to test the decoders and the
# diagnostic list directly.
function(_mmd_pmx_test target)
    usdmmd_test_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE mmdPmx::mmdPmx)
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../src"
        "${CMAKE_CURRENT_SOURCE_DIR}")
endfunction()

_mmd_pmx_test(mmdPmx_tests
    PmxSamples.cpp
    test_diagnostic.cpp
    test_text.cpp
    test_reader.cpp
    test_main.cpp)
add_test(NAME mmdPmx_unit COMMAND mmdPmx_tests)

# Every byte of the sample models overwritten and read back
# (test_robustness.cpp): a deterministic cousin of the fuzz target.
_mmd_pmx_test(mmdPmx_robustness_tests
    PmxSamples.cpp
    test_robustness.cpp
    test_robustness_main.cpp)
add_test(NAME mmdPmx_robustness COMMAND mmdPmx_robustness_tests)

# WORKSPACE.md §2.1 allows mmdPmx no edge at all.
usdmmd_add_boundary_test(mmdPmx TARGET mmdPmx BINARY mmdPmx_tests)

# The check's rules against cases they must reject.
usdmmd_test_python()
if(USDMMD_TEST_PYTHON)
    add_test(NAME mmdPmx_boundaries_selftest
        COMMAND "${USDMMD_TEST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../scripts/check_library_boundaries.py"
            --selftest)
endif()
