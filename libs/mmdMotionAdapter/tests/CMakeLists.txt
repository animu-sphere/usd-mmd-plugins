# SPDX-License-Identifier: Apache-2.0

add_executable(mmdMotionAdapter_tests test_adapter.cpp)
target_link_libraries(mmdMotionAdapter_tests PRIVATE mmdMotionAdapter::mmdMotionAdapter)
usdmmd_target_defaults(mmdMotionAdapter_tests)
target_compile_options(mmdMotionAdapter_tests
    PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
add_test(NAME mmdMotionAdapter_unit COMMAND mmdMotionAdapter_tests)

if(NOT TARGET motionUsd::motionUsd)
    find_package(motionUsd 0.5 CONFIG REQUIRED)
endif()

add_executable(mmdMotionAdapter_acceptance
    test_acceptance.cpp
    generic_retarget.cpp)
target_link_libraries(mmdMotionAdapter_acceptance PRIVATE
    mmdMotionAdapter::mmdMotionAdapter
    motionRetarget::motionRetarget
    motionUsd::motionUsd)
usdmmd_target_defaults(mmdMotionAdapter_acceptance)
target_compile_options(mmdMotionAdapter_acceptance
    PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
add_test(NAME mmdMotionAdapter_acceptance COMMAND mmdMotionAdapter_acceptance)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mmdMotionAdapter_link_$<CONFIG>.txt"
    CONTENT "LINK_LIBRARIES=$<TARGET_PROPERTY:mmdMotionAdapter,LINK_LIBRARIES>
INTERFACE_LINK_LIBRARIES=$<TARGET_PROPERTY:mmdMotionAdapter,INTERFACE_LINK_LIBRARIES>
")

if(NOT DEFINED USDMMD_TEST_PYTHON)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        set(USDMMD_TEST_PYTHON "${Python3_EXECUTABLE}")
    endif()
endif()
if(USDMMD_TEST_PYTHON)
    add_test(NAME mmdMotionAdapter_boundaries
        COMMAND "${USDMMD_TEST_PYTHON}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../scripts/check_library_boundaries.py"
            --name mmdMotionAdapter
            --source "${CMAKE_CURRENT_SOURCE_DIR}/.."
            --link-file "${CMAKE_CURRENT_BINARY_DIR}/mmdMotionAdapter_link_$<CONFIG>.txt"
            --binary "$<TARGET_FILE:mmdMotionAdapter_tests>"
            --allow mmdControl::mmdControl
            --allow mmdModel::mmdModel
            --allow mmdSkeletonAdapter::mmdSkeletonAdapter
            --allow motionCore::motionCore
            --allow-openusd-foundation)
endif()
