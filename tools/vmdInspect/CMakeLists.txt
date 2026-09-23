# SPDX-License-Identifier: Apache-2.0
#
# vmd_inspect -- what a VMD contains, without a model and without USD
# (docs/design/MOTION_CONTRACT.md §2). It links motionVmd and nothing else:
# WORKSPACE.md §2.1 allows it no OpenUSD and no model library.
#
# Self-contained, so a standalone configure -- plain CMake or `ost` --
# resolves motionVmd as an installed package; also add_subdirectory()'d by the
# repository root.
cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdProject.cmake")
project(vmdInspect
    VERSION ${USDMMD_VERSION}
    DESCRIPTION "VMD inspection CLI for usd-mmd-plugins"
    LANGUAGES CXX)
usdmmd_component(TESTS_OPTION VMDINSPECT_BUILD_TESTS)

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
