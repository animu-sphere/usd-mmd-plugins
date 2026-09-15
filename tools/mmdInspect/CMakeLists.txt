# SPDX-License-Identifier: Apache-2.0
#
# mmd_inspect -- what a PMX contains, without USD (docs/design/DESIGN_POLICY.md
# §5.5). It links mmdPmx and nothing else: WORKSPACE.md §2.1 allows it no
# OpenUSD, and its link line is the cheapest place to keep that true.
#
# Self-contained, so a standalone configure resolves mmdPmx as an installed
# package; also add_subdirectory()'d by the repository root, which already has
# the in-tree target.
cmake_minimum_required(VERSION 3.22)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION"
         _mmd_inspect_version LIMIT_COUNT 1)
    string(STRIP "${_mmd_inspect_version}" _mmd_inspect_version)
else()
    set(_mmd_inspect_version "0.0.0")  # mirrors openstrata.tool.yaml
endif()

project(mmdInspect
    VERSION ${_mmd_inspect_version}
    DESCRIPTION "PMX inspection CLI for usd-mmd-plugins"
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

if(NOT DEFINED MMDINSPECT_BUILD_TESTS)
    if(DEFINED USDMMD_BUILD_TESTS)
        set(_mmd_inspect_tests_default "${USDMMD_BUILD_TESTS}")
    else()
        set(_mmd_inspect_tests_default "${PROJECT_IS_TOP_LEVEL}")
    endif()
    option(MMDINSPECT_BUILD_TESTS "Build mmd_inspect tests" ${_mmd_inspect_tests_default})
endif()

if(NOT TARGET mmdPmx::mmdPmx)
    find_package(mmdPmx CONFIG REQUIRED)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdTargets.cmake")

add_executable(mmd_inspect
    src/Report.cpp
    src/main.cpp)
target_link_libraries(mmd_inspect PRIVATE mmdPmx::mmdPmx)
usdmmd_target_defaults(mmd_inspect)
usdmmd_use_utf8_code_page(mmd_inspect)

# Staged into the tool's own bin/: `ost` packages the directories
# openstrata.tool.yaml declares, not a build tree.
set_target_properties(mmd_inspect PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
foreach(_cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(mmd_inspect PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endforeach()

include(GNUInstallDirs)
install(TARGETS mmd_inspect RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

if(MMDINSPECT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
