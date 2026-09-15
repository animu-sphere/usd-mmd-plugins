// SPDX-License-Identifier: Apache-2.0
//
// pmx::Document: a PMX file as the file states it -- source facts in the
// source's own basis and units, never USD policy
// (docs/design/DESIGN_POLICY.md §5.1, docs/design/PMX_CONTRACT.md §1).
//
// Phase 0 scaffold: the document holds the header only. The tables arrive
// with the Phase 1 parser.
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace mmd::pmx {

/// The two versions PMX defines. Compared exactly on read (PMX_CONTRACT.md §3).
enum class Version : std::uint8_t {
    V2_0,
    V2_1,
};

/// "2.0" or "2.1".
std::string_view ToString(Version version);

/// globals[0]: the encoding of every string in the file.
enum class TextEncoding : std::uint8_t {
    Utf16Le = 0,
    Utf8 = 1,
};

/// The header's globals (PMX_CONTRACT.md §3), validated.
struct Globals {
    TextEncoding textEncoding = TextEncoding::Utf16Le;
    std::uint8_t additionalVec4Count = 0;  ///< 0-4
    // Index widths in bytes: 1, 2 or 4 each (PMX_CONTRACT.md §4).
    std::uint8_t vertexIndexSize = 1;
    std::uint8_t textureIndexSize = 1;
    std::uint8_t materialIndexSize = 1;
    std::uint8_t boneIndexSize = 1;
    std::uint8_t morphIndexSize = 1;
    std::uint8_t rigidBodyIndexSize = 1;
    /// Bytes beyond the eight PMX defines, preserved as read.
    std::vector<std::uint8_t> unknown;
};

struct Header {
    Version version = Version::V2_0;
    Globals globals;
};

struct Document {
    Header header;
};

}  // namespace mmd::pmx
