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
# Self-contained, so `ost library build libs/mmdControl` can configure it
# against installed packages; also add_subdirectory()'d by the repository root.
cmake_minimum_required(VERSION 3.22)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION"
         _mmd_control_version LIMIT_COUNT 1)
    string(STRIP "${_mmd_control_version}" _mmd_control_version)
else()
    set(_mmd_control_version "0.1.0")  # mirrors openstrata.library.yaml
endif()

project(mmdControl
    VERSION ${_mmd_control_version}
    DESCRIPTION "MMD control evaluation for usd-mmd-plugins"
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

if(NOT DEFINED MMDCONTROL_BUILD_TESTS)
    if(DEFINED USDMMD_BUILD_TESTS)
        set(_mmd_control_tests_default "${USDMMD_BUILD_TESTS}")
    else()
        set(_mmd_control_tests_default "${PROJECT_IS_TOP_LEVEL}")
    endif()
    option(MMDCONTROL_BUILD_TESTS "Build mmdControl tests"
           ${_mmd_control_tests_default})
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdTargets.cmake")

set(MMDCONTROL_SANITIZERS "" CACHE STRING
    "Sanitizers for mmdControl and its tests, e.g. \"address;undefined\" (Clang or GCC)")
if(MMDCONTROL_SANITIZERS)
    list(JOIN MMDCONTROL_SANITIZERS "," _mmd_control_sanitize)
    add_compile_options(-fsanitize=${_mmd_control_sanitize} -fno-sanitize-recover=all
                        -fno-omit-frame-pointer)
    add_link_options(-fsanitize=${_mmd_control_sanitize})
endif()

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

include(GNUInstallDirs)
install(TARGETS mmdControl
    EXPORT mmdControlTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mmdControlConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/mmdControlConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mmdControl")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/mmdControlConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion)
install(EXPORT mmdControlTargets
    FILE mmdControlTargets.cmake
    NAMESPACE mmdControl::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mmdControl")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/mmdControlConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/mmdControlConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mmdControl")

if(MMDCONTROL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
