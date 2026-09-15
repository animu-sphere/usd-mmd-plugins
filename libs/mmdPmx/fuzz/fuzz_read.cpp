// SPDX-License-Identifier: Apache-2.0
//
// The libFuzzer target for the PMX reader (DESIGN_POLICY.md §13): any bytes
// in, and the reader must return -- never crash, never read outside the span,
// never report a fatal diagnostic as recoverable -- and any document it
// returns must keep the invariants Document.h promises. Built only with
// -DMMDPMX_BUILD_FUZZER=ON under Clang; .github/workflows/parser-sanitizers.yml
// runs it with AddressSanitizer and UndefinedBehaviorSanitizer, seeded with
// the generated fixtures.
#include "mmdPmx/Reader.h"

#include "PmxEncoder.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>

extern "C" int
LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    const auto result =
        mmd::pmx::Read(std::span<const std::byte>(reinterpret_cast<const std::byte*>(data), size));
    for (const mmd::Diagnostic& d : result.diagnostics()) {
        if (!d.recoverable || d.severity == mmd::Severity::Fatal) {
            std::fprintf(stderr, "a fatal diagnostic among the recoverable ones: %s\n",
                mmd::FormatDiagnostic(d).c_str());
            std::abort();
        }
    }
    if (!result.ok()) {
        if (result.fatal() == nullptr || result.fatal()->recoverable) {
            std::fprintf(stderr, "a failure without a fatal diagnostic\n");
            std::abort();
        }
        return 0;
    }
    const std::string violation = pmxtest::CheckInvariants(result.value());
    if (!violation.empty()) {
        std::fprintf(stderr, "a document that breaks an invariant: %s\n", violation.c_str());
        std::abort();
    }
    return 0;
}
