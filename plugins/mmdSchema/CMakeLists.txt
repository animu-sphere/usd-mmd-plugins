# SPDX-License-Identifier: Apache-2.0
#
# mmdSchema -- the typed MMD API schemas, today MmdMaterialAPI alone
# (docs/design/MATERIAL_POLICY.md §4.3, docs/architecture/WORKSPACE.md §1.2).
#
# Self-contained, so `ost plugin build plugins/mmdSchema` or plain CMake can
# configure it directly against OpenUSD; also add_subdirectory()'d by the
# repository root. Consumers resolve it as an installed package:
# find_package(mmdSchema CONFIG REQUIRED) and link mmdSchema::mmdSchema.
cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdProject.cmake")
project(mmdSchema
    VERSION ${USDMMD_VERSION}
    DESCRIPTION "Typed MMD API schemas for OpenUSD"
    LANGUAGES CXX)
usdmmd_component(TESTS_OPTION MMDSCHEMA_BUILD_TESTS)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdTargets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdOpenUsd.cmake")
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# OpenUSD, and nothing else (WORKSPACE.md §2.1).
usdmmd_find_openusd()

set(PLUGIN_NAME mmdSchema)

add_library(${PLUGIN_NAME} SHARED)
add_library(mmdSchema::mmdSchema ALIAS ${PLUGIN_NAME})

# usdGenSchema's output, committed so a plain-CMake build needs neither `ost`
# nor usdGenSchema; tools/generate_schema.py is its one author, and
# mmdSchema_generated checks it against schema/schema.usda. An `ost` build
# regenerates the same sources from schema/schema.usda in the resolved
# runtime and hands them over in this fragment instead.
if(DEFINED OPENSTRATA_SCHEMA_SOURCES_FILE AND EXISTS "${OPENSTRATA_SCHEMA_SOURCES_FILE}")
    include("${OPENSTRATA_SCHEMA_SOURCES_FILE}")
else()
    target_sources(${PLUGIN_NAME} PRIVATE
        src/mmdSchema/tokens.cpp
        src/mmdSchema/mmdMaterialAPI.cpp)
endif()

# Headers live under src/mmdSchema/, so in-tree and installed consumers both
# write #include <mmdSchema/...>. OpenUSD's own headers reach a consumer
# through the OpenUSD targets linked below, never as a path recorded here.
target_compile_features(${PLUGIN_NAME} PUBLIC cxx_std_17)
target_include_directories(${PLUGIN_NAME}
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/mmdSchema"
        ${PXR_INCLUDE_DIRS})
# The generated MMDSCHEMA_API is ARCH_EXPORT only where MMDSCHEMA_EXPORTS is
# defined -- this library -- and ARCH_IMPORT for every consumer.
target_compile_definitions(${PLUGIN_NAME} PRIVATE MMDSCHEMA_EXPORTS)
usdmmd_target_defaults(${PLUGIN_NAME})

# The OpenUSD monolith when the install has one, else the modules the
# generated code uses. Public because the installed headers include
# OpenUSD's, and a consumer uses the same ABI-bearing types.
if(TARGET usd_ms)
    target_link_libraries(${PLUGIN_NAME} PUBLIC usd_ms)
else()
    set(_pxr_targets)
    foreach(_t IN ITEMS arch tf gf vt sdf usd)
        if(TARGET ${_t})
            list(APPEND _pxr_targets ${_t})
        elseif(TARGET pxr::${_t})
            list(APPEND _pxr_targets pxr::${_t})
        endif()
    endforeach()
    if(_pxr_targets)
        target_link_libraries(${PLUGIN_NAME} PUBLIC ${_pxr_targets})
    elseif(PXR_LIBRARIES)
        target_link_libraries(${PLUGIN_NAME} PUBLIC ${PXR_LIBRARIES})
    else()
        message(FATAL_ERROR "Could not locate OpenUSD CMake targets or PXR_LIBRARIES.")
    endif()
endif()

# Staged into the bundle's lib/, where both `ost plugin test` and
# plugInfo.json (LibraryPath = ../../../lib/<lib>) look for it, as
# usdMmdFileFormat's is.
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

# The registration file, for this platform's library suffix; generated, never
# committed, like the file-format bundle's. The template carries the schema
# Types, which tools/generate_schema.py writes.
set(_mmd_resources "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/mmdSchema")
configure_file("${_mmd_resources}/plugInfo.json.in" "${_mmd_resources}/plugInfo.json" @ONLY)

# The installed bundle keeps the source bundle's shape -- lib/ beside
# plugin/resources/ -- so the relative LibraryPath resolves in both, and is
# also a CMake package: the import library, the headers and the export.
install(TARGETS ${PLUGIN_NAME}
    EXPORT mmdSchemaTargets
    RUNTIME DESTINATION "lib"
    LIBRARY DESTINATION "lib"
    ARCHIVE DESTINATION "lib"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(FILES
    src/mmdSchema/api.h
    src/mmdSchema/tokens.h
    src/mmdSchema/mmdMaterialAPI.h
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/mmdSchema")
install(FILES
    "${_mmd_resources}/plugInfo.json"
    "${_mmd_resources}/generatedSchema.usda"
    DESTINATION "plugin/resources/mmdSchema")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/openstrata.plugin.yaml" DESTINATION ".")

set(_package_dir "lib/cmake/mmdSchema")
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mmdSchemaConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/mmdSchemaConfig.cmake"
    INSTALL_DESTINATION "${_package_dir}")
# Pre-1.0, a minor version may break the API: the same policy as every
# package of this workspace (PACKAGE_CONTRACT.md).
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/mmdSchemaConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion)
install(EXPORT mmdSchemaTargets
    FILE mmdSchemaTargets.cmake
    NAMESPACE mmdSchema::
    DESTINATION "${_package_dir}")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/mmdSchemaConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/mmdSchemaConfigVersion.cmake"
    DESTINATION "${_package_dir}")

if(MMDSCHEMA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
