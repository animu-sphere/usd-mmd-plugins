# SPDX-License-Identifier: Apache-2.0

add_executable(mmdSkeletonAdapter_tests test_adapter.cpp)
target_link_libraries(mmdSkeletonAdapter_tests PRIVATE
    mmdSkeletonAdapter::mmdSkeletonAdapter)
usdmmd_target_defaults(mmdSkeletonAdapter_tests)
target_compile_options(mmdSkeletonAdapter_tests
    PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
add_test(NAME mmdSkeletonAdapter_unit COMMAND mmdSkeletonAdapter_tests)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mmdSkeletonAdapter_link_$<CONFIG>.txt"
    CONTENT "LINK_LIBRARIES=$<TARGET_PROPERTY:mmdSkeletonAdapter,LINK_LIBRARIES>
INTERFACE_LINK_LIBRARIES=$<TARGET_PROPERTY:mmdSkeletonAdapter,INTERFACE_LINK_LIBRARIES>
")

if(NOT DEFINED USDMMD_TEST_PYTHON)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        set(USDMMD_TEST_PYTHON "${Python3_EXECUTABLE}")
    endif()
endif()
if(USDMMD_TEST_PYTHON)
    add_test(NAME mmdSkeletonAdapter_boundaries
        COMMAND "${USDMMD_TEST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../scripts/check_library_boundaries.py"
            --name mmdSkeletonAdapter
            --source "${CMAKE_CURRENT_SOURCE_DIR}/.."
            --link-file "${CMAKE_CURRENT_BINARY_DIR}/mmdSkeletonAdapter_link_$<CONFIG>.txt"
            --binary "$<TARGET_FILE:mmdSkeletonAdapter_tests>"
            --allow mmdModel::mmdModel
            --allow motionRetarget::motionRetarget
            --allow-openusd-foundation)
endif()
