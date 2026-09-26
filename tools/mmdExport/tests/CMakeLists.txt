# SPDX-License-Identifier: Apache-2.0

include("${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/UsdMmdTesting.cmake")

# Each step on its own, over .usda layers and images the suite writes: no MMD
# plugin is loaded (PACKAGING_POLICY.md §14, "Unit").
usdmmd_test_executable(mmdExport_tests test_packaging.cpp)
target_link_libraries(mmdExport_tests PRIVATE mmdExportPackaging)
add_test(NAME mmdExport_unit COMMAND mmdExport_tests)
usdmmd_openusd_test_environment(mmdExport_unit)

usdmmd_test_python()
if(NOT USDMMD_TEST_PYTHON)
    message(WARNING "mmd_export: no Python 3 interpreter; its tool tests are not registered")
    return()
endif()

# WORKSPACE.md §2.1 allows mmd_export OpenUSD and nothing of this repository:
# no parser, canonical model or importer header, and no import of the
# importer's or the schema's library -- it reaches them through Plug.
set(_openusd_allow)
foreach(_t IN LISTS MMDEXPORT_OPENUSD_TARGETS)
    list(APPEND _openusd_allow --allow ${_t})
endforeach()
usdmmd_add_boundary_test(mmd_export TARGET mmdExportPackaging BINARY mmd_export
    ARGS --allow-openusd ${_openusd_allow}
         --forbid-include mmdPmx/ --forbid-include mmdModel/
         --forbid-include usd/UsdMmd --forbid-include UsdMmdFileFormat
         --forbid-include mmdSchema/
         --forbid-import UsdMmdFileFormat --forbid-import mmdSchema)

# The tool over the generated fixtures: each outcome of §7 and §11, a
# separate process with no MMD plugin opening every package and running
# usdchecker, the .pmx-against-.usdz comparison, and non-ASCII paths
# (PACKAGING_POLICY.md §14). The tool finds the importer as any host does,
# through PXR_PLUGINPATH_NAME; the plugin-free process is given none.
set(_repo "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
usdmmd_openusd_root(_usd_root)
find_program(MMDEXPORT_USDCHECKER usdchecker
    HINTS "${_usd_root}/bin" NO_DEFAULT_PATH)
set(_usdchecker)
if(MMDEXPORT_USDCHECKER)
    set(_usdchecker --usdchecker "${MMDEXPORT_USDCHECKER}")
endif()
add_test(NAME mmd_export_fixtures
    COMMAND "${USDMMD_TEST_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/test_mmd_export.py"
        --tool "$<TARGET_FILE:mmd_export>"
        --generator "${_repo}/tests/fixtures/generate_fixtures.py"
        --plugin-path "${_repo}/plugins/mmdSchema/plugin/resources/mmdSchema"
        --plugin-path "${_repo}/plugins/usdMmdFileFormat/plugin/resources/usdMmdFileFormat"
        ${_usdchecker})
if(_usd_root)
    set_tests_properties(mmd_export_fixtures PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${_usd_root}/bin;PATH=path_list_prepend:${_usd_root}/lib;PYTHONPATH=path_list_prepend:${_usd_root}/lib/python")
endif()
set_tests_properties(mmd_export_fixtures PROPERTIES TIMEOUT 300)
