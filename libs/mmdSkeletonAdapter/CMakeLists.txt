# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.22)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION"
         _mmd_skeleton_adapter_version LIMIT_COUNT 1)
    string(STRIP "${_mmd_skeleton_adapter_version}" _mmd_skeleton_adapter_version)
else()
    set(_mmd_skeleton_adapter_version "0.1.0")
endif()

project(mmdSkeletonAdapter VERSION ${_mmd_skeleton_adapter_version}
    DESCRIPTION "MMD skeleton adapter for usd-motion-plugins" LANGUAGES CXX)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED MMDSKELETONADAPTER_BUILD_TESTS)
    if(DEFINED USDMMD_BUILD_TESTS)
        set(_mmd_skeleton_adapter_tests_default "${USDMMD_BUILD_TESTS}")
    else()
        set(_mmd_skeleton_adapter_tests_default "${PROJECT_IS_TOP_LEVEL}")
    endif()
    option(MMDSKELETONADAPTER_BUILD_TESTS "Build mmdSkeletonAdapter tests"
           ${_mmd_skeleton_adapter_tests_default})
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdTargets.cmake")

if(NOT TARGET mmdModel::mmdModel)
    find_package(mmdModel CONFIG REQUIRED)
endif()
if(NOT TARGET motionRetarget::motionRetarget)
    find_package(motionRetarget 0.5 CONFIG REQUIRED)
endif()

add_library(mmdSkeletonAdapter STATIC src/Adapter.cpp)
add_library(mmdSkeletonAdapter::mmdSkeletonAdapter ALIAS mmdSkeletonAdapter)
set_target_properties(mmdSkeletonAdapter PROPERTIES
    EXPORT_NAME mmdSkeletonAdapter POSITION_INDEPENDENT_CODE ON)
target_compile_features(mmdSkeletonAdapter PUBLIC cxx_std_20)
target_include_directories(mmdSkeletonAdapter PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:include>")
target_link_libraries(mmdSkeletonAdapter
    PUBLIC mmdModel::mmdModel motionRetarget::motionRetarget)
usdmmd_target_defaults(mmdSkeletonAdapter)

include(GNUInstallDirs)
install(TARGETS mmdSkeletonAdapter EXPORT mmdSkeletonAdapterTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mmdSkeletonAdapterConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/mmdSkeletonAdapterConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mmdSkeletonAdapter")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/mmdSkeletonAdapterConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
install(EXPORT mmdSkeletonAdapterTargets FILE mmdSkeletonAdapterTargets.cmake
    NAMESPACE mmdSkeletonAdapter::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mmdSkeletonAdapter")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/mmdSkeletonAdapterConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/mmdSkeletonAdapterConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mmdSkeletonAdapter")

if(MMDSKELETONADAPTER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
