// SPDX-License-Identifier: Apache-2.0
//
// The Phase 0 header reader against bytes built here, so every case states
// exactly which byte it is about.
#include "mmdPmx/Reader.h"

#include <bit>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<std::byte>;

void
Append(Bytes& out, std::initializer_list<int> values)
{
    for (int v : values) {
        out.push_back(static_cast<std::byte>(v));
    }
}

void
AppendF32(Bytes& out, float value)
{
    const auto bits = std::bit_cast<std::uint32_t>(value);
    for (int shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<std::byte>((bits >> shift) & 0xFF));
    }
}

/// signature, version, globals -- the whole Phase 0 header.
Bytes
Header(float version = 2.0f, std::vector<int> globals = {0, 0, 4, 1, 1, 2, 2, 1})
{
    Bytes out;
    Append(out, {'P', 'M', 'X', ' '});
    AppendF32(out, version);
    out.push_back(static_cast<std::byte>(globals.size()));
    for (int g : globals) {
        out.push_back(static_cast<std::byte>(g));
    }
    return out;
}

mmd::Result<mmd::pmx::Document>
ReadBytes(const Bytes& bytes)
{
    return mmd::pmx::Read(std::span<const std::byte>(bytes.data(), bytes.size()));
}

void
ExpectFatal(const Bytes& bytes, const std::string& code, std::uint64_t offset)
{
    const auto result = ReadBytes(bytes);
    assert(!result.ok());
    assert(result.fatal() != nullptr);
    if (result.fatal()->code != code) {
        std::fprintf(stderr, "expected %s, got %s\n", code.c_str(),
            mmd::FormatDiagnostic(*result.fatal()).c_str());
    }
    assert(result.fatal()->code == code);
    assert(!result.fatal()->recoverable);
    assert(result.fatal()->location.byteOffset == offset);
}

void
TestValidHeaders()
{
    const auto v20 = ReadBytes(Header(2.0f, {0, 0, 4, 1, 1, 2, 2, 1}));
    assert(v20.ok());
    assert(v20.diagnostics().empty());
    const mmd::pmx::Header& h = v20.value().header;
    assert(h.version == mmd::pmx::Version::V2_0);
    assert(mmd::pmx::ToString(h.version) == "2.0");
    assert(h.globals.textEncoding == mmd::pmx::TextEncoding::Utf16Le);
    assert(h.globals.additionalVec4Count == 0);
    assert(h.globals.vertexIndexSize == 4);
    assert(h.globals.textureIndexSize == 1);
    assert(h.globals.materialIndexSize == 1);
    assert(h.globals.boneIndexSize == 2);
    assert(h.globals.morphIndexSize == 2);
    assert(h.globals.rigidBodyIndexSize == 1);
    assert(h.globals.unknown.empty());

    const auto v21 = ReadBytes(Header(2.1f, {1, 4, 1, 1, 1, 1, 1, 1}));
    assert(v21.ok());
    assert(v21.value().header.version == mmd::pmx::Version::V2_1);
    assert(mmd::pmx::ToString(v21.value().header.version) == "2.1");
    assert(v21.value().header.globals.textEncoding == mmd::pmx::TextEncoding::Utf8);
    assert(v21.value().header.globals.additionalVec4Count == 4);

    // Phase 0 reads the header only: what follows is not examined.
    Bytes withTail = Header();
    Append(withTail, {0xDE, 0xAD});
    assert(ReadBytes(withTail).ok());
}

void
TestSignature()
{
    ExpectFatal({}, "MMD_PMX_BAD_SIGNATURE", 0);
    Bytes pmd;
    Append(pmd, {'P', 'm', 'd', 0});
    ExpectFatal(pmd, "MMD_PMX_BAD_SIGNATURE", 0);
    Bytes lower = Header();
    lower[1] = std::byte{'m'};
    ExpectFatal(lower, "MMD_PMX_BAD_SIGNATURE", 0);

    // A prefix of the signature is a truncated PMX, not a foreign file.
    Bytes prefix;
    Append(prefix, {'P', 'M'});
    ExpectFatal(prefix, "MMD_PMX_TRUNCATED_BUFFER", 0);
}

void
TestVersion()
{
    ExpectFatal(Header(1.0f), "MMD_PMX_UNSUPPORTED_VERSION", 4);
    ExpectFatal(Header(2.2f), "MMD_PMX_UNSUPPORTED_VERSION", 4);
    // Compared exactly: the float nearest 2.1 is legal, its neighbour is not.
    ExpectFatal(Header(std::nextafter(2.1f, 3.0f)), "MMD_PMX_UNSUPPORTED_VERSION", 4);

    Bytes shortVersion = Header();
    shortVersion.resize(6);
    ExpectFatal(shortVersion, "MMD_PMX_TRUNCATED_BUFFER", 4);
}

void
TestGlobals()
{
    Bytes noCount = Header();
    noCount.resize(8);
    ExpectFatal(noCount, "MMD_PMX_TRUNCATED_BUFFER", 8);

    ExpectFatal(Header(2.0f, {0, 0, 1, 1, 1, 1, 1}), "MMD_PMX_INVALID_GLOBALS", 8);

    Bytes shortGlobals = Header();
    shortGlobals.resize(12);
    ExpectFatal(shortGlobals, "MMD_PMX_TRUNCATED_BUFFER", 9);

    ExpectFatal(Header(2.0f, {2, 0, 1, 1, 1, 1, 1, 1}),
        "MMD_TEXT_INVALID_ENCODING_FLAG", 9);
    ExpectFatal(Header(2.0f, {0, 5, 1, 1, 1, 1, 1, 1}), "MMD_PMX_INVALID_GLOBALS", 10);

    // Each index width, in its own byte.
    for (std::size_t i = 2; i < 8; ++i) {
        for (int bad : {0, 3, 8}) {
            std::vector<int> globals{0, 0, 1, 1, 1, 1, 1, 1};
            globals[i] = bad;
            ExpectFatal(Header(2.0f, globals), "MMD_PMX_INVALID_INDEX_SIZE", 9 + i);
        }
    }
    const auto located = ReadBytes(Header(2.0f, {0, 0, 1, 1, 1, 3, 1, 1}));
    assert(located.fatal()->location.field == "globals[5]");
}

void
TestUnknownGlobals()
{
    const auto result = ReadBytes(Header(2.0f, {0, 0, 1, 1, 1, 1, 1, 1, 7, 9}));
    assert(result.ok());
    assert(result.diagnostics().size() == 1);
    const mmd::Diagnostic& d = result.diagnostics()[0];
    assert(d.code == "MMD_PMX_UNKNOWN_GLOBALS");
    assert(d.severity == mmd::Severity::Warning);
    assert(d.recoverable);
    assert(d.location.byteOffset == 17);
    assert((result.value().header.globals.unknown == std::vector<std::uint8_t>{7, 9}));
}

}  // namespace

void
TestReader()
{
    TestValidHeaders();
    TestSignature();
    TestVersion();
    TestGlobals();
    TestUnknownGlobals();
}
