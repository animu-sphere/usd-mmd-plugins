# SPDX-License-Identifier: Apache-2.0
#
# motionVmd_fuzz: the libFuzzer target (fuzz_read.cpp). Clang only; the library
# itself is instrumented with -fsanitize=fuzzer-no-link by
# usdmmd_target_defaults() (cmake/UsdMmdSanitizers.cmake).

add_executable(motionVmd_fuzz fuzz_read.cpp)
target_link_libraries(motionVmd_fuzz PRIVATE motionVmd::motionVmd)
target_include_directories(motionVmd_fuzz PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../tests")
usdmmd_target_defaults(motionVmd_fuzz)
target_compile_options(motionVmd_fuzz PRIVATE -fsanitize=fuzzer)
target_link_options(motionVmd_fuzz PRIVATE -fsanitize=fuzzer)
