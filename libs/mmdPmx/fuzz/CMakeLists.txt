# SPDX-License-Identifier: Apache-2.0
#
# mmdPmx_fuzz: the libFuzzer target (fuzz_read.cpp). Clang only; the library
# itself is instrumented with -fsanitize=fuzzer-no-link by
# usdmmd_target_defaults() (cmake/UsdMmdSanitizers.cmake), so the fuzzer sees
# coverage inside the parser and not only in this harness.

add_executable(mmdPmx_fuzz
    fuzz_read.cpp
    ../tests/PmxSamples.cpp)
target_link_libraries(mmdPmx_fuzz PRIVATE mmdPmx::mmdPmx)
target_include_directories(mmdPmx_fuzz PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../tests")
usdmmd_target_defaults(mmdPmx_fuzz)
target_compile_options(mmdPmx_fuzz PRIVATE -fsanitize=fuzzer)
target_link_options(mmdPmx_fuzz PRIVATE -fsanitize=fuzzer)
