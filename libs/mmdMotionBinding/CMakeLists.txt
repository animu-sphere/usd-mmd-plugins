# SPDX-License-Identifier: Apache-2.0
#
# mmdMotionBinding -- binds a VMD motion to a PMX model by source name, in the
# model's basis (docs/design/MOTION_CONTRACT.md §8).
#
# A plain static library whose edges are mmdModel (and through it mmdPmx) and
# motionVmd, and which has NO OpenUSD dependency (docs/architecture/
# WORKSPACE.md §2). It is the one place a motion and a model meet, so neither
# motionVmd nor mmdModel has to know the other.
#
# Self-contained, so `ost library build libs/mmdMotionBinding` or plain CMake
# can configure it against installed packages; also add_subdirectory()'d by
# the repository root.
cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdProject.cmake")
project(mmdMotionBinding
    VERSION ${USDMMD_VERSION}
    DESCRIPTION "VMD-to-PMX motion binding for usd-mmd-plugins"
    LANGUAGES CXX)
usdmmd_component(TESTS_OPTION MMDMOTIONBINDING_BUILD_TESTS)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdTargets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdPackage.cmake")

# The declared edges (WORKSPACE.md §2.1). A composed build already has the
# targets; a standalone configure resolves the installed packages.
if(NOT TARGET mmdModel::mmdModel)
    find_package(mmdModel CONFIG REQUIRED)
endif()
if(NOT TARGET motionVmd::motionVmd)
    find_package(motionVmd CONFIG REQUIRED)
endif()

add_library(mmdMotionBinding STATIC
    src/Bind.cpp)
add_library(mmdMotionBinding::mmdMotionBinding ALIAS mmdMotionBinding)
set_target_properties(mmdMotionBinding PROPERTIES
    EXPORT_NAME mmdMotionBinding
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(mmdMotionBinding PUBLIC cxx_std_20)
target_include_directories(mmdMotionBinding
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
# PUBLIC: Bind.h takes both libraries' types.
target_link_libraries(mmdMotionBinding PUBLIC mmdModel::mmdModel motionVmd::motionVmd)
usdmmd_target_defaults(mmdMotionBinding)

usdmmd_install_library(mmdMotionBinding)

if(MMDMOTIONBINDING_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
