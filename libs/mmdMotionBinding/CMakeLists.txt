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
# Self-contained, so `ost library build libs/mmdMotionBinding` can configure it
# against installed packages; also add_subdirectory()'d by the repository root.
cmake_minimum_required(VERSION 3.22)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION"
         _mmd_binding_version LIMIT_COUNT 1)
    string(STRIP "${_mmd_binding_version}" _mmd_binding_version)
else()
    set(_mmd_binding_version "0.0.0")  # mirrors openstrata.library.yaml
endif()

project(mmdMotionBinding
    VERSION ${_mmd_binding_version}
    DESCRIPTION "VMD-to-PMX motion binding for usd-mmd-plugins"
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

if(NOT DEFINED MMDMOTIONBINDING_BUILD_TESTS)
    if(DEFINED USDMMD_BUILD_TESTS)
        set(_mmd_binding_tests_default "${USDMMD_BUILD_TESTS}")
    else()
        set(_mmd_binding_tests_default "${PROJECT_IS_TOP_LEVEL}")
    endif()
    option(MMDMOTIONBINDING_BUILD_TESTS "Build mmdMotionBinding tests"
           ${_mmd_binding_tests_default})
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdTargets.cmake")

set(MMDMOTIONBINDING_SANITIZERS "" CACHE STRING
    "Sanitizers for mmdMotionBinding and its tests, e.g. \"address;undefined\" (Clang or GCC)")
if(MMDMOTIONBINDING_SANITIZERS)
    list(JOIN MMDMOTIONBINDING_SANITIZERS "," _mmd_binding_sanitize)
    add_compile_options(-fsanitize=${_mmd_binding_sanitize} -fno-sanitize-recover=all
                        -fno-omit-frame-pointer)
    add_link_options(-fsanitize=${_mmd_binding_sanitize})
endif()

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

include(GNUInstallDirs)
install(TARGETS mmdMotionBinding
    EXPORT mmdMotionBindingTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mmdMotionBindingConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/mmdMotionBindingConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mmdMotionBinding")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/mmdMotionBindingConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion)
install(EXPORT mmdMotionBindingTargets
    FILE mmdMotionBindingTargets.cmake
    NAMESPACE mmdMotionBinding::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mmdMotionBinding")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/mmdMotionBindingConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/mmdMotionBindingConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mmdMotionBinding")

if(MMDMOTIONBINDING_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
