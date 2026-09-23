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
# Self-contained, so `ost library build libs/mmdModel` or plain CMake can
# configure it against an installed mmdPmx; also add_subdirectory()'d by the
# repository root, which already has the in-tree mmdPmx target.
cmake_minimum_required(VERSION 3.22)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdProject.cmake")
project(mmdModel
    VERSION ${USDMMD_VERSION}
    DESCRIPTION "Canonical MMD semantics for usd-mmd-plugins"
    LANGUAGES CXX)
usdmmd_component(TESTS_OPTION MMDMODEL_BUILD_TESTS)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdTargets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdPackage.cmake")

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

usdmmd_install_library(mmdModel)

if(MMDMODEL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
