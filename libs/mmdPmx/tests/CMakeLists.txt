# SPDX-License-Identifier: Apache-2.0

add_executable(mmdPmx_tests
    test_diagnostic.cpp
    test_reader.cpp
    test_main.cpp)
target_link_libraries(mmdPmx_tests PRIVATE mmdPmx::mmdPmx)
usdmmd_target_defaults(mmdPmx_tests)
# The suite checks with `assert()`, which NDEBUG compiles away -- and Release is
# the default build type -- so NDEBUG is undefined for this target only.
target_compile_options(mmdPmx_tests
    PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
add_test(NAME mmdPmx_unit COMMAND mmdPmx_tests)

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
