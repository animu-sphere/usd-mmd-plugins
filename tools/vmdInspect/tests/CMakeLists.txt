# SPDX-License-Identifier: Apache-2.0

include("${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/UsdMmdTesting.cmake")

usdmmd_test_python()
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
usdmmd_add_boundary_test(vmd_inspect TARGET vmd_inspect BINARY vmd_inspect
    ARGS --allow motionVmd::motionVmd
         --forbid-include mmdPmx/
         --forbid-include mmdModel/)
