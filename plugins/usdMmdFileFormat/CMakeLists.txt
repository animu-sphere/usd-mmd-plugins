# SPDX-License-Identifier: Apache-2.0
#
# usdMmdFileFormat -- the OpenUSD SdfFileFormat bundle that imports .pmx
# (docs/design/DESIGN_POLICY.md §5.4).
#
# Self-contained, so `ost plugin build plugins/usdMmdFileFormat` or plain
# CMake can configure it directly against installed packages; also
# add_subdirectory()'d by the repository root.
cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdProject.cmake")
project(UsdMmdFileFormatPlugin
    VERSION ${USDMMD_VERSION}
    DESCRIPTION "OpenUSD pmx file-format plugin"
    LANGUAGES CXX)
usdmmd_component()

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdTargets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdOpenUsd.cmake")

# OpenUSD, from the runtime OpenStrata activates or from CMAKE_PREFIX_PATH in
# the plain-CMake flow. The composed root build has already resolved it, and
# this reuses those targets; either way the pin is checked here.
usdmmd_find_openusd()

# mmdPmx and mmdModel are plain libraries, not bundles. ost executes the
# manifest's requires.libraries closure -- it builds and installs them into
# the workspace prefix before configuring this bundle -- and the composed root
# build already has the in-tree aliases. Any other standalone caller supplies
# installed packages through CMAKE_PREFIX_PATH: a bundle never reaches into a
# sibling's source tree (WORKSPACE.md §5).
if(NOT TARGET mmdPmx::mmdPmx)
    find_package(mmdPmx CONFIG REQUIRED)
endif()
if(NOT TARGET mmdModel::mmdModel)
    find_package(mmdModel CONFIG REQUIRED)
endif()

set(PLUGIN_NAME UsdMmdFileFormat)

add_library(${PLUGIN_NAME} SHARED
    src/UsdMmdFileFormat.cpp
    src/usd/UsdMmdAuthorer.cpp)

target_compile_features(${PLUGIN_NAME} PRIVATE cxx_std_20)
target_include_directories(${PLUGIN_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    ${PXR_INCLUDE_DIRS})
target_link_libraries(${PLUGIN_NAME} PRIVATE mmdModel::mmdModel mmdPmx::mmdPmx)
usdmmd_target_defaults(${PLUGIN_NAME})

# The OpenUSD monolith when the install has one, else the discrete libraries
# this bundle uses (DEPENDENCIES.md §1 lists the modules).
if(TARGET usd_ms)
    target_link_libraries(${PLUGIN_NAME} PRIVATE usd_ms)
else()
    set(_pxr_targets)
    foreach(_t IN ITEMS arch tf gf vt ar sdf usd usdGeom usdPhysics usdShade usdSkel kind)
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

# The registration file, for this platform's library suffix: .dll, .dylib or
# .so. It is generated, never committed -- one committed copy would name one
# platform's suffix and be rewritten by every configure on the others.
configure_file("${_mmd_resources}/plugInfo.json.in" "${_mmd_resources}/plugInfo.json" @ONLY)

# buildInfo.json (WORKSPACE.md §4): what produced this binary, readable from a
# shipped package without the build tree. No timestamp, host name or absolute
# path, so an unchanged build packages to the same digest.
#
# Its values must be current when the file is installed, so each has a way to
# refresh without a manual reconfigure: the project version and the
# stage-contract version are read from files that are configure dependencies,
# and the git commit is read at build time by cmake/WriteBuildInfo.cmake.

# The stage-contract version has one source: the constant the authorer stamps
# on every stage. Read it rather than restate it.
set(_mmd_authorer_header "${CMAKE_CURRENT_SOURCE_DIR}/src/usd/UsdMmdAuthorer.h")
file(STRINGS "${_mmd_authorer_header}"
     _mmd_contract_line REGEX "kStageContractVersion = [0-9]+;")
string(REGEX MATCH "[0-9]+" _mmd_stage_contract_version "${_mmd_contract_line}")
if(NOT _mmd_stage_contract_version)
    message(FATAL_ERROR
        "usdMmdFileFormat: could not read kStageContractVersion from "
        "src/usd/UsdMmdAuthorer.h")
endif()
# VERSION is one already: cmake/UsdMmdProject.cmake reads it.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_mmd_authorer_header}")

find_package(Git QUIET)
add_custom_target(UsdMmdFileFormat_buildInfo ALL
    COMMAND "${CMAKE_COMMAND}"
        "-DTEMPLATE=${_mmd_resources}/buildInfo.json.in"
        "-DOUTPUT=${_mmd_resources}/buildInfo.json"
        "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
        "-D_mmd_plugin_version=${PROJECT_VERSION}"
        "-D_mmd_stage_contract_version=${_mmd_stage_contract_version}"
        "-DUSDMMD_OPENUSD_RELEASE=${USDMMD_OPENUSD_RELEASE}"
        "-DCMAKE_CXX_COMPILER_ID=${CMAKE_CXX_COMPILER_ID}"
        "-DCMAKE_CXX_COMPILER_VERSION=${CMAKE_CXX_COMPILER_VERSION}"
        "-DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
        "-DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteBuildInfo.cmake"
    BYPRODUCTS "${_mmd_resources}/buildInfo.json"
    COMMENT "Stamping usdMmdFileFormat buildInfo.json"
    VERBATIM)
add_dependencies(${PLUGIN_NAME} UsdMmdFileFormat_buildInfo)

# The installed bundle keeps the source bundle's shape -- lib/ beside
# plugin/resources/ -- so the relative LibraryPath resolves in both.
install(TARGETS ${PLUGIN_NAME}
    RUNTIME DESTINATION "lib"
    LIBRARY DESTINATION "lib")
install(FILES
    "${_mmd_resources}/plugInfo.json"
    "${_mmd_resources}/buildInfo.json"
    DESTINATION "plugin/resources/usdMmdFileFormat")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/openstrata.plugin.yaml" DESTINATION ".")

# No bundle-local tests: everything this bundle does is observed by opening a
# fixture, which the workspace's integration tests do against the stage
# checklist (tests/integration/) and `ost plugin test` does from the
# manifest's smoke, roundtrip and negative lists. What the stage is made from
# is mmdModel's, and tested there.
