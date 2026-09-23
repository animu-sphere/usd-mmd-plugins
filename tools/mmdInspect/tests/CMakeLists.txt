# SPDX-License-Identifier: Apache-2.0

include("${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/UsdMmdTesting.cmake")

usdmmd_test_python()
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
usdmmd_add_boundary_test(mmd_inspect TARGET mmd_inspect BINARY mmd_inspect
    ARGS --allow mmdPmx::mmdPmx)
