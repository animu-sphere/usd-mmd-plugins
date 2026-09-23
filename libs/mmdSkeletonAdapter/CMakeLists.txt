# SPDX-License-Identifier: Apache-2.0
#
# mmdSkeletonAdapter -- a canonical PMX skeleton as usd-motion-plugins'
# SkeletonDescriptor, RetargetMap and SourceRestPose
# (docs/design/MOTION_CONTRACT.md §10.4).
#
# One of the two narrow adapters that cross into usd-motion-plugins
# (docs/architecture/WORKSPACE.md §2.4): its edges are mmdModel and the
# released motionRetarget package, which brings motionCore and the OpenUSD
# foundation types with it through its own package config.
#
# Self-contained, so `ost library build libs/mmdSkeletonAdapter` or plain
# CMake can configure it against installed packages; also
# add_subdirectory()'d by the repository root.
cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdProject.cmake")
project(mmdSkeletonAdapter
    VERSION ${USDMMD_VERSION}
    DESCRIPTION "MMD skeleton adapter for usd-motion-plugins"
    LANGUAGES CXX)
usdmmd_component(TESTS_OPTION MMDSKELETONADAPTER_BUILD_TESTS)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdTargets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdPackage.cmake")

# The declared edges (WORKSPACE.md §2.1). mmdModel is this repository's: the
# composed build already has the target. motionRetarget is always an
# installed package, never a sibling checkout (WORKSPACE.md §2.4).
if(NOT TARGET mmdModel::mmdModel)
    find_package(mmdModel CONFIG REQUIRED)
endif()
if(NOT TARGET motionRetarget::motionRetarget)
    find_package(motionRetarget 0.5 CONFIG REQUIRED)
endif()

add_library(mmdSkeletonAdapter STATIC src/Adapter.cpp)
add_library(mmdSkeletonAdapter::mmdSkeletonAdapter ALIAS mmdSkeletonAdapter)
set_target_properties(mmdSkeletonAdapter PROPERTIES
    EXPORT_NAME mmdSkeletonAdapter
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(mmdSkeletonAdapter PUBLIC cxx_std_20)
target_include_directories(mmdSkeletonAdapter PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>")
# PUBLIC: Adapter.h returns motionRetarget's types for mmdModel's.
target_link_libraries(mmdSkeletonAdapter
    PUBLIC mmdModel::mmdModel motionRetarget::motionRetarget)
usdmmd_target_defaults(mmdSkeletonAdapter)

usdmmd_install_library(mmdSkeletonAdapter)

if(MMDSKELETONADAPTER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
