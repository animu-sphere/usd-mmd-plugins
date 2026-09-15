# SPDX-License-Identifier: Apache-2.0
#
# mmdPmx -- the PMX syntax parser (docs/design/DESIGN_POLICY.md §5.1).
#
# A plain static library with NO OpenUSD dependency (docs/architecture/
# WORKSPACE.md §2): this file never resolves pxr, and the tests below fail if
# anything it links, or anything linking it inherits, reaches OpenUSD.
#
# Self-contained, so `ost library build libs/mmdPmx` can configure it directly;
# also add_subdirectory()'d by the repository root.
cmake_minimum_required(VERSION 3.22)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION"
         _mmd_pmx_version LIMIT_COUNT 1)
    string(STRIP "${_mmd_pmx_version}" _mmd_pmx_version)
else()
    set(_mmd_pmx_version "0.0.0")  # mirrors openstrata.library.yaml
endif()

project(mmdPmx
    VERSION ${_mmd_pmx_version}
    DESCRIPTION "PMX syntax parser for usd-mmd-plugins"
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

if(NOT DEFINED MMDPMX_BUILD_TESTS)
    if(DEFINED USDMMD_BUILD_TESTS)
        set(_mmd_pmx_tests_default "${USDMMD_BUILD_TESTS}")
    else()
        set(_mmd_pmx_tests_default "${PROJECT_IS_TOP_LEVEL}")
    endif()
    option(MMDPMX_BUILD_TESTS "Build mmdPmx tests" ${_mmd_pmx_tests_default})
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdTargets.cmake")

add_library(mmdPmx STATIC
    src/Diagnostic.cpp
    src/Reader.cpp)
add_library(mmdPmx::mmdPmx ALIAS mmdPmx)
set_target_properties(mmdPmx PROPERTIES
    EXPORT_NAME mmdPmx
    # Linked into the shared file-format plugin, so it must be relocatable.
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(mmdPmx PUBLIC cxx_std_20)
target_include_directories(mmdPmx
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
usdmmd_target_defaults(mmdPmx)

include(GNUInstallDirs)
install(TARGETS mmdPmx
    EXPORT mmdPmxTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mmdPmxConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/mmdPmxConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mmdPmx")
# Pre-1.0, a minor version may break the API, so a consumer's request is
# compatible only within the same major.minor.
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/mmdPmxConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion)
install(EXPORT mmdPmxTargets
    FILE mmdPmxTargets.cmake
    NAMESPACE mmdPmx::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mmdPmx")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/mmdPmxConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/mmdPmxConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mmdPmx")

if(MMDPMX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
