# SPDX-License-Identifier: Apache-2.0
#
# motionVmd_fuzz: the libFuzzer target (fuzz_read.cpp). Clang only; the library
# itself is instrumented with -fsanitize=fuzzer-no-link by the parent file.

add_executable(motionVmd_fuzz fuzz_read.cpp)
target_link_libraries(motionVmd_fuzz PRIVATE motionVmd::motionVmd)
target_include_directories(motionVmd_fuzz PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../tests")
target_compile_options(motionVmd_fuzz PRIVATE -fsanitize=fuzzer)
target_link_options(motionVmd_fuzz PRIVATE -fsanitize=fuzzer)
