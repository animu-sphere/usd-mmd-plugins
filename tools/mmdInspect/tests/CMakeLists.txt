# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED USDMMD_TEST_PYTHON)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        set(USDMMD_TEST_PYTHON "${Python3_EXECUTABLE}")
    endif()
endif()
if(NOT USDMMD_TEST_PYTHON)
    message(WARNING "mmd_inspect: no Python 3 interpreter; its tests are not registered")
    return()
endif()

set(_repo "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

# Every generated fixture through the tool, against fixtures.json: the parser
# end to end, from a file on disk, under an ASCII and a non-ASCII directory.
add_test(NAME mmd_inspect_fixtures
    COMMAND "${USDMMD_TEST_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/test_mmd_inspect.py"
        --tool "$<TARGET_FILE:mmd_inspect>"
        --generator "${_repo}/tests/fixtures/generate_fixtures.py")

# WORKSPACE.md §2.1 allows mmd_inspect mmdPmx and nothing else -- no OpenUSD.
file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mmd_inspect_link_$<CONFIG>.txt"
    CONTENT "LINK_LIBRARIES=$<TARGET_PROPERTY:mmd_inspect,LINK_LIBRARIES>
")
add_test(NAME mmd_inspect_boundaries
    COMMAND "${USDMMD_TEST_PYTHON}" "${_repo}/scripts/check_library_boundaries.py"
        --name mmd_inspect
        --source "${CMAKE_CURRENT_SOURCE_DIR}/.."
        --link-file "${CMAKE_CURRENT_BINARY_DIR}/mmd_inspect_link_$<CONFIG>.txt"
        --binary "$<TARGET_FILE:mmd_inspect>"
        --allow mmdPmx::mmdPmx)
