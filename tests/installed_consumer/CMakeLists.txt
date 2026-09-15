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
find_package(mmdModel 0.0 CONFIG REQUIRED)

add_executable(pmx_header_probe main.cpp)
target_link_libraries(pmx_header_probe PRIVATE mmdModel::mmdModel)
if(MSVC)
    target_compile_options(pmx_header_probe PRIVATE /utf-8)
endif()
