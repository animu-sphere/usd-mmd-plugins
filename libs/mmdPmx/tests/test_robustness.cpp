// SPDX-License-Identifier: Apache-2.0
//
// Malformed input never crashes the reader, never reads past the buffer, and
// never yields a document that breaks the invariants Document.h promises.
// Every byte of a sample model is overwritten with values chosen to hit the
// interesting cases -- zero, all ones, sign bits, an off-by-one -- and read
// back. The fuzz target (libs/mmdPmx/fuzz/) explores the same property without
// a fixed list; this suite runs it deterministically, in every build, and
// under the sanitizers wherever they are enabled.
#include "mmdPmx/Reader.h"

#include "PmxEncoder.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using namespace pmxtest;
using namespace mmd::pmx;

std::size_t g_reads = 0;
std::size_t g_documents = 0;

void
ReadAndCheck(const Bytes& bytes)
{
    // A copy the exact size of the input, so an over-read is outside any
    // allocation the sanitizers know about.
    const std::vector<std::byte> exact(bytes.begin(), bytes.end());
    const auto result = Read(std::span<const std::byte>(exact.data(), exact.size()));
    ++g_reads;
    if (result.ok()) {
        ++g_documents;
        const std::string violation = CheckInvariants(result.value());
        if (!violation.empty()) {
            std::fprintf(stderr,
                         "a mutated file read as a document that breaks an invariant: %s\n",
                         violation.c_str());
        }
        assert(violation.empty());
    } else {
        assert(result.fatal()->severity == mmd::Severity::Fatal);
    }
    for (const mmd::Diagnostic& d : result.diagnostics()) {
        assert(d.recoverable);
    }
}

void
MutateEveryByte(const Bytes& original)
{
    constexpr std::uint8_t kValues[] = {0x00, 0xFF, 0x7F, 0x80, 0x01, 0xFE};
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

void
TestRobustness()
{
    for (Version version : {Version::V2_0, Version::V2_1}) {
        for (TextEncoding encoding : {TextEncoding::Utf16Le, TextEncoding::Utf8}) {
            for (std::uint8_t width : {1, 4}) {
                const Bytes bytes = Encode(SampleDocument(version, encoding, width, 1));
                MutateEveryByte(bytes);
                TruncateAndExtend(bytes);
            }
        }
    }
    // The documents a mutation leaves readable are the interesting ones: the
    // suite is only meaningful if some exist.
    assert(g_documents > 0);
    std::printf("robustness: %zu reads, %zu of them documents\n", g_reads, g_documents);
}
