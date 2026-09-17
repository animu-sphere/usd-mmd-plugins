# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED USDMMD_TEST_PYTHON)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        set(USDMMD_TEST_PYTHON "${Python3_EXECUTABLE}")
    endif()
endif()
if(NOT USDMMD_TEST_PYTHON)
    message(WARNING "vmd_inspect: no Python 3 interpreter; its tests are not registered")
    return()
endif()

set(_repo "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

# Every generated VMD fixture through the tool: the reader end to end, from a
# file on disk, under an ASCII and a non-ASCII directory.
add_test(NAME vmd_inspect_fixtures
    COMMAND "${USDMMD_TEST_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/test_vmd_inspect.py"
        --tool "$<TARGET_FILE:vmd_inspect>"
        --generator "${_repo}/tests/fixtures/generate_vmd_fixtures.py")

# WORKSPACE.md §2.1 allows vmd_inspect motionVmd and nothing else.
file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/vmd_inspect_link_$<CONFIG>.txt"
    CONTENT "LINK_LIBRARIES=$<TARGET_PROPERTY:vmd_inspect,LINK_LIBRARIES>
")
add_test(NAME vmd_inspect_boundaries
    COMMAND "${USDMMD_TEST_PYTHON}" "${_repo}/scripts/check_library_boundaries.py"
        --name vmd_inspect
        --source "${CMAKE_CURRENT_SOURCE_DIR}/.."
        --link-file "${CMAKE_CURRENT_BINARY_DIR}/vmd_inspect_link_$<CONFIG>.txt"
        --binary "$<TARGET_FILE:vmd_inspect>"
        --allow motionVmd::motionVmd
        --forbid-include mmdPmx/
        --forbid-include mmdModel/)
