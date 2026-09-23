# SPDX-License-Identifier: Apache-2.0
#
# UsdMmdProject.cmake -- what every project in the workspace declares the same
# way: its version, its language level, its build type and its tests option.
#
# Included by the root project and by each component BEFORE project(), since
# the version is an argument of project():
#
#   include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/UsdMmdProject.cmake")
#   project(mmdModel VERSION ${USDMMD_VERSION} ... LANGUAGES CXX)
#   usdmmd_component(TESTS_OPTION MMDMODEL_BUILD_TESTS)
#
# project() itself stays in each CMakeLists.txt -- CMake requires the call to
# be literal there -- and so do add_library(), target_link_libraries() and
# every dependency a component declares (docs/architecture/WORKSPACE.md §5).
# Nothing here resolves a dependency.

# The single product version: the repository-root VERSION file
# (WORKSPACE.md §4). CHANGELOG.md, the git tag and every manifest mirror it;
# no CMakeLists.txt restates the number. Recomputed on every include, so a
# component reads it whether or not a parent already did.
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../VERSION" USDMMD_VERSION LIMIT_COUNT 1)
string(STRIP "${USDMMD_VERSION}" USDMMD_VERSION)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/../VERSION")

include_guard(GLOBAL)

# usdmmd_component([TESTS_OPTION <var>])
#
# Called right after project(). A macro, so what it sets lands in the calling
# project's scope.
#
#   * C++20, unless the caller already chose a standard: the parser's public
#     API takes std::span (DEPENDENCIES.md §2). Each library also states
#     `cxx_std_20` as a usage requirement, which is what a consumer sees.
#   * Release when a single-config generator (Ninja) is given no build type:
#     the OpenUSD installs this builds against are Release-only, and an empty
#     build type would pull debug-only imported dependencies.
#   * TESTS_OPTION declares the component's own tests switch. It defaults to
#     USDMMD_BUILD_TESTS when the component is composed by the root project
#     (or when the caller passes it), else to whether the component is the
#     top-level project -- so a standalone configure builds its tests, and a
#     consumer that add_subdirectory()s it does not.
macro(usdmmd_component)
    cmake_parse_arguments(_usdmmd_component "" "TESTS_OPTION" "" ${ARGN})

    if(NOT CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 20)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    get_property(_usdmmd_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if(NOT _usdmmd_multi_config AND NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    endif()

    if(_usdmmd_component_TESTS_OPTION)
        if(DEFINED USDMMD_BUILD_TESTS)
            set(_usdmmd_tests_default "${USDMMD_BUILD_TESTS}")
        else()
            set(_usdmmd_tests_default "${PROJECT_IS_TOP_LEVEL}")
        endif()
        option(${_usdmmd_component_TESTS_OPTION}
               "Build ${PROJECT_NAME} tests" ${_usdmmd_tests_default})
        unset(_usdmmd_tests_default)
    endif()

    unset(_usdmmd_multi_config)
    unset(_usdmmd_component_TESTS_OPTION)
    unset(_usdmmd_component_UNPARSED_ARGUMENTS)
    unset(_usdmmd_component_KEYWORDS_MISSING_VALUES)
endmacro()
