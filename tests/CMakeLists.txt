# SPDX-License-Identifier: Apache-2.0
#
# Workspace-level tests: the fixtures themselves, and the integration tests
# that open them through the registered plugin (docs/architecture/WORKSPACE.md
# §6). A component's own unit tests live in that component.

if(NOT USDMMD_TEST_PYTHON)
    message(WARNING
        "usd-mmd-plugins: no Python 3 interpreter; the fixture and "
        "integration tests are not registered")
    return()
endif()

# The generator is workspace tooling; its committed output lives in the bundle
# whose manifest names it (tests/fixtures/generate_fixtures.py says why).
set(_bundle "${PROJECT_SOURCE_DIR}/plugins/usdMmdFileFormat")
set(_fixtures "${_bundle}/tests/fixtures")

# The committed fixtures are exactly what the committed generator writes.
add_test(NAME workspace_fixtures
    COMMAND "${USDMMD_TEST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/generate_fixtures.py"
        --check --out "${_fixtures}")

# The docs check the hand-written docs-check workflow runs, so a local ctest
# catches a broken link or a half-bumped version before a push does.
add_test(NAME workspace_docs
    COMMAND "${USDMMD_TEST_PYTHON}" "${PROJECT_SOURCE_DIR}/scripts/check_docs.py")
add_test(NAME workspace_docs_selftest
    COMMAND "${USDMMD_TEST_PYTHON}" "${PROJECT_SOURCE_DIR}/scripts/check_docs.py"
        --selftest)

include("${PROJECT_SOURCE_DIR}/cmake/UsdMmdTesting.cmake")

# The OpenUSD install root, from where pxrConfig.cmake was found.
usdmmd_openusd_root(_usd_root)

# The plugin as a host finds it: through the bundle's own plugInfo.json, whose
# relative LibraryPath points at the library staged into the bundle's lib/.
# With it, the registration of the mmdSchema bundle it requires. No search
# path names the schema's library: Plug loads it first, from the path its own
# plugInfo.json gives, because the importer's names it as a dependency.
set(_schema_bundle "${PROJECT_SOURCE_DIR}/plugins/mmdSchema")
set(_plugin_env
    "PATH=path_list_prepend:${_usd_root}/bin"
    "PATH=path_list_prepend:${_usd_root}/lib"
    "PYTHONPATH=path_list_prepend:${_usd_root}/lib/python"
    "PXR_PLUGINPATH_NAME=path_list_prepend:${_schema_bundle}/plugin/resources/mmdSchema"
    "PXR_PLUGINPATH_NAME=path_list_prepend:${_bundle}/plugin/resources/usdMmdFileFormat")

# Every fixture opened with Usd.Stage.Open, against fixtures.json and the
# Phase 0 part of the stage checklist (STAGE_CONTRACT.md §14).
add_test(NAME usdMmdFileFormat_stage_open
    COMMAND "${USDMMD_TEST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/integration/test_stage_open.py"
        --fixtures "${_fixtures}")
set_tests_properties(usdMmdFileFormat_stage_open PROPERTIES
    ENVIRONMENT_MODIFICATION "${_plugin_env}")

# The importer without the schema bundle it requires: Plug must refuse to load
# it, naming the schema type its plugInfo.json depends on.
add_test(NAME usdMmdFileFormat_schema_required
    COMMAND "${USDMMD_TEST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/integration/test_schema_required.py"
        --fixtures "${_fixtures}")
set_tests_properties(usdMmdFileFormat_schema_required PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${_usd_root}/bin;PATH=path_list_prepend:${_usd_root}/lib;PYTHONPATH=path_list_prepend:${_usd_root}/lib/python;PXR_PLUGINPATH_NAME=set:${_bundle}/plugin/resources/usdMmdFileFormat")

# The same open under a directory no single ANSI code page can spell, from a
# Python host (TEXT_ENCODING_POLICY.md §4).
add_test(NAME usdMmdFileFormat_unicode_paths
    COMMAND "${USDMMD_TEST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/integration/test_unicode_paths.py"
        --fixtures "${_fixtures}")
set_tests_properties(usdMmdFileFormat_unicode_paths PROPERTIES
    ENVIRONMENT_MODIFICATION "${_plugin_env}")

# Every Python entry point that reaches Read, from a host listening to every
# change notice: the importer authors on a worker thread, and must not wait for
# it while holding the GIL a Python listener on that thread needs.
add_test(NAME usdMmdFileFormat_notice_listeners
    COMMAND "${USDMMD_TEST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/integration/test_notice_listeners.py"
        --fixtures "${_fixtures}")
set_tests_properties(usdMmdFileFormat_notice_listeners PROPERTIES
    ENVIRONMENT_MODIFICATION "${_plugin_env}"
    TIMEOUT 120)

# The installed-consumer lane: install this build tree into a clean prefix and
# consume it from outside the repository (scripts/check_installed_consumer.py).
# It configures and builds a second CMake project, so it is labelled and can be
# left out with `ctest -LE installed-consumer`.
set(_consumer_toolchain --generator "${CMAKE_GENERATOR}")
if(CMAKE_GENERATOR MATCHES "Ninja|Makefiles")
    list(APPEND _consumer_toolchain
        --make-program "${CMAKE_MAKE_PROGRAM}"
        --cxx-compiler "${CMAKE_CXX_COMPILER}")
endif()
set(_motion_dependency_prefixes)
foreach(_motion_package_dir IN ITEMS "${motionCore_DIR}" "${motionRetarget_DIR}")
    if(_motion_package_dir)
        get_filename_component(_motion_package_prefix
            "${_motion_package_dir}/../../.." ABSOLUTE)
        list(APPEND _motion_dependency_prefixes "${_motion_package_prefix}")
    endif()
endforeach()
list(REMOVE_DUPLICATES _motion_dependency_prefixes)
set(_consumer_dependencies)
foreach(_motion_package_prefix IN LISTS _motion_dependency_prefixes)
    list(APPEND _consumer_dependencies --dependency-prefix "${_motion_package_prefix}")
endforeach()
set(_consumer_python)
# pxrConfig.cmake records the Python used to build the runtime as a fallback.
# A relocatable runtime can therefore name a path from its build host.  The
# root configure has already resolved that fallback against this host; carry
# the resolved values into the clean installed-consumer configure so it does
# not try the artifact producer's absolute path again.
foreach(_python_variable IN ITEMS
        Python3_EXECUTABLE Python3_LIBRARY Python3_INCLUDE_DIR)
    if(DEFINED ${_python_variable} AND NOT "${${_python_variable}}" STREQUAL "")
        string(TOLOWER "${_python_variable}" _python_option)
        string(REPLACE "_" "-" _python_option "${_python_option}")
        list(APPEND _consumer_python
            "--${_python_option}" "${${_python_variable}}")
    endif()
endforeach()
add_test(NAME workspace_installed_consumer
    COMMAND "${USDMMD_TEST_PYTHON}"
        "${PROJECT_SOURCE_DIR}/scripts/check_installed_consumer.py"
        --build-dir "${PROJECT_BINARY_DIR}"
        --config "$<CONFIG>"
        --usd-root "${_usd_root}"
        ${_consumer_python}
        ${_consumer_dependencies}
        ${_consumer_toolchain})
set_tests_properties(workspace_installed_consumer PROPERTIES
    LABELS "installed-consumer")
