# SPDX-License-Identifier: Apache-2.0
#
# mmd_export -- an MMD model written out as conventional OpenUSD; version 1
# writes a self-contained USDZ (docs/design/PACKAGING_POLICY.md). It links OpenUSD and nothing of this
# repository: the importer is reached at run time through Plug, as any host
# reaches it, so a package is exactly the stage `Usd.Stage.Open` gives
# (WORKSPACE.md §2.1). Its link line is the cheapest place to keep that true.
#
# Self-contained, so a standalone configure -- plain CMake or `ost` --
# resolves OpenUSD itself; also add_subdirectory()'d by the repository root,
# after OpenUSD is resolved there.
cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdProject.cmake")
project(mmdExport
    VERSION ${USDMMD_VERSION}
    DESCRIPTION "USDZ packaging CLI for usd-mmd-plugins"
    LANGUAGES CXX)
usdmmd_component(TESTS_OPTION MMDEXPORT_BUILD_TESTS)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdTargets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdOpenUsd.cmake")
usdmmd_find_openusd()

# The OpenUSD libraries §2.1 allows it: discovery (usdUtils), the archive
# (sdf), the stage and its validators (usd, usdValidation), the image
# conversion (hio), and the foundation they are called through. The monolith
# when the install has one.
set(MMDEXPORT_OPENUSD_TARGETS)
if(TARGET usd_ms)
    set(MMDEXPORT_OPENUSD_TARGETS usd_ms)
else()
    foreach(_t IN ITEMS tf vt ar sdf usd usdUtils usdValidation hio)
        if(TARGET ${_t})
            list(APPEND MMDEXPORT_OPENUSD_TARGETS ${_t})
        elseif(TARGET pxr::${_t})
            list(APPEND MMDEXPORT_OPENUSD_TARGETS pxr::${_t})
        else()
            message(FATAL_ERROR "mmd_export: OpenUSD has no '${_t}' target")
        endif()
    endforeach()
endif()

# The steps of PACKAGING_POLICY.md §3, each a function the unit tests call
# directly. Private to the tool, and never installed (§12).
add_library(mmdExportPackaging STATIC src/Packaging.cpp)
target_compile_features(mmdExportPackaging PUBLIC cxx_std_20)
target_include_directories(mmdExportPackaging PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    ${PXR_INCLUDE_DIRS})
target_link_libraries(mmdExportPackaging PUBLIC ${MMDEXPORT_OPENUSD_TARGETS})
usdmmd_target_defaults(mmdExportPackaging)

add_executable(mmd_export src/main.cpp)
target_link_libraries(mmd_export PRIVATE mmdExportPackaging)
target_compile_definitions(mmd_export PRIVATE MMDEXPORT_VERSION="${PROJECT_VERSION}")
usdmmd_target_defaults(mmd_export)
usdmmd_use_utf8_code_page(mmd_export)

# Staged into the tool's own bin/: `ost` packages the directories
# openstrata.tool.yaml declares, not a build tree.
set_target_properties(mmd_export PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
foreach(_cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(mmd_export PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endforeach()

include(GNUInstallDirs)
install(TARGETS mmd_export RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

if(MMDEXPORT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
