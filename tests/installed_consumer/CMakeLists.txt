# SPDX-License-Identifier: Apache-2.0
#
# A consumer of the installed packages, and nothing else. It is never
# add_subdirectory()'d: scripts/check_installed_consumer.py copies this
# directory out of the repository and configures it against a clean install
# prefix, so a dependency on the source tree, the build tree, a relative
# include path or an undeclared sibling fails here (DESIGN_POLICY.md §13).
cmake_minimum_required(VERSION 3.22)

project(UsdMmdInstalledConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# mmdModel alone: its package config must find mmdPmx, its one edge, by
# itself (PACKAGE_CONTRACT.md).
find_package(mmdModel 0.1 CONFIG REQUIRED)

add_executable(pmx_header_probe main.cpp)
target_link_libraries(pmx_header_probe PRIVATE mmdModel::mmdModel)
if(MSVC)
    target_compile_options(pmx_header_probe PRIVATE /utf-8)
endif()

# mmdMotionBinding alone: its package config must find both of its edges,
# mmdModel and motionVmd, by itself.
find_package(mmdMotionBinding 0.1 CONFIG REQUIRED)

add_executable(vmd_probe vmd_probe.cpp)
target_link_libraries(vmd_probe PRIVATE mmdMotionBinding::mmdMotionBinding)
if(MSVC)
    target_compile_options(vmd_probe PRIVATE /utf-8)
endif()

# mmdControl alone: its package config must find mmdMotionBinding and
# mmdModel, and through them motionVmd and mmdPmx, by itself.
find_package(mmdControl 0.1 CONFIG REQUIRED)

add_executable(control_probe control_probe.cpp)
target_link_libraries(control_probe PRIVATE mmdControl::mmdControl)
if(MSVC)
    target_compile_options(control_probe PRIVATE /utf-8)
endif()

# The Phase 9 edge: both adapters must resolve their digest-pinned shared
# motion dependencies from installed packages, never sibling source trees.
find_package(mmdSkeletonAdapter 0.1 CONFIG REQUIRED)
find_package(mmdMotionAdapter 0.1 CONFIG REQUIRED)

add_executable(adapter_probe adapter_probe.cpp)
target_link_libraries(adapter_probe PRIVATE mmdMotionAdapter::mmdMotionAdapter)
if(MSVC)
    target_compile_options(adapter_probe PRIVATE /utf-8)
endif()

# The Phase 8 schema bundle: its package config must find OpenUSD by itself,
# and its generated headers must compile from the prefix alone.
find_package(mmdSchema 0.1 CONFIG REQUIRED)

add_executable(schema_probe schema_probe.cpp)
target_link_libraries(schema_probe PRIVATE mmdSchema::mmdSchema)
if(MSVC)
    target_compile_options(schema_probe PRIVATE /utf-8)
    target_compile_definitions(schema_probe PRIVATE NOMINMAX)
endif()
