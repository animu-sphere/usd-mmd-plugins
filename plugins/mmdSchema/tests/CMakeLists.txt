# SPDX-License-Identifier: Apache-2.0
#
# mmdSchema's own tests, with no importer in the session
# (docs/architecture/WORKSPACE.md §6). `ost plugin test plugins/mmdSchema`
# runs the manifest's pyramid over tests/fixtures/basic.usda; these run the
# same fixture through CTest.

include("${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/UsdMmdTesting.cmake")
usdmmd_test_python()
usdmmd_openusd_root(_usd_root)

set(_bundle "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(_fixture "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/basic.usda")

# The bundle as a host finds it: its own configured plugInfo.json, whose
# relative LibraryPath names the library staged into the bundle's lib/.
set(_schema_env
    "PATH=path_list_prepend:${_usd_root}/bin"
    "PATH=path_list_prepend:${_usd_root}/lib"
    "PATH=path_list_prepend:${_bundle}/lib"
    "PYTHONPATH=path_list_prepend:${_usd_root}/lib/python"
    "PXR_PLUGINPATH_NAME=path_list_prepend:${_bundle}/plugin/resources/mmdSchema")

# The generated C++ accessors, as the UsdImaging adapter will use them.
usdmmd_test_executable(mmdSchema_material_api test_material_api.cpp)
target_link_libraries(mmdSchema_material_api PRIVATE mmdSchema::mmdSchema)
if(NOT TARGET usd_ms)
    if(TARGET usdShade)
        target_link_libraries(mmdSchema_material_api PRIVATE usdShade)
    elseif(TARGET pxr::usdShade)
        target_link_libraries(mmdSchema_material_api PRIVATE pxr::usdShade)
    endif()
endif()
add_test(NAME mmdSchema_material_api
    COMMAND mmdSchema_material_api "${_fixture}")
set_tests_properties(mmdSchema_material_api PROPERTIES
    ENVIRONMENT_MODIFICATION "${_schema_env}")

if(NOT USDMMD_TEST_PYTHON)
    message(STATUS "mmdSchema: no Python 3 interpreter; the registry and generation tests are not registered")
    return()
endif()

# Registration, the §4.1 inventory, connectability and the fixture, through
# the schema registry alone.
add_test(NAME mmdSchema_plugin
    COMMAND "${USDMMD_TEST_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/test_mmdschema_plugin.py")
set_tests_properties(mmdSchema_plugin PROPERTIES
    ENVIRONMENT_MODIFICATION "${_schema_env}")

# The committed usdGenSchema output is exactly what schema/schema.usda
# generates. Needs usdGenSchema, which OpenUSD installs only when it was built
# with jinja2, and jinja2 in the test interpreter; a host without either skips
# the check rather than failing it.
set(_mmd_jinja2 1)
if(_usd_root AND EXISTS "${_usd_root}/bin/usdGenSchema")
    execute_process(
        COMMAND "${USDMMD_TEST_PYTHON}" -c "import jinja2"
        RESULT_VARIABLE _mmd_jinja2
        OUTPUT_QUIET ERROR_QUIET)
endif()
if(_mmd_jinja2 EQUAL 0)
    add_test(NAME mmdSchema_generated
        COMMAND "${USDMMD_TEST_PYTHON}" "${_bundle}/tools/generate_schema.py"
            --check --usd-root "${_usd_root}")
else()
    message(STATUS "mmdSchema: no usdGenSchema or jinja2; mmdSchema_generated is not registered")
endif()
