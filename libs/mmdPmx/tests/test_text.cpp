// SPDX-License-Identifier: Apache-2.0
//
// The text decoders (TEXT_ENCODING_POLICY.md §3), byte by byte.
#include "Text.h"

#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace {

using mmd::pmx::detail::DecodeUtf16Le;
using mmd::pmx::detail::DecodeUtf8;

std::vector<std::byte>
B(std::initializer_list<int> values)
{
    std::vector<std::byte> out;
    for (int v : values) {
        out.push_back(static_cast<std::byte>(v));
    }
    return out;
}

std::vector<std::byte>
B(const std::string& s)
{
    std::vector<std::byte> out;
    for (char c : s) {
        out.push_back(static_cast<std::byte>(c));
    }
    return out;
}

void
ExpectUtf8(const std::vector<std::byte>& in, bool valid, std::size_t offset = 0)
{
    std::string out = "stale";
    const auto error = DecodeUtf8(in, out);
    assert(error.has_value() == !valid);
    if (valid) {
        assert(out.size() == in.size());
    } else {
        assert(out.empty());
        assert(error->offset == offset);
    }
}

void
TestUtf8()
{
    // Valid: ASCII, Japanese, the edges of each sequence length, and
    // everything a valid string may hold -- U+0000, a BOM, CR LF.
    for (const std::string& s : {std::string(), std::string("LeftArm"),
             std::string("左腕"), std::string("ユニコード-é"), std::string("\xF0\x9F\x98\x80"),
             std::string("\x7F"), std::string("\xC2\x80"), std::string("\xDF\xBF"),
             std::string("\xE0\xA0\x80"), std::string("\xED\x9F\xBF"),
             std::string("\xEE\x80\x80"), std::string("\xEF\xBF\xBF"),
             std::string("\xF0\x90\x80\x80"), std::string("\xF4\x8F\xBF\xBF"),
             std::string("a\0b", 3), std::string("\xEF\xBB\xBFname"),
             std::string("line\r\nline")}) {
        ExpectUtf8(B(s), true);
    }
    std::string kept;
    assert(!DecodeUtf8(B(std::string("\xEF\xBB\xBF左\0", 7)), kept));
    assert(kept == std::string("\xEF\xBB\xBF左\0", 7));  // nothing trimmed

    ExpectUtf8(B({0x80}), false, 0);                 // a lone continuation byte
    ExpectUtf8(B({'a', 0xBF}), false, 1);
    ExpectUtf8(B({0xC0, 0x80}), false, 0);           // overlong U+0000
    ExpectUtf8(B({0xC1, 0xBF}), false, 0);           // overlong U+007F
    ExpectUtf8(B({0xE0, 0x80, 0x80}), false, 1);     // overlong three-byte
    ExpectUtf8(B({0xE0, 0x9F, 0xBF}), false, 1);
    ExpectUtf8(B({0xF0, 0x8F, 0xBF, 0xBF}), false, 1);  // overlong four-byte
    ExpectUtf8(B({0xED, 0xA0, 0x80}), false, 1);     // U+D800, an encoded surrogate
    ExpectUtf8(B({0xED, 0xBF, 0xBF}), false, 1);     // U+DFFF
    ExpectUtf8(B({0xF4, 0x90, 0x80, 0x80}), false, 1);  // U+110000
    ExpectUtf8(B({0xF5, 0x80, 0x80, 0x80}), false, 0);
    ExpectUtf8(B({0xFF}), false, 0);
    ExpectUtf8(B({0xE5, 0xB7}), false, 0);           // truncated by the string's end
    ExpectUtf8(B({'x', 0xF0, 0x9F, 0x98}), false, 1);
    ExpectUtf8(B({0xE5, 0x41, 0xA6}), false, 1);     // missing continuation
    ExpectUtf8(B({0xE5, 0xB7, 0x41}), false, 2);
}

void
ExpectUtf16(const std::vector<std::byte>& in, const std::string& utf8)
{
    std::string out;
    assert(!DecodeUtf16Le(in, out));
    assert(out == utf8);
}

void
ExpectBadUtf16(const std::vector<std::byte>& in, std::size_t offset)
{
    std::string out = "stale";
    const auto error = DecodeUtf16Le(in, out);
    assert(error.has_value());
    assert(out.empty());
    assert(error->offset == offset);
}

void
TestUtf16()
{
    ExpectUtf16({}, "");
    ExpectUtf16(B({'A', 0, 'r', 0, 'm', 0}), "Arm");
    ExpectUtf16(B({0xE6, 0x5D, 0x55, 0x81}), "左腕");            // U+5DE6 U+8155
    ExpectUtf16(B({0xE9, 0x00}), "é");                           // two UTF-8 bytes
    ExpectUtf16(B({0xFF, 0xFF}), "\xEF\xBF\xBF");                // U+FFFF
    ExpectUtf16(B({0x3D, 0xD8, 0x00, 0xDE}), "\xF0\x9F\x98\x80");  // a surrogate pair
    ExpectUtf16(B({0xFF, 0xDB, 0xFF, 0xDF}), "\xF4\x8F\xBF\xBF");  // U+10FFFF
    ExpectUtf16(B({0xFF, 0xFE, 'a', 0}), "\xEF\xBB\xBF" "a");     // a BOM is a character
    ExpectUtf16(B({'a', 0, 0, 0}), std::string("a\0", 2));

    ExpectBadUtf16(B({'a'}), 0);                        // odd length
    ExpectBadUtf16(B({'a', 0, 'b'}), 2);
    ExpectBadUtf16(B({0x00, 0xDC}), 0);                 // a lone low surrogate
    ExpectBadUtf16(B({'a', 0, 0x3D, 0xD8}), 2);         // a high surrogate at the end
    ExpectBadUtf16(B({0x3D, 0xD8, 'a', 0}), 0);         // a high surrogate, unpaired
    ExpectBadUtf16(B({0x3D, 0xD8, 0x3D, 0xD8, 0x00, 0xDE}), 0);
}

}  // namespace

void
TestText()
{
    TestUtf8();
    TestUtf16();
}
