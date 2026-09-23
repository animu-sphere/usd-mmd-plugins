# SPDX-License-Identifier: Apache-2.0
#
# mmdMotionAdapter -- fully evaluated mmdControl output as usd-motion-plugins'
# MotionClip (docs/design/MOTION_CONTRACT.md §10), with mmdSkeletonAdapter's
# roles and source rest.
#
# The other narrow adapter that crosses into usd-motion-plugins
# (docs/architecture/WORKSPACE.md §2.4): its edges are mmdControl, mmdModel,
# mmdSkeletonAdapter and the released motionCore package.
#
# Self-contained, so `ost library build libs/mmdMotionAdapter` or plain CMake
# can configure it against installed packages; also add_subdirectory()'d by
# the repository root.
cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdProject.cmake")
project(mmdMotionAdapter
    VERSION ${USDMMD_VERSION}
    DESCRIPTION "Evaluated MMD motion adapter for usd-motion-plugins"
    LANGUAGES CXX)
usdmmd_component(TESTS_OPTION MMDMOTIONADAPTER_BUILD_TESTS)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdTargets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdPackage.cmake")

# The declared edges (WORKSPACE.md §2.1). The three repository libraries are
# in-tree targets in the composed build; motionCore is always an installed
# package, never a sibling checkout (WORKSPACE.md §2.4).
if(NOT TARGET mmdModel::mmdModel)
    find_package(mmdModel CONFIG REQUIRED)
endif()
if(NOT TARGET mmdControl::mmdControl)
    find_package(mmdControl CONFIG REQUIRED)
endif()
if(NOT TARGET mmdSkeletonAdapter::mmdSkeletonAdapter)
    find_package(mmdSkeletonAdapter CONFIG REQUIRED)
endif()
if(NOT TARGET motionCore::motionCore)
    find_package(motionCore 0.5 CONFIG REQUIRED)
endif()

add_library(mmdMotionAdapter STATIC src/Adapter.cpp)
add_library(mmdMotionAdapter::mmdMotionAdapter ALIAS mmdMotionAdapter)
set_target_properties(mmdMotionAdapter PROPERTIES
    EXPORT_NAME mmdMotionAdapter
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(mmdMotionAdapter PUBLIC cxx_std_20)
target_include_directories(mmdMotionAdapter PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>")
# PUBLIC: Adapter.h takes mmdControl's and mmdSkeletonAdapter's types and
# returns motionCore's.
target_link_libraries(mmdMotionAdapter PUBLIC
    mmdControl::mmdControl
    mmdModel::mmdModel
    mmdSkeletonAdapter::mmdSkeletonAdapter
    motionCore::motionCore)
usdmmd_target_defaults(mmdMotionAdapter)

usdmmd_install_library(mmdMotionAdapter)

if(MMDMOTIONADAPTER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
