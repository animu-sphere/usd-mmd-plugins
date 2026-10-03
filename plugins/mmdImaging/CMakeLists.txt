# SPDX-License-Identifier: Apache-2.0
#
# mmdImaging -- MmdMaterialAPI made visible to Hydra
# (docs/design/MATERIAL_POLICY.md §12.1, docs/architecture/WORKSPACE.md §1.2).
# A UsdImaging API-schema adapter contributing the canonical material values
# and the draw order to the Hydra material prim, and turning a changed USD
# property into the one locator it feeds. No renderer, no shader, no image
# loading: what an MMD material says, not how it is drawn.
#
# Self-contained, so `ost plugin build plugins/mmdImaging` or plain CMake can
# configure it directly against OpenUSD; also add_subdirectory()'d by the
# repository root.
cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdProject.cmake")
project(mmdImaging
    VERSION ${USDMMD_VERSION}
    DESCRIPTION "UsdImaging adapter exposing MmdMaterialAPI to Hydra"
    LANGUAGES CXX)
usdmmd_component(TESTS_OPTION MMDIMAGING_BUILD_TESTS)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdTargets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdOpenUsd.cmake")
include(GNUInstallDirs)

# OpenUSD, and nothing else (WORKSPACE.md §2.1). mmdSchema is NOT linked: the
# adapter needs the schema registered, which is mmdSchema's plugInfo.json, and
# reads the field names from the registered definition rather than from
# UsdMmdTokens. Nothing of the importer is reachable either (WORKSPACE.md
# §2.2): the composed stage is the boundary.
usdmmd_find_openusd()

set(PLUGIN_NAME MmdImaging)
add_library(${PLUGIN_NAME} SHARED src/MmdMaterialAPIAdapter.cpp)

target_include_directories(${PLUGIN_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    ${PXR_INCLUDE_DIRS})
usdmmd_target_defaults(${PLUGIN_NAME})
if(MSVC)
    # OpenUSD's hd/dataSource.h uses the std::atomic_* shared_ptr overloads,
    # deprecated in C++20, which this workspace compiles.
    target_compile_definitions(${PLUGIN_NAME} PRIVATE
        _SILENCE_CXX20_OLD_SHARED_PTR_ATOMIC_SUPPORT_DEPRECATION_WARNING)
endif()

# The OpenUSD monolith when the install has one, else the imaging modules.
# A runtime built without imaging cannot host a UsdImaging adapter, so a
# missing module is a configure error rather than a silently absent plugin.
set(MMDIMAGING_OPENUSD_TARGETS)
if(TARGET usd_ms)
    set(MMDIMAGING_OPENUSD_TARGETS usd_ms)
else()
    foreach(_t IN ITEMS arch tf vt sdf usd hd usdImaging)
        if(TARGET ${_t})
            list(APPEND MMDIMAGING_OPENUSD_TARGETS ${_t})
        elseif(TARGET pxr::${_t})
            list(APPEND MMDIMAGING_OPENUSD_TARGETS pxr::${_t})
        else()
            message(FATAL_ERROR
                "mmdImaging needs the OpenUSD target '${_t}', which this "
                "install does not define. A runtime built without imaging "
                "cannot host a UsdImaging adapter.")
        endif()
    endforeach()
endif()
target_link_libraries(${PLUGIN_NAME} PRIVATE ${MMDIMAGING_OPENUSD_TARGETS})

# Staged into the bundle's lib/, where both `ost plugin test` and
# plugInfo.json (LibraryPath = ../../../lib/<lib>) look for it, as the other
# bundles' are.
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
# committed, like the other bundles'.
set(_mmd_resources "${CMAKE_CURRENT_SOURCE_DIR}/plugin/resources/mmdImaging")
configure_file("${_mmd_resources}/plugInfo.json.in" "${_mmd_resources}/plugInfo.json" @ONLY)

# The installed bundle keeps the source bundle's shape -- lib/ beside
# plugin/resources/ -- so the relative LibraryPath resolves in both. No
# headers and no CMake package: a Hydra consumer spells the locators itself
# (MATERIAL_POLICY.md §12.1 item 7).
install(TARGETS ${PLUGIN_NAME}
    RUNTIME DESTINATION "lib"
    LIBRARY DESTINATION "lib")
install(FILES "${_mmd_resources}/plugInfo.json"
    DESTINATION "plugin/resources/mmdImaging")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/openstrata.plugin.yaml" DESTINATION ".")

if(MMDIMAGING_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
