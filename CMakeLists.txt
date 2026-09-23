# SPDX-License-Identifier: Apache-2.0
#
# Top-level project for usd-mmd-plugins: the dependency graph of the
# workspace, and nothing else. Each component declares its own dependencies
# and its own install; this file says in which order they are composed, and
# where OpenUSD enters.
#
# One CMake contract, three ways to drive it (docs/architecture/WORKSPACE.md
# §5):
#   * Plain CMake, this file: CMAKE_PREFIX_PATH names the dependency prefix --
#     OpenUSD 26.08 and usd-motion-plugins' packages, installed:
#       cmake -S . -B build -DCMAKE_PREFIX_PATH=<dependency-prefix>
#       cmake --build build --config Release
#       ctest --test-dir build -C Release
#   * Plain CMake, one component: `cmake -S libs/<component>` against the same
#     prefix plus the installed packages of its in-repository edges.
#   * OpenStrata: `ost build`, `ost plugin build plugins/usdMmdFileFormat`,
#     `ost library build libs/<component>` compose that prefix and configure
#     these same files; nothing here knows it.
#
# Inside this repository an edge is the in-tree target when it exists and the
# installed package otherwise (`if(NOT TARGET ...) find_package(...)`);
# outside it, always an installed package. No sibling checkout is ever
# add_subdirectory()'d.
cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/UsdMmdProject.cmake")
project(UsdMmdPlugins
    VERSION ${USDMMD_VERSION}
    DESCRIPTION "OpenUSD plugins for MikuMikuDance assets"
    LANGUAGES CXX)
usdmmd_component(TESTS_OPTION USDMMD_BUILD_TESTS)

if(USDMMD_BUILD_TESTS)
    # At the top level, so tests registered by components are discoverable by
    # `ctest` from this build tree.
    enable_testing()
endif()

# Pure MMD libraries: the PMX syntax parser, the canonical model, the VMD
# reader, motion binding and control evaluation, and the tools that report on
# a PMX and a VMD. They link no OpenUSD and nothing outside this repository
# (WORKSPACE.md §2), so they are added before OpenUSD is resolved. Their
# position is the statement: nothing they configure can see pxr.
add_subdirectory("libs/mmdPmx")
add_subdirectory("libs/mmdModel")
add_subdirectory("libs/motionVmd")
add_subdirectory("libs/mmdMotionBinding")
add_subdirectory("libs/mmdControl")
add_subdirectory("tools/mmdInspect")
add_subdirectory("tools/vmdInspect")

# OpenUSD, resolved once for everything below and held to the pin.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/UsdMmdOpenUsd.cmake")
usdmmd_find_openusd()

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

# Ecosystem adapters: the only libraries that cross into usd-motion-plugins
# (WORKSPACE.md §2.4). Each finds its motion package itself; they follow
# OpenUSD so those packages reuse the foundation targets resolved above.
add_subdirectory("libs/mmdSkeletonAdapter")
add_subdirectory("libs/mmdMotionAdapter")

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
