// SPDX-License-Identifier: Apache-2.0
//
// The libFuzzer target for the VMD reader: any bytes in, and the reader must
// return -- never crash, never read outside the span, never report a fatal
// diagnostic as recoverable -- and any document it returns, and the motion
// built from it, must keep the promises VmdInvariants.h checks. Built only
// with -DMOTIONVMD_BUILD_FUZZER=ON under Clang;
// .github/workflows/parser-sanitizers.yml runs it under ASan and UBSan.
#include "motionVmd/Motion.h"
#include "motionVmd/Reader.h"

#include "VmdInvariants.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>

namespace {

void
Check(const std::string& violation)
{
    if (!violation.empty()) {
        std::fprintf(stderr, "an invariant is broken: %s\n", violation.c_str());
        std::abort();
    }
}

} // namespace

extern "C" int
LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    const auto result =
        motionVmd::Read(std::span<const std::byte>(reinterpret_cast<const std::byte*>(data), size));
    for (const motionVmd::Diagnostic& d : result.diagnostics()) {
        if (!d.recoverable || d.severity == motionVmd::Severity::Fatal) {
            std::fprintf(stderr,
                         "a fatal diagnostic among the recoverable ones: %s\n",
                         motionVmd::FormatDiagnostic(d).c_str());
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
    Check(vmdtest::CheckInvariants(result.value()));
    const auto motion = motionVmd::BuildMotion(result.value());
    Check(vmdtest::CheckInvariants(motion.value()));
    return 0;
}
