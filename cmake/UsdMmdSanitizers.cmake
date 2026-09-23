# SPDX-License-Identifier: Apache-2.0
#
# UsdMmdSanitizers.cmake -- AddressSanitizer, UndefinedBehaviorSanitizer and
# libFuzzer instrumentation (docs/design/DESIGN_POLICY.md §13), for the lane
# that builds the plain libraries on their own:
# .github/workflows/parser-sanitizers.yml. Nothing shipped is built with them
# (docs/architecture/DEPENDENCIES.md §2).
#
# Two cache options, the same in every component:
#
#   USDMMD_SANITIZERS      e.g. "address;undefined" (Clang or GCC)
#   USDMMD_BUILD_FUZZERS   ON builds a component's libFuzzer target, where it
#                          has one (Clang only; needs USDMMD_SANITIZERS)
#
# They are applied per target, by usdmmd_target_defaults()
# (UsdMmdTargets.cmake): every target a component creates -- library, tests,
# fuzz harness -- is instrumented, and nothing else in a composed build is.
# Nothing is added to a target's link line, so the boundary checks see
# exactly the edges they did before.
include_guard(GLOBAL)

set(USDMMD_SANITIZERS "" CACHE STRING
    "Sanitizers for every target of the configured components, e.g. \"address;undefined\" (Clang or GCC)")
option(USDMMD_BUILD_FUZZERS
    "Build the libFuzzer targets of the configured components (Clang only)" OFF)

if(USDMMD_BUILD_FUZZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "USDMMD_BUILD_FUZZERS needs Clang's libFuzzer")
    endif()
    # The coverage hooks fuzzer-no-link compiles into every target are defined
    # by the sanitizer runtime; without one the tests do not link.
    if(NOT USDMMD_SANITIZERS)
        message(FATAL_ERROR
            "USDMMD_BUILD_FUZZERS needs USDMMD_SANITIZERS (at least \"address\")")
    endif()
endif()

# usdmmd_target_sanitizers(<target>)
#
# PRIVATE compile and link options: a static library passes neither to its
# consumers, so an installed instrumented archive is linked by a consumer that
# instruments itself (as the sanitizer lane's dependent libraries do).
function(usdmmd_target_sanitizers target)
    if(USDMMD_SANITIZERS)
        list(JOIN USDMMD_SANITIZERS "," _sanitize)
        target_compile_options(${target} PRIVATE
            -fsanitize=${_sanitize} -fno-sanitize-recover=all -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=${_sanitize})
    endif()
    if(USDMMD_BUILD_FUZZERS)
        target_compile_options(${target} PRIVATE -fsanitize=fuzzer-no-link)
    endif()
endfunction()
