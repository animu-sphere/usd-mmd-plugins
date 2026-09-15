# SPDX-License-Identifier: Apache-2.0
#
# usdMmdFileFormat -- the OpenUSD SdfFileFormat bundle that imports .pmx
# (docs/design/DESIGN_POLICY.md §5.4).
#
# Self-contained, so `ost plugin build plugins/usdMmdFileFormat` can configure
# it directly; also add_subdirectory()'d by the repository root for the
# plain-CMake workflow.
cmake_minimum_required(VERSION 3.22)

# The repository-root VERSION file is the single source. A standalone export
# without the repository root falls back to the manifest's version.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION"
         _mmd_plugin_version LIMIT_COUNT 1)
    string(STRIP "${_mmd_plugin_version}" _mmd_plugin_version)
else()
    set(_mmd_plugin_version "0.0.0")  # mirrors openstrata.plugin.yaml
endif()

project(UsdMmdFileFormatPlugin
    VERSION ${_mmd_plugin_version}
    DESCRIPTION "OpenUSD pmx file-format plugin"
    LANGUAGES CXX)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_property(_isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _isMultiConfig AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdTargets.cmake")

# Resolved from the runtime OpenStrata activates, or from CMAKE_PREFIX_PATH in
# the plain-CMake flow. Idempotent when the root project already found it.
if(NOT pxr_FOUND)
    find_package(pxr REQUIRED CONFIG)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdOpenUsd.cmake")

# mmdPmx is a plain library, not a bundle. ost executes the manifest's
# requires.libraries closure -- it builds and installs the library into the
# workspace prefix before configuring this bundle -- and the composed root
# build already has the in-tree alias. Any other standalone caller supplies an
# installed package through CMAKE_PREFIX_PATH: a bundle never reaches into a
# sibling's source tree (WORKSPACE.md §5).
if(NOT TARGET mmdPmx::mmdPmx)
    find_package(mmdPmx CONFIG REQUIRED)
endif()

set(PLUGIN_NAME UsdMmdFileFormat)

add_library(${PLUGIN_NAME} SHARED
    src/UsdMmdFileFormat.cpp
    src/usd/UsdMmdAuthorer.cpp)

target_compile_features(${PLUGIN_NAME} PRIVATE cxx_std_20)
target_include_directories(${PLUGIN_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    ${PXR_INCLUDE_DIRS})
target_link_libraries(${PLUGIN_NAME} PRIVATE mmdPmx::mmdPmx)
usdmmd_target_defaults(${PLUGIN_NAME})

# The OpenUSD monolith when the install has one, else the discrete libraries
# this bundle uses (DEPENDENCIES.md §1 lists the modules).
if(TARGET usd_ms)
    target_link_libraries(${PLUGIN_NAME} PRIVATE usd_ms)
else()
    set(_pxr_targets)
    foreach(_t IN ITEMS arch tf gf vt ar sdf usd usdGeom kind)
        if(TARGET ${_t})
            list(APPEND _pxr_targets ${_t})
        elseif(TARGET pxr::${_t})
            list(APPEND _pxr_targets pxr::${_t})
        endif()
    endforeach()
    if(_pxr_targets)
        target_link_libraries(${PLUGIN_NAME} PRIVATE ${_pxr_targets})
    elseif(PXR_LIBRARIES)
        target_link_libraries(${PLUGIN_NAME} PRIVATE ${PXR_LIBRARIES})
    else()
        message(FATAL_ERROR "Could not locate OpenUSD CMake targets or PXR_LIBRARIES.")
    endif()
endif()

# Stage the shared library into the bundle's lib/, where both `ost plugin test`
# and plugInfo.json (LibraryPath = ../../../lib/<lib>) look for it. The
# bundle's own source directory, not CMAKE_SOURCE_DIR, so the location is the
# same standalone and composed.
set(USDMMD_PLUGIN_LIBRARY_PREFIX "lib")
set_target_properties(${PLUGIN_NAME} PROPERTIES
    PREFIX "${USDMMD_PLUGIN_LIBRARY_PREFIX}"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")
foreach(_cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(${PLUGIN_NAME} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_CURRENT_SOURCE_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_CURRENT_SOURCE_DIR}/lib"
        RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_CURRENT_SOURCE_DIR}/lib")
endforeach()

set(_mmd_resources "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/usdMmdFileFormat")

# The registration file for this platform's library suffix. The committed
# plugInfo.json lets the bundle be inspected before it is built; configuring
# rewrites it, so macOS gets .dylib, Windows .dll, and Linux .so.
configure_file("${_mmd_resources}/plugInfo.json.in" "${_mmd_resources}/plugInfo.json" @ONLY)

# buildInfo.json (WORKSPACE.md §4): what produced this binary, readable from a
# shipped package without the build tree. No timestamp, host name or absolute
# path, so an unchanged build packages to the same digest.
set(_mmd_git_commit "unknown")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_CURRENT_SOURCE_DIR}" rev-parse HEAD
        OUTPUT_VARIABLE _mmd_git_head
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _mmd_git_rc)
    if(_mmd_git_rc EQUAL 0)
        set(_mmd_git_commit "${_mmd_git_head}")
    endif()
endif()

# The stage-contract version has one source: the constant the authorer stamps
# on every stage. Read it rather than restate it.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/usd/UsdMmdAuthorer.h"
     _mmd_contract_line REGEX "kStageContractVersion = [0-9]+;")
string(REGEX MATCH "[0-9]+" _mmd_stage_contract_version "${_mmd_contract_line}")
if(NOT _mmd_stage_contract_version)
    message(FATAL_ERROR
        "usdMmdFileFormat: could not read kStageContractVersion from "
        "src/usd/UsdMmdAuthorer.h")
endif()

configure_file("${_mmd_resources}/buildInfo.json.in" "${_mmd_resources}/buildInfo.json" @ONLY)

# The installed bundle keeps the source bundle's shape -- lib/ beside
# plugin/resources/ -- so the committed relative LibraryPath resolves in both.
install(TARGETS ${PLUGIN_NAME}
    RUNTIME DESTINATION "lib"
    LIBRARY DESTINATION "lib")
install(FILES
    "${_mmd_resources}/plugInfo.json"
    "${_mmd_resources}/buildInfo.json"
    DESTINATION "plugin/resources/usdMmdFileFormat")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/openstrata.plugin.yaml" DESTINATION ".")

# No bundle-local tests yet: in Phase 0 everything this bundle does is observed
# by opening a fixture, which the workspace's integration tests do
# (tests/integration/) and `ost plugin test` does from the manifest's smoke
# list. Unit tests of the authorer arrive with mmdModel in Phase 2.
