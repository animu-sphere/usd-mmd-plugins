// SPDX-License-Identifier: Apache-2.0
//
// Malformed input never crashes the reader, never reads past the buffer, and
// never yields a document or motion that breaks the promises VmdInvariants.h
// checks. Every byte of the sample motions is overwritten with values chosen
// to hit the interesting cases, and every prefix is read. The fuzz target
// (libs/motionVmd/fuzz/) explores the same property without a fixed list.
#include "motionVmd/Motion.h"
#include "motionVmd/Reader.h"

#include "VmdEncoder.h"
#include "VmdInvariants.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using namespace vmdtest;
using namespace motionVmd;

std::size_t g_reads = 0;
std::size_t g_documents = 0;

void
Fail(const std::string& violation)
{
    if (!violation.empty()) {
        std::fprintf(stderr, "a mutated file broke an invariant: %s\n", violation.c_str());
    }
    assert(violation.empty());
}

void
ReadAndCheck(const Bytes& bytes)
{
    const std::vector<std::byte> exact(bytes.begin(), bytes.end());
    const auto result = Read(std::span<const std::byte>(exact.data(), exact.size()));
    ++g_reads;
    for (const Diagnostic& d : result.diagnostics()) {
        assert(d.recoverable);
    }
    if (!result.ok()) {
        assert(result.fatal()->severity == Severity::Fatal);
        return;
    }
    ++g_documents;
    Fail(CheckInvariants(result.value()));
    const auto motion = BuildMotion(result.value());
    assert(motion.ok());
    Fail(CheckInvariants(motion.value()));
}

void
MutateEveryByte(const Bytes& original)
{
    constexpr std::uint8_t kValues[] = {0x00, 0xFF, 0x7F, 0x80, 0x81, 0x01, 0xFE};
    Bytes bytes = original;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const std::byte kept = bytes[i];
        for (std::uint8_t value : kValues) {
            bytes[i] = static_cast<std::byte>(value);
            ReadAndCheck(bytes);
        }
        bytes[i] = kept ^ std::byte{0x01};
        ReadAndCheck(bytes);
        bytes[i] = kept;
    }
}

void
TruncateAndExtend(const Bytes& original)
{
    for (std::size_t n = 0; n <= original.size(); ++n) {
        ReadAndCheck(Bytes(original.begin(), original.begin() + static_cast<std::ptrdiff_t>(n)));
    }
    Bytes longer = original;
    for (int i = 0; i < 64; ++i) {
        longer.push_back(static_cast<std::byte>(0xFF - i));
        ReadAndCheck(longer);
    }
}

} // namespace

int
main()
{
    Document v1 = SampleDocument();
    v1.header.version = Version::V1;
    for (const Document& doc : {SampleDocument(), v1}) {
        const Bytes bytes = Encode(doc);
        MutateEveryByte(bytes);
        TruncateAndExtend(bytes);
    }
    assert(g_documents > 0);
    std::printf("robustness: %zu reads, %zu of them documents\n", g_reads, g_documents);
    std::puts("motionVmd robustness tests passed");
    return 0;
}
