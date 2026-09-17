# SPDX-License-Identifier: Apache-2.0
#
# vmd_inspect -- what a VMD contains, without a model and without USD
# (docs/design/MOTION_CONTRACT.md §2). It links motionVmd and nothing else:
# WORKSPACE.md §2.1 allows it no OpenUSD and no model library.
#
# Self-contained, so a standalone configure resolves motionVmd as an installed
# package; also add_subdirectory()'d by the repository root.
cmake_minimum_required(VERSION 3.22)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION"
         _mmd_vmd_inspect_version LIMIT_COUNT 1)
    string(STRIP "${_mmd_vmd_inspect_version}" _mmd_vmd_inspect_version)
else()
    set(_mmd_vmd_inspect_version "0.1.0")  # mirrors openstrata.tool.yaml
endif()

project(vmdInspect
    VERSION ${_mmd_vmd_inspect_version}
    DESCRIPTION "VMD inspection CLI for usd-mmd-plugins"
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

if(NOT DEFINED VMDINSPECT_BUILD_TESTS)
    if(DEFINED USDMMD_BUILD_TESTS)
        set(_vmd_inspect_tests_default "${USDMMD_BUILD_TESTS}")
    else()
        set(_vmd_inspect_tests_default "${PROJECT_IS_TOP_LEVEL}")
    endif()
    option(VMDINSPECT_BUILD_TESTS "Build vmd_inspect tests" ${_vmd_inspect_tests_default})
endif()

if(NOT TARGET motionVmd::motionVmd)
    find_package(motionVmd CONFIG REQUIRED)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdTargets.cmake")

add_executable(vmd_inspect
    src/Report.cpp
    src/main.cpp)
target_link_libraries(vmd_inspect PRIVATE motionVmd::motionVmd)
usdmmd_target_defaults(vmd_inspect)
usdmmd_use_utf8_code_page(vmd_inspect)

# Staged into the tool's own bin/: `ost` packages the directories
# openstrata.tool.yaml declares, not a build tree.
set_target_properties(vmd_inspect PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
foreach(_cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(vmd_inspect PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endforeach()

include(GNUInstallDirs)
install(TARGETS vmd_inspect RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

if(VMDINSPECT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
