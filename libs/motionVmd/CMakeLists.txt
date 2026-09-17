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
# Self-contained, so `ost library build libs/motionVmd` can configure it
# directly; also add_subdirectory()'d by the repository root.
cmake_minimum_required(VERSION 3.22)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION"
         _mmd_vmd_version LIMIT_COUNT 1)
    string(STRIP "${_mmd_vmd_version}" _mmd_vmd_version)
else()
    set(_mmd_vmd_version "0.0.0")  # mirrors openstrata.library.yaml
endif()

project(motionVmd
    VERSION ${_mmd_vmd_version}
    DESCRIPTION "VMD motion reader for usd-mmd-plugins"
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

if(NOT DEFINED MOTIONVMD_BUILD_TESTS)
    if(DEFINED USDMMD_BUILD_TESTS)
        set(_mmd_vmd_tests_default "${USDMMD_BUILD_TESTS}")
    else()
        set(_mmd_vmd_tests_default "${PROJECT_IS_TOP_LEVEL}")
    endif()
    option(MOTIONVMD_BUILD_TESTS "Build motionVmd tests" ${_mmd_vmd_tests_default})
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdTargets.cmake")

# Sanitizers and the fuzz target, for the lane that builds this library on its
# own (.github/workflows/parser-sanitizers.yml), exactly as mmdPmx declares
# them: a VMD reader takes untrusted bytes too.
set(MOTIONVMD_SANITIZERS "" CACHE STRING
    "Sanitizers for motionVmd and its tests, e.g. \"address;undefined\" (Clang or GCC)")
option(MOTIONVMD_BUILD_FUZZER "Build the motionVmd_fuzz libFuzzer target (Clang only)" OFF)
if(MOTIONVMD_SANITIZERS)
    list(JOIN MOTIONVMD_SANITIZERS "," _mmd_vmd_sanitize)
    add_compile_options(-fsanitize=${_mmd_vmd_sanitize} -fno-sanitize-recover=all
                        -fno-omit-frame-pointer)
    add_link_options(-fsanitize=${_mmd_vmd_sanitize})
endif()
if(MOTIONVMD_BUILD_FUZZER)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "MOTIONVMD_BUILD_FUZZER needs Clang's libFuzzer")
    endif()
    if(NOT MOTIONVMD_SANITIZERS)
        message(FATAL_ERROR
            "MOTIONVMD_BUILD_FUZZER needs MOTIONVMD_SANITIZERS (at least \"address\")")
    endif()
    add_compile_options(-fsanitize=fuzzer-no-link)
endif()

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

include(GNUInstallDirs)
install(TARGETS motionVmd
    EXPORT motionVmdTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/motionVmdConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/motionVmdConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/motionVmd")
# Pre-1.0, a minor version may break the API, so a consumer's request is
# compatible only within the same major.minor.
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/motionVmdConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion)
install(EXPORT motionVmdTargets
    FILE motionVmdTargets.cmake
    NAMESPACE motionVmd::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/motionVmd")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/motionVmdConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/motionVmdConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/motionVmd")

if(MOTIONVMD_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
if(MOTIONVMD_BUILD_FUZZER)
    add_subdirectory(fuzz)
endif()
