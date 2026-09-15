# SPDX-License-Identifier: Apache-2.0
#
# mmdModel -- canonical MMD semantics (docs/design/DESIGN_POLICY.md §5.2): the
# one place a PMX becomes USD-basis, meter-scaled, identified and ordered data.
#
# A plain static library whose one edge is mmdPmx, and which has NO OpenUSD
# dependency (docs/architecture/WORKSPACE.md §2): this file never resolves pxr,
# and the tests below fail if anything it links, or anything linking it
# inherits, reaches OpenUSD.
#
# Self-contained, so `ost library build libs/mmdModel` can configure it
# against an installed mmdPmx; also add_subdirectory()'d by the repository
# root, which already has the in-tree mmdPmx target.
cmake_minimum_required(VERSION 3.22)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../VERSION"
         _mmd_model_version LIMIT_COUNT 1)
    string(STRIP "${_mmd_model_version}" _mmd_model_version)
else()
    set(_mmd_model_version "0.0.0")  # mirrors openstrata.library.yaml
endif()

project(mmdModel
    VERSION ${_mmd_model_version}
    DESCRIPTION "Canonical MMD semantics for usd-mmd-plugins"
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

if(NOT DEFINED MMDMODEL_BUILD_TESTS)
    if(DEFINED USDMMD_BUILD_TESTS)
        set(_mmd_model_tests_default "${USDMMD_BUILD_TESTS}")
    else()
        set(_mmd_model_tests_default "${PROJECT_IS_TOP_LEVEL}")
    endif()
    option(MMDMODEL_BUILD_TESTS "Build mmdModel tests" ${_mmd_model_tests_default})
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdTargets.cmake")

# Sanitizers, for the lane that builds this library on its own against an
# instrumented mmdPmx: .github/workflows/parser-sanitizers.yml. Canonicalization
# runs on every document the parser accepts, malformed-but-recoverable ones
# included, so it is held to the parser's bar (DESIGN_POLICY.md §13).
set(MMDMODEL_SANITIZERS "" CACHE STRING
    "Sanitizers for mmdModel and its tests, e.g. \"address;undefined\" (Clang or GCC)")
if(MMDMODEL_SANITIZERS)
    list(JOIN MMDMODEL_SANITIZERS "," _mmd_model_sanitize)
    add_compile_options(-fsanitize=${_mmd_model_sanitize} -fno-sanitize-recover=all
                        -fno-omit-frame-pointer)
    add_link_options(-fsanitize=${_mmd_model_sanitize})
endif()

# The declared mmdModel -> mmdPmx edge (WORKSPACE.md §2.1). In the composed
# workspace build mmdPmx is already an in-tree target; a standalone configure
# resolves its installed package.
if(NOT TARGET mmdPmx::mmdPmx)
    find_package(mmdPmx CONFIG REQUIRED)
endif()

add_library(mmdModel STATIC
    src/Basis.cpp
    src/Canonicalize.cpp
    src/Identifiers.cpp
    src/Skeleton.cpp
    src/TexturePaths.cpp)
add_library(mmdModel::mmdModel ALIAS mmdModel)
set_target_properties(mmdModel PROPERTIES
    EXPORT_NAME mmdModel
    # Linked into the shared file-format plugin, so it must be relocatable.
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(mmdModel PUBLIC cxx_std_20)
target_include_directories(mmdModel
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
# PUBLIC: the canonical document carries mmdPmx's diagnostic record and
# Result<T>, so a consumer of mmdModel's headers includes mmdPmx's.
target_link_libraries(mmdModel PUBLIC mmdPmx::mmdPmx)
usdmmd_target_defaults(mmdModel)

include(GNUInstallDirs)
install(TARGETS mmdModel
    EXPORT mmdModelTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mmdModelConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/mmdModelConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mmdModel")
# Pre-1.0, a minor version may break the API, so a consumer's request is
# compatible only within the same major.minor.
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/mmdModelConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion)
install(EXPORT mmdModelTargets
    FILE mmdModelTargets.cmake
    NAMESPACE mmdModel::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mmdModel")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/mmdModelConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/mmdModelConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/mmdModel")

if(MMDMODEL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
