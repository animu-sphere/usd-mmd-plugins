# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.22)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION"
         _mmd_motion_adapter_version LIMIT_COUNT 1)
    string(STRIP "${_mmd_motion_adapter_version}" _mmd_motion_adapter_version)
else()
    set(_mmd_motion_adapter_version "0.1.0")
endif()

project(mmdMotionAdapter VERSION ${_mmd_motion_adapter_version}
    DESCRIPTION "Evaluated MMD motion adapter for usd-motion-plugins" LANGUAGES CXX)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED MMDMOTIONADAPTER_BUILD_TESTS)
    if(DEFINED USDMMD_BUILD_TESTS)
        set(_mmd_motion_adapter_tests_default "${USDMMD_BUILD_TESTS}")
    else()
        set(_mmd_motion_adapter_tests_default "${PROJECT_IS_TOP_LEVEL}")
    endif()
    option(MMDMOTIONADAPTER_BUILD_TESTS "Build mmdMotionAdapter tests"
           ${_mmd_motion_adapter_tests_default})
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdTargets.cmake")

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
    EXPORT_NAME mmdMotionAdapter POSITION_INDEPENDENT_CODE ON)
target_compile_features(mmdMotionAdapter PUBLIC cxx_std_20)
target_include_directories(mmdMotionAdapter PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>")
target_link_libraries(mmdMotionAdapter PUBLIC
    mmdControl::mmdControl
    mmdModel::mmdModel
    mmdSkeletonAdapter::mmdSkeletonAdapter
    motionCore::motionCore)
usdmmd_target_defaults(mmdMotionAdapter)

include(GNUInstallDirs)
install(TARGETS mmdMotionAdapter EXPORT mmdMotionAdapterTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mmdMotionAdapterConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/mmdMotionAdapterConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mmdMotionAdapter")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/mmdMotionAdapterConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
install(EXPORT mmdMotionAdapterTargets FILE mmdMotionAdapterTargets.cmake
    NAMESPACE mmdMotionAdapter::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mmdMotionAdapter")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/mmdMotionAdapterConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/mmdMotionAdapterConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mmdMotionAdapter")

if(MMDMOTIONADAPTER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
