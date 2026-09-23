# SPDX-License-Identifier: Apache-2.0
#
# UsdMmdTesting.cmake -- how a component registers its tests
# (docs/architecture/WORKSPACE.md §6). Included by a component's tests/
# directory; add_executable(), target_link_libraries() and add_test() stay in
# the component, so what a suite links is read where it is declared.
include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/UsdMmdTargets.cmake")

# usdmmd_test_python()
#
# Sets USDMMD_TEST_PYTHON in the caller's scope, unless it is already defined:
# the root project defines it after OpenUSD is resolved, to the interpreter
# pxrConfig.cmake names, and a component configured on its own finds any
# Python 3. Left undefined when there is none, and the caller then registers
# no Python-driven test.
macro(usdmmd_test_python)
    if(NOT DEFINED USDMMD_TEST_PYTHON)
        find_package(Python3 COMPONENTS Interpreter QUIET)
        if(Python3_Interpreter_FOUND)
            set(USDMMD_TEST_PYTHON "${Python3_EXECUTABLE}")
        endif()
    endif()
endmacro()

# usdmmd_test_executable(<target> <source>...)
#
# A suite: a plain executable that checks with `assert()`, which NDEBUG
# compiles away -- and Release is the default build type -- so NDEBUG is
# undefined for it. It compiles as the component's own targets do
# (usdmmd_target_defaults). The caller links it.
function(usdmmd_test_executable target)
    add_executable(${target} ${ARGN})
    usdmmd_target_defaults(${target})
    target_compile_options(${target}
        PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
endfunction()

# usdmmd_openusd_root(<out-var>)
#
# The OpenUSD install root, from where pxrConfig.cmake was found: an
# install's root holds pxrConfig.cmake itself, a relocated runtime's holds it
# under lib/cmake/pxr. Empty when OpenUSD was not resolved.
function(usdmmd_openusd_root out_var)
    set(_root "")
    if(pxr_DIR AND EXISTS "${pxr_DIR}/bin")
        set(_root "${pxr_DIR}")
    elseif(pxr_DIR)
        get_filename_component(_root "${pxr_DIR}/../../.." ABSOLUTE)
    endif()
    set(${out_var} "${_root}" PARENT_SCOPE)
endfunction()

# usdmmd_openusd_test_environment(<test>...)
#
# Puts OpenUSD's shared libraries on PATH for tests whose executables load
# them -- the adapters reach the foundation libraries through the shared
# motion packages. An `ost` session sets this itself; a plain-CMake build
# does not, and on Windows a DLL is found through PATH alone.
function(usdmmd_openusd_test_environment)
    usdmmd_openusd_root(_root)
    if(_root)
        set_property(TEST ${ARGN} APPEND PROPERTY ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${_root}/bin"
            "PATH=path_list_prepend:${_root}/lib")
    endif()
endfunction()

# usdmmd_add_boundary_test(<name>
#     TARGET <target>        the library or executable whose edges are gated
#     BINARY <target>        an executable that links it and nothing else
#     [ARGS <arg>...])       --allow, --forbid-include, --allow-openusd-foundation
#
# Registers <name>_boundaries: scripts/check_library_boundaries.py over the
# component's sources (the parent of the calling tests/ directory), over
# <target>'s link line exactly as CMake resolved it, and over what BINARY
# imports (WORKSPACE.md §2.3). The link line is LINK_LIBRARIES, and for a
# library also INTERFACE_LINK_LIBRARIES.
function(usdmmd_add_boundary_test name)
    cmake_parse_arguments(PARSE_ARGV 1 _arg "" "TARGET;BINARY" "ARGS")
    foreach(_required TARGET BINARY)
        if(NOT _arg_${_required})
            message(FATAL_ERROR "usdmmd_add_boundary_test(${name}): ${_required} is required")
        endif()
    endforeach()

    usdmmd_test_python()
    if(NOT USDMMD_TEST_PYTHON)
        message(WARNING
            "${name}: no Python 3 interpreter; the boundary check is not registered")
        return()
    endif()

    set(_link_file "${CMAKE_CURRENT_BINARY_DIR}/${name}_link_$<CONFIG>.txt")
    get_target_property(_type ${_arg_TARGET} TYPE)
    if(_type STREQUAL "EXECUTABLE")
        file(GENERATE OUTPUT "${_link_file}"
            CONTENT "LINK_LIBRARIES=$<TARGET_PROPERTY:${_arg_TARGET},LINK_LIBRARIES>
")
    else()
        file(GENERATE OUTPUT "${_link_file}"
            CONTENT "LINK_LIBRARIES=$<TARGET_PROPERTY:${_arg_TARGET},LINK_LIBRARIES>
INTERFACE_LINK_LIBRARIES=$<TARGET_PROPERTY:${_arg_TARGET},INTERFACE_LINK_LIBRARIES>
")
    endif()

    add_test(NAME ${name}_boundaries
        COMMAND "${USDMMD_TEST_PYTHON}"
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../scripts/check_library_boundaries.py"
            --name ${name}
            --source "${CMAKE_CURRENT_SOURCE_DIR}/.."
            --link-file "${_link_file}"
            --binary "$<TARGET_FILE:${_arg_BINARY}>"
            ${_arg_ARGS})
endfunction()
