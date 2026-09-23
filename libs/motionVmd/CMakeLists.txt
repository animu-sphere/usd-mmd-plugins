# SPDX-License-Identifier: Apache-2.0
#
# motionVmd -- VMD syntax, CP932 decoding and the motion source representation
# (docs/design/MOTION_CONTRACT.md §2).
#
# A plain static library with NO dependency at all -- no OpenUSD, and nothing
# in this repository (docs/architecture/WORKSPACE.md §2): it is
# extraction-ready, written to leave for the shared motion architecture, so it
# may not reach mmdPmx or mmdModel either. The boundary test below fails if
# its sources include either, or its link line names anything.
#
# Self-contained, so `ost library build libs/motionVmd` or plain CMake can
# configure it directly; also add_subdirectory()'d by the repository root.
cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdProject.cmake")
project(motionVmd
    VERSION ${USDMMD_VERSION}
    DESCRIPTION "VMD motion reader for usd-mmd-plugins"
    LANGUAGES CXX)
usdmmd_component(TESTS_OPTION MOTIONVMD_BUILD_TESTS)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdTargets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdPackage.cmake")

add_library(motionVmd STATIC
    src/Cp932.cpp
    src/Diagnostic.cpp
    src/Motion.cpp
    src/Reader.cpp)
add_library(motionVmd::motionVmd ALIAS motionVmd)
set_target_properties(motionVmd PROPERTIES
    EXPORT_NAME motionVmd
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(motionVmd PUBLIC cxx_std_20)
target_include_directories(motionVmd
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
usdmmd_target_defaults(motionVmd)

usdmmd_install_library(motionVmd)

if(MOTIONVMD_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
# The libFuzzer target, as mmdPmx has one: a VMD reader takes untrusted bytes
# too.
if(USDMMD_BUILD_FUZZERS)
    add_subdirectory(fuzz)
endif()
