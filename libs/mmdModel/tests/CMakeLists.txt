# SPDX-License-Identifier: Apache-2.0

include("${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/UsdMmdTesting.cmake")

# The suites see the library's private headers, to test identifiers, texture
# paths and the joint order directly. Canonicalization takes a pmx::Document,
# so the tests state their documents as data: no PMX bytes are involved.
function(_mmd_model_test target)
    usdmmd_test_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE mmdModel::mmdModel)
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../src"
        "${CMAKE_CURRENT_SOURCE_DIR}")
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

# WORKSPACE.md §2.1 allows mmdModel one edge, mmdPmx.
usdmmd_add_boundary_test(mmdModel TARGET mmdModel BINARY mmdModel_tests
    ARGS --allow mmdPmx::mmdPmx)
