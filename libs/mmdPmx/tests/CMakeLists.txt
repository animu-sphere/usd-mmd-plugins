# SPDX-License-Identifier: Apache-2.0

# Each suite checks with `assert()`, which NDEBUG compiles away -- and Release
# is the default build type -- so NDEBUG is undefined for these targets only.
# They see the library's private headers, to test the decoders and the
# diagnostic list directly.
function(_mmd_pmx_test target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE mmdPmx::mmdPmx)
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../src"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    usdmmd_target_defaults(${target})
    target_compile_options(${target}
        PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
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

# The link line exactly as CMake resolved it, for the boundary check below:
# WORKSPACE.md §2.1 allows mmdPmx no edge at all.
file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mmdPmx_link_$<CONFIG>.txt"
    CONTENT "LINK_LIBRARIES=$<TARGET_PROPERTY:mmdPmx,LINK_LIBRARIES>
INTERFACE_LINK_LIBRARIES=$<TARGET_PROPERTY:mmdPmx,INTERFACE_LINK_LIBRARIES>
")

if(NOT DEFINED USDMMD_TEST_PYTHON)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        set(USDMMD_TEST_PYTHON "${Python3_EXECUTABLE}")
    endif()
endif()
if(USDMMD_TEST_PYTHON)
    add_test(NAME mmdPmx_boundaries
        COMMAND "${USDMMD_TEST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../scripts/check_library_boundaries.py"
            --name mmdPmx
            --source "${CMAKE_CURRENT_SOURCE_DIR}/.."
            --link-file "${CMAKE_CURRENT_BINARY_DIR}/mmdPmx_link_$<CONFIG>.txt"
            --binary "$<TARGET_FILE:mmdPmx_tests>")
    # The check's rules against cases they must reject.
    add_test(NAME mmdPmx_boundaries_selftest
        COMMAND "${USDMMD_TEST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../scripts/check_library_boundaries.py"
            --selftest)
else()
    message(WARNING
        "mmdPmx: no Python 3 interpreter; the boundary check is not registered")
endif()
