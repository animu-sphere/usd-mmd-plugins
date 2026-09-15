// SPDX-License-Identifier: Apache-2.0
#include "mmdPmx/Reader.h"

#include "mmdPmx/Codes.h"

#include "ByteReader.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <string>
#include <utility>

namespace mmd::pmx {

namespace {

constexpr std::array<std::byte, 4> kSignature{
    std::byte{'P'}, std::byte{'M'}, std::byte{'X'}, std::byte{' '}};

// The eight globals PMX defines; a file may declare more (PMX_CONTRACT.md §3).
constexpr std::size_t kDefinedGlobals = 8;

// globals[2]..[7], in order, as they are named in diagnostics.
constexpr std::array<const char*, 6> kIndexSizeNames{
    "vertex", "texture", "material", "bone", "morph", "rigidBody"};

std::string
FloatText(float value)
{
    char buffer[32];
    const auto [end, ec] = std::to_chars(std::begin(buffer), std::end(buffer), value);
    return ec == std::errc() ? std::string(buffer, end) : std::string("?");
}

Location
AtByte(std::size_t offset, std::string field = {})
{
    Location location;
    location.byteOffset = offset;
    location.table = field.empty() ? std::string() : "header";
    location.field = std::move(field);
    return location;
}

}  // namespace

std::string_view
ToString(Version version)
{
    return version == Version::V2_1 ? "2.1" : "2.0";
}

Result<Document>
Read(std::span<const std::byte> bytes)
{
    detail::ByteReader in(bytes);
    std::vector<Diagnostic> diagnostics;

    // A failed read does not advance, so the offset is where the field starts.
    const auto truncated = [&](const char* field) {
        return Result<Document>::Failure(
            MakeDiagnostic(codes::PmxTruncatedBuffer,
                "the file ends inside the header (" + std::to_string(bytes.size())
                    + " bytes)",
                AtByte(in.offset(), field)),
            std::move(diagnostics));
    };

    // Signature. A non-empty file too short to hold it, whose bytes still match
    // "PMX ", is a truncated PMX; anything else -- an empty file included -- is
    // not a PMX at all.
    const std::size_t available = std::min(bytes.size(), kSignature.size());
    if (bytes.empty()
        || !std::equal(bytes.begin(), bytes.begin() + available, kSignature.begin())) {
        return Result<Document>::Failure(
            MakeDiagnostic(codes::PmxBadSignature,
                "the file does not start with the PMX signature \"PMX \"",
                AtByte(0, "signature")),
            std::move(diagnostics));
    }
    if (!in.Bytes(kSignature.size())) {
        return truncated("signature");
    }

    // Version: exactly 2.0 or 2.1, both exactly representable in binary32.
    const std::size_t versionOffset = in.offset();
    const std::optional<float> version = in.F32();
    if (!version) {
        return truncated("version");
    }
    Document document;
    if (*version == 2.0f) {
        document.header.version = Version::V2_0;
    } else if (*version == 2.1f) {
        document.header.version = Version::V2_1;
    } else {
        return Result<Document>::Failure(
            MakeDiagnostic(codes::PmxUnsupportedVersion,
                "PMX version " + FloatText(*version) + " is not 2.0 or 2.1",
                AtByte(versionOffset, "version")),
            std::move(diagnostics));
    }

    // Globals.
    const std::size_t countOffset = in.offset();
    const std::optional<std::uint8_t> count = in.U8();
    if (!count) {
        return truncated("globalsCount");
    }
    if (*count < kDefinedGlobals) {
        return Result<Document>::Failure(
            MakeDiagnostic(codes::PmxInvalidGlobals,
                "the header declares " + std::to_string(*count)
                    + " globals; PMX defines 8",
                AtByte(countOffset, "globalsCount")),
            std::move(diagnostics));
    }
    const std::size_t globalsOffset = in.offset();
    const auto globalBytes = in.Bytes(*count);
    if (!globalBytes) {
        return truncated("globals");
    }
    const auto global = [&](std::size_t i) {
        return std::to_integer<std::uint8_t>((*globalBytes)[i]);
    };
    const auto globalField = [](std::size_t i) {
        return "globals[" + std::to_string(i) + "]";
    };

    Globals& globals = document.header.globals;

    if (global(0) > 1) {
        return Result<Document>::Failure(
            MakeDiagnostic(codes::TextInvalidEncodingFlag,
                "text encoding flag " + std::to_string(global(0))
                    + " is neither 0 (UTF-16LE) nor 1 (UTF-8)",
                AtByte(globalsOffset, globalField(0))),
            std::move(diagnostics));
    }
    globals.textEncoding = static_cast<TextEncoding>(global(0));

    if (global(1) > 4) {
        return Result<Document>::Failure(
            MakeDiagnostic(codes::PmxInvalidGlobals,
                "additional vec4 count " + std::to_string(global(1))
                    + " is outside 0-4",
                AtByte(globalsOffset + 1, globalField(1))),
            std::move(diagnostics));
    }
    globals.additionalVec4Count = global(1);

    std::uint8_t* const indexSizes[] = {
        &globals.vertexIndexSize,   &globals.textureIndexSize,
        &globals.materialIndexSize, &globals.boneIndexSize,
        &globals.morphIndexSize,    &globals.rigidBodyIndexSize,
    };
    for (std::size_t k = 0; k < kIndexSizeNames.size(); ++k) {
        const std::size_t i = 2 + k;
        const std::uint8_t size = global(i);
        if (size != 1 && size != 2 && size != 4) {
            return Result<Document>::Failure(
                MakeDiagnostic(codes::PmxInvalidIndexSize,
                    std::string(kIndexSizeNames[k]) + " index size "
                        + std::to_string(size) + " is not 1, 2 or 4",
                    AtByte(globalsOffset + i, globalField(i))),
                std::move(diagnostics));
        }
        *indexSizes[k] = size;
    }

    if (*count > kDefinedGlobals) {
        for (std::size_t i = kDefinedGlobals; i < *count; ++i) {
            globals.unknown.push_back(global(i));
        }
        diagnostics.push_back(MakeDiagnostic(codes::PmxUnknownGlobals,
            "the header declares " + std::to_string(*count)
                + " globals; those beyond the 8 PMX defines are preserved "
                  "and ignored",
            AtByte(globalsOffset + kDefinedGlobals, globalField(kDefinedGlobals))));
    }

    return Result<Document>::Success(std::move(document), std::move(diagnostics));
}

}  // namespace mmd::pmx
