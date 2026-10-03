# SPDX-License-Identifier: Apache-2.0
#
# mmdImaging's own tests, with no importer in the session
# (docs/architecture/WORKSPACE.md §6, docs/design/MATERIAL_POLICY.md §12.1).
# Each suite links OpenUSD and NOT the plugin: discovery goes through
# PXR_PLUGINPATH_NAME and the staged plugInfo.json, the path a packaged plugin
# takes.

include("${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/UsdMmdTesting.cmake")
usdmmd_openusd_root(_usd_root)

set(_imaging_test_targets)
if(TARGET usd_ms)
    set(_imaging_test_targets usd_ms)
else()
    foreach(_t IN ITEMS arch tf gf js vt plug ar sdf usd hd usdImaging)
        if(TARGET ${_t})
            list(APPEND _imaging_test_targets ${_t})
        elseif(TARGET pxr::${_t})
            list(APPEND _imaging_test_targets pxr::${_t})
        endif()
    endforeach()
endif()

# `import` is built here and registered by the workspace's tests/: its stages
# are the importer's fixtures, so it needs the importer in the same build,
# which only the composed root build has.
foreach(_suite IN ITEMS discovery material import)
    set(_exe mmdImaging_${_suite}_tests)
    usdmmd_test_executable(${_exe} test_mmd_imaging_${_suite}.cpp)
    target_include_directories(${_exe} PRIVATE ${PXR_INCLUDE_DIRS})
    target_link_libraries(${_exe} PRIVATE ${_imaging_test_targets})
    if(MSVC)
        # OpenUSD's hd/dataSource.h uses the std::atomic_* shared_ptr
        # overloads, deprecated in C++20.
        target_compile_definitions(${_exe} PRIVATE
            _SILENCE_CXX20_OLD_SHARED_PTR_ATOMIC_SUPPORT_DEPRECATION_WARNING)
    endif()
    # The plugin is loaded at run time, so the test has to be built after it.
    add_dependencies(${_exe} ${PLUGIN_NAME})
endforeach()

# The importer's fixtures have names no single ANSI code page spells.
usdmmd_use_utf8_code_page(mmdImaging_import_tests)

set(_bundle "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(_fixture "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/materials.usda")
set(_imaging_resources "${_bundle}/plugin/resources/mmdImaging")
# The schema bundle, from its sibling directory. Its library goes on PATH
# although nothing here loads it, so a plugin that one day did would not fail
# for a reason this file hides.
set(_schema_bundle "${_bundle}/../mmdSchema")
set(_schema_resources "${_schema_bundle}/plugin/resources/mmdSchema")

set(_imaging_path_env
    "PATH=path_list_prepend:${_usd_root}/bin"
    "PATH=path_list_prepend:${_usd_root}/lib"
    "PATH=path_list_prepend:${_bundle}/lib"
    "PATH=path_list_prepend:${_schema_bundle}/lib")

add_test(NAME mmdImaging_discovery COMMAND mmdImaging_discovery_tests)
add_test(NAME mmdImaging_material
    COMMAND mmdImaging_material_tests "${_fixture}")
set_tests_properties(mmdImaging_discovery mmdImaging_material PROPERTIES
    ENVIRONMENT_MODIFICATION
        "${_imaging_path_env};PXR_PLUGINPATH_NAME=path_list_prepend:${_imaging_resources};PXR_PLUGINPATH_NAME=path_list_prepend:${_schema_resources}")

# The same executable, in a session that registers no mmdSchema: the plugin
# links nothing of that bundle and still contributes nothing without it,
# because UsdImaging asks an API-schema adapter only about schemas a prim's
# definition includes. `set`, not a prepend: an activated ost session may
# already carry mmdSchema on PXR_PLUGINPATH_NAME, and the suite refuses to run
# under it.
add_test(NAME mmdImaging_material_without_schema
    COMMAND mmdImaging_material_tests "${_fixture}" --without-schema)
set_tests_properties(mmdImaging_material_without_schema PROPERTIES
    ENVIRONMENT_MODIFICATION
        "${_imaging_path_env};PXR_PLUGINPATH_NAME=set:${_imaging_resources}")

# WORKSPACE.md §2.1-§2.3: OpenUSD's imaging modules are the adapter's whole
# link line; no parser, canonical model, importer, schema or renderer header;
# and its library imports neither the importer's nor the schema's -- the
# schema is reached through the registry.
set(_openusd_allow)
foreach(_t IN LISTS MMDIMAGING_OPENUSD_TARGETS)
    list(APPEND _openusd_allow --allow ${_t})
endforeach()
usdmmd_add_boundary_test(mmdImaging TARGET ${PLUGIN_NAME} BINARY ${PLUGIN_NAME}
    ARGS --plugin ${_openusd_allow}
         --forbid-include mmdPmx/ --forbid-include mmdModel/
         --forbid-include usd/UsdMmd --forbid-include UsdMmdFileFormat
         --forbid-include mmdSchema/ --forbid-include hydraToon
         --forbid-import UsdMmdFileFormat --forbid-import mmdSchema)
