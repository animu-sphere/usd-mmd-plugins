# SPDX-License-Identifier: Apache-2.0
#
# Writes buildInfo.json at BUILD time, as a `cmake -P` script run by the
# bundle's UsdMmdFileFormat_buildInfo target on every build.
#
# The git commit is the one value that changes without a reconfigure: a
# configure-time stamp kept naming the commit the tree was first configured
# at, however many commits later it was built and installed. Reading HEAD here
# keeps it current. configure_file() rewrites the output only when its content
# changes, so an unchanged tree leaves the file -- and the package digest --
# untouched.
#
# Every other value is fixed at configure time and passed in with -D; the
# bundle's CMakeLists.txt makes the files they come from (VERSION, the
# authorer's header) configure dependencies.
#
# Inputs: TEMPLATE, OUTPUT, SOURCE_DIR, GIT_EXECUTABLE (may be empty), and the
# @-variables TEMPLATE names.
foreach(_required TEMPLATE OUTPUT SOURCE_DIR)
    if(NOT DEFINED ${_required})
        message(FATAL_ERROR "WriteBuildInfo.cmake: ${_required} is not set")
    endif()
endforeach()

set(_mmd_git_commit "unknown")
if(GIT_EXECUTABLE)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" rev-parse HEAD
        OUTPUT_VARIABLE _mmd_git_head
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _mmd_git_rc)
    if(_mmd_git_rc EQUAL 0)
        set(_mmd_git_commit "${_mmd_git_head}")
    endif()
endif()

configure_file("${TEMPLATE}" "${OUTPUT}" @ONLY)
