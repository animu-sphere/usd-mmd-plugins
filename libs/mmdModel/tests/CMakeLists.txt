# SPDX-License-Identifier: Apache-2.0

# Each suite checks with `assert()`, which NDEBUG compiles away -- and Release
# is the default build type -- so NDEBUG is undefined for these targets only.
# They see the library's private headers, to test identifiers, texture paths
# and the joint order directly. Canonicalization takes a pmx::Document, so the
# tests state their documents as data: no PMX bytes are involved.
function(_mmd_model_test target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE mmdModel::mmdModel)
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../src"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    usdmmd_target_defaults(${target})
    target_compile_options(${target}
        PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
endfunction()

_mmd_model_test(mmdModel_tests
    test_basis.cpp
    test_identifiers.cpp
    test_texture_paths.cpp
    test_skeleton.cpp
    test_canonicalize.cpp
    test_main.cpp)
add_test(NAME mmdModel_unit COMMAND mmdModel_tests)

# Thousands of generated documents -- wild parents, weights, names and paths,
# within what the parser can return -- canonicalized and checked against the
# promises CanonicalDocument.h makes.
_mmd_model_test(mmdModel_robustness_tests test_robustness.cpp)
add_test(NAME mmdModel_robustness COMMAND mmdModel_robustness_tests)

# The link line exactly as CMake resolved it: WORKSPACE.md §2.1 allows
# mmdModel one edge, mmdPmx.
file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mmdModel_link_$<CONFIG>.txt"
    CONTENT "LINK_LIBRARIES=$<TARGET_PROPERTY:mmdModel,LINK_LIBRARIES>
INTERFACE_LINK_LIBRARIES=$<TARGET_PROPERTY:mmdModel,INTERFACE_LINK_LIBRARIES>
")

if(NOT DEFINED USDMMD_TEST_PYTHON)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        set(USDMMD_TEST_PYTHON "${Python3_EXECUTABLE}")
    endif()
endif()
if(USDMMD_TEST_PYTHON)
    add_test(NAME mmdModel_boundaries
        COMMAND "${USDMMD_TEST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../scripts/check_library_boundaries.py"
            --name mmdModel
            --source "${CMAKE_CURRENT_SOURCE_DIR}/.."
            --link-file "${CMAKE_CURRENT_BINARY_DIR}/mmdModel_link_$<CONFIG>.txt"
            --binary "$<TARGET_FILE:mmdModel_tests>"
            --allow mmdPmx::mmdPmx)
else()
    message(WARNING
        "mmdModel: no Python 3 interpreter; the boundary check is not registered")
endif()
