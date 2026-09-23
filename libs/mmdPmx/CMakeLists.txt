# SPDX-License-Identifier: Apache-2.0
#
# mmdPmx -- the PMX syntax parser (docs/design/DESIGN_POLICY.md §5.1).
#
# A plain static library with NO dependency at all -- no OpenUSD, nothing in
# this repository (docs/architecture/WORKSPACE.md §2): this file never
# resolves pxr, and the tests below fail if anything it links, or anything
# linking it inherits, reaches OpenUSD.
#
# Self-contained, so `ost library build libs/mmdPmx` or plain CMake can
# configure it directly; also add_subdirectory()'d by the repository root.
cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdProject.cmake")
project(mmdPmx
    VERSION ${USDMMD_VERSION}
    DESCRIPTION "PMX syntax parser for usd-mmd-plugins"
    LANGUAGES CXX)
usdmmd_component(TESTS_OPTION MMDPMX_BUILD_TESTS)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdTargets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdPackage.cmake")

add_library(mmdPmx STATIC
    src/Diagnostic.cpp
    src/Document.cpp
    src/Reader.cpp
    src/Text.cpp)
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

usdmmd_install_library(mmdPmx)

if(MMDPMX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
# The libFuzzer target (DESIGN_POLICY.md §13), for the sanitizer lane:
# .github/workflows/parser-sanitizers.yml.
if(USDMMD_BUILD_FUZZERS)
    add_subdirectory(fuzz)
endif()
