# SPDX-License-Identifier: Apache-2.0
#
# Per-target settings every component of the workspace applies to its own
# targets. Included by the root project and by each component, so a component
# built standalone (`ost plugin build`, `ost library build`, or plain CMake on
# its directory) compiles exactly as it does in the composed tree.
#
# Every setting is a PRIVATE property of the target it is applied to: nothing
# here is directory-scoped (add_compile_options, add_link_options), so a
# composed build -- this workspace, or a consumer that add_subdirectory()s a
# component -- sees no flag it did not ask for.
#
# Nothing here resolves a dependency. The workspace has no third-party
# dependency (docs/architecture/DEPENDENCIES.md), and OpenUSD is resolved by
# the components that may link it, through cmake/UsdMmdOpenUsd.cmake.
include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/UsdMmdSanitizers.cmake")

# usdmmd_target_defaults(<target>)
#
# Windows: `/utf-8`, so source literals and narrow strings are UTF-8 whatever
# the host code page is, and `NOMINMAX`, so <windows.h> -- which OpenUSD pulls
# in -- does not define min/max macros over std::min/std::max
# (docs/design/TEXT_ENCODING_POLICY.md §4). Both are PRIVATE: they govern how
# this workspace compiles, and are never imposed on a consumer of an installed
# package.
#
# GCC and Clang: `-ffp-contract=off`, so `a * b + c` is never fused into one
# FMA instruction. Clang fuses by default wherever the target has FMA (arm64
# does, baseline x86-64 does not), and a fused result can round differently --
# the same bytes would then author a different stage on macOS than on Linux,
# and one golden could not serve both (DESIGN_POLICY.md §2.5). MSVC's default
# /fp:precise does not contract.
#
# And the sanitizer instrumentation USDMMD_SANITIZERS and USDMMD_BUILD_FUZZERS
# ask for (UsdMmdSanitizers.cmake); none by default.
function(usdmmd_target_defaults target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8)
        target_compile_definitions(${target} PRIVATE NOMINMAX)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -ffp-contract=off)
    endif()
    usdmmd_target_sanitizers(${target})
endfunction()

# usdmmd_use_utf8_code_page(<target>)
#
# Makes a Windows executable's process code page UTF-8, so the paths it is
# handed arrive in the encoding OpenUSD and this workspace read them in. A
# Windows `main(int, char**)` otherwise receives its arguments in the ANSI code
# page -- CP932 on a Japanese host, CP1252 on a CI runner. The manifest sets
# `activeCodePage` (Windows 10 1903 and later), which makes `argv`, `getenv`
# and every narrow file API in the process UTF-8 at once.
#
# It is a property of the executable only. A plugin is loaded by a host whose
# code page is not ours, and reads its files through Ar for that reason
# (TEXT_ENCODING_POLICY.md §4); that is why the Unicode-path test drives the
# importer from a Python host, where no manifest can mask a regression.
function(usdmmd_use_utf8_code_page target)
    # The MSVC linker merges a `.manifest` source into the embedded manifest.
    if(MSVC)
        target_sources(${target} PRIVATE
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/utf8-code-page.manifest")
    endif()
endfunction()
