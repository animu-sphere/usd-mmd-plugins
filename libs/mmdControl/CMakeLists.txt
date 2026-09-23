# SPDX-License-Identifier: Apache-2.0
#
# mmdControl -- MMD control evaluation: a bound motion sampled at an explicit
# time and evaluated over the model's control rig -- bone morphs, appends, IK
# -- into deformation-joint transforms (docs/design/MOTION_CONTRACT.md §11).
#
# A plain static library whose edges are mmdMotionBinding and mmdModel, and
# which has NO OpenUSD dependency and nothing of usd-motion-plugins
# (docs/architecture/WORKSPACE.md §2).
#
# Self-contained, so `ost library build libs/mmdControl` or plain CMake can
# configure it against installed packages; also add_subdirectory()'d by the
# repository root.
cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdProject.cmake")
project(mmdControl
    VERSION ${USDMMD_VERSION}
    DESCRIPTION "MMD control evaluation for usd-mmd-plugins"
    LANGUAGES CXX)
usdmmd_component(TESTS_OPTION MMDCONTROL_BUILD_TESTS)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdTargets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdPackage.cmake")

# The declared edges (WORKSPACE.md §2.1). A composed build already has the
# targets; a standalone configure resolves the installed packages.
if(NOT TARGET mmdModel::mmdModel)
    find_package(mmdModel CONFIG REQUIRED)
endif()
if(NOT TARGET mmdMotionBinding::mmdMotionBinding)
    find_package(mmdMotionBinding CONFIG REQUIRED)
endif()

add_library(mmdControl STATIC
    src/Evaluator.cpp
    src/Sample.cpp)
add_library(mmdControl::mmdControl ALIAS mmdControl)
set_target_properties(mmdControl PROPERTIES
    EXPORT_NAME mmdControl
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(mmdControl PUBLIC cxx_std_20)
target_include_directories(mmdControl
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
# PUBLIC: Evaluator.h takes both libraries' types.
target_link_libraries(mmdControl PUBLIC mmdMotionBinding::mmdMotionBinding mmdModel::mmdModel)
usdmmd_target_defaults(mmdControl)

usdmmd_install_library(mmdControl)

if(MMDCONTROL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
