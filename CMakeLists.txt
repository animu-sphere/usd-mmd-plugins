# SPDX-License-Identifier: Apache-2.0
#
# Top-level project for usd-mmd-plugins.
#
# The workspace builds two ways, and both are kept working
# (docs/architecture/WORKSPACE.md §5):
#   * OpenStrata: `ost plugin build|test plugins/usdMmdFileFormat` drives the
#     bundle's own CMakeLists.txt against the activated runtime's toolchain, and
#     `ost plugin test --workspace` configures this file.
#   * Plain CMake: configure this file with CMAKE_PREFIX_PATH pointing at an
#     OpenUSD 26.08 install, or use CMakePresets.json:
#       cmake -S . -B build -DCMAKE_PREFIX_PATH=<usd-install>
#       cmake --build build --config Release
#       ctest --test-dir build -C Release
#
# Every component is a self-contained CMake project (so `ost` can build it
# standalone) that is also add_subdirectory()-able from here.
cmake_minimum_required(VERSION 3.22)

# The single product version: the repository-root VERSION file. CHANGELOG.md,
# the git tag and every manifest mirror it; nothing restates the number here.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" _mmd_project_version LIMIT_COUNT 1)
string(STRIP "${_mmd_project_version}" _mmd_project_version)

project(UsdMmdPlugins
    VERSION ${_mmd_project_version}
    DESCRIPTION "OpenUSD plugins for MikuMikuDance assets"
    LANGUAGES CXX)

# C++20: the parser's public API takes std::span (DEPENDENCIES.md §2).
if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Single-config generators (Ninja) default to an empty build type. The OpenUSD
# installs this builds against are Release-only, so default to Release rather
# than pull debug-only imported dependencies.
get_property(_isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _isMultiConfig AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(USDMMD_BUILD_TESTS "Build usd-mmd-plugins tests" ${PROJECT_IS_TOP_LEVEL})

if(USDMMD_BUILD_TESTS)
    # At the top level, so tests registered by components are discoverable by
    # `ctest` from this build tree.
    enable_testing()
endif()

# The PMX syntax parser, the canonical model, the VMD reader and the tools that
# report on a PMX and a VMD link no OpenUSD (WORKSPACE.md §2), so they are
# added before the one workspace-wide OpenUSD resolution below. Their position
# is the statement: nothing they configure can see pxr.
add_subdirectory("libs/mmdPmx")
add_subdirectory("libs/mmdModel")
add_subdirectory("libs/motionVmd")
add_subdirectory("libs/mmdMotionBinding")
add_subdirectory("libs/mmdControl")
add_subdirectory("tools/mmdInspect")
add_subdirectory("tools/vmdInspect")

# Resolve OpenUSD once for the whole project; bundles reuse the result.
find_package(pxr REQUIRED CONFIG)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/UsdMmdOpenUsd.cmake")

# The interpreter the integration tests import OpenUSD's Python bindings into.
# It must be the Python OpenUSD was built against, and pxrConfig.cmake names
# that one -- it sets Python3_EXECUTABLE unless it is already defined -- so the
# interpreter is found here, after it, and never before in this scope: a newer
# Python found first is "already defined", and the bindings then fail to
# import ("Module use of python313.dll conflicts with this version").
if(USDMMD_BUILD_TESTS)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        set(USDMMD_TEST_PYTHON "${Python3_EXECUTABLE}")
    endif()
endif()

# Every plugin bundle: an immediate plugins/<name>/ carrying both an
# openstrata.plugin.yaml and a CMakeLists.txt. Named rather than globbed, so
# adding a bundle is a visible edit here, in openstrata.toml, and in
# WORKSPACE.md §1 -- in that order.
add_subdirectory("plugins/usdMmdFileFormat")

# Workspace-level tests: the fixtures, and what spans components
# (WORKSPACE.md §6).
if(USDMMD_BUILD_TESTS)
    add_subdirectory("tests")
endif()

install(FILES
    LICENSE
    README.md
    THIRD_PARTY_NOTICES.md
    DESTINATION "share/doc/usd-mmd-plugins")
