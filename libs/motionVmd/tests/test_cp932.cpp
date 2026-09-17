// SPDX-License-Identifier: Apache-2.0
//
// CP932 decoding and encoding through the project's table: the characters MMD
// names are made of, the byte ranges where Shift-JIS differs from ASCII, the
// NEC and IBM duplicates, and every way a sequence can be malformed.
#include "motionVmd/Cp932.h"

#include "VmdEncoder.h"

#include <cassert>
#include <cstdint>
#include <string>

namespace {

using namespace motionVmd;
using namespace vmdtest;

Cp932Decoded
Decode(const std::string& bytes)
{
    return DecodeCp932(
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()));
}

void
ExpectText(const std::string& bytes, const std::string& text)
{
    const Cp932Decoded decoded = Decode(bytes);
    assert(!decoded.invalidAt);
    assert(!decoded.truncated);
    assert(decoded.text == text);
}

void
TestDecode()
{
    ExpectText("", "");
    ExpectText("Center_01", "Center_01");
    ExpectText(kCenter, kCenterText);
    ExpectText(kRightLegIk, kRightLegIkText);
    ExpectText(kBlink, kBlinkText);
    ExpectText(kLongName, kLongNameText);
    // Half-width katakana are single bytes.
    ExpectText(Cp932({0xBE, 0xDD, 0xC0, 0xB0}), "ｾﾝﾀｰ");
    // 0x5C and 0x7E are the ASCII characters Microsoft's table maps them to.
    ExpectText("\\~", "\\~");
    // A trail byte of 0x5C is part of its character, never a backslash.
    ExpectText(Cp932({0x95, 0x5C, 0x8E, 0xA6, 0x81, 0x45, 0x82, 0x68, 0x82, 0x6A}), "表示・ＩＫ");
    // NEC row 13 and the IBM extensions.
    ExpectText(Cp932({0x87, 0x40}), "①");
    ExpectText(Cp932({0xEE, 0xE0}), "髙");
    ExpectText(Cp932({0x81, 0x60}), "～");
    // A NUL is a character here; cutting at it is the reader's rule.
    ExpectText(std::string("a\0b", 3), std::string("a\0b", 3));
}

void
TestMalformed()
{
    // A lead byte with nothing after it: dropped, and said so.
    Cp932Decoded decoded = Decode(kCenter + Cp932({0x83}));
    assert(decoded.truncated);
    assert(!decoded.invalidAt);
    assert(decoded.text == kCenterText);

    // A trail byte below 0x40, at 0x7F-range edges, and above 0xFC.
    for (unsigned trail : {0x00u, 0x3Fu, 0xFDu, 0xFFu}) {
        decoded = Decode(Cp932({0x41, 0x83, trail, 0x42}));
        assert(decoded.invalidAt == 1u);
        assert(decoded.text.empty());
        assert(!decoded.truncated);
    }
    // In range, but a pair CP932 does not map.
    decoded = Decode(Cp932({0x85, 0x40}));
    assert(decoded.invalidAt == 0u);
    assert(decoded.text.empty());
}

void
TestEncode()
{
    assert(EncodeCp932("") == std::string());
    assert(EncodeCp932("Center_01") == std::string("Center_01"));
    assert(EncodeCp932(kCenterText) == kCenter);
    assert(EncodeCp932(kRightLegIkText) == kRightLegIk);
    assert(EncodeCp932(kLongNameText) == kLongName);
    assert(EncodeCp932("ｾﾝﾀｰ") == Cp932({0xBE, 0xDD, 0xC0, 0xB0}));
    assert(EncodeCp932("①") == Cp932({0x87, 0x40}));
    // Where several sequences decode to one character, the encoder writes the
    // one Microsoft's does: JIS X 0208 over NEC row 13, and NEC's selection of
    // the IBM extensions over IBM's own.
    assert(Decode(Cp932({0x87, 0x90})).text == "≒");
    assert(EncodeCp932("≒") == Cp932({0x81, 0xE0}));
    assert(Decode(Cp932({0xFB, 0xFC})).text == "髙");
    assert(EncodeCp932("髙") == Cp932({0xEE, 0xE0}));

    // Not in CP932, outside the BMP, and not UTF-8 at all.
    assert(!EncodeCp932("é"));
    assert(!EncodeCp932("😀"));
    assert(!EncodeCp932("\xE3\x81"));
    assert(!EncodeCp932("\xC0\x80"));
    assert(!EncodeCp932("\xED\xA0\x80"));
}

void
TestEveryDecodedCharacterEncodes()
{
    // Whatever the table decodes, the table can encode -- perhaps as the other
    // byte sequence of a duplicate, which then decodes to the same text.
    std::size_t characters = 0;
    for (unsigned lead = 0x81; lead <= 0xFC; ++lead) {
        if (lead > 0x9F && lead < 0xE0) {
            continue;
        }
        for (unsigned trail = 0x40; trail <= 0xFC; ++trail) {
            const Cp932Decoded decoded = Decode(Cp932({lead, trail}));
            if (decoded.invalidAt) {
                continue;
            }
            ++characters;
            const auto encoded = EncodeCp932(decoded.text);
            assert(encoded.has_value());
            assert(Decode(*encoded).text == decoded.text);
        }
    }
    assert(characters == 9604);
}

} // namespace

void
TestCp932()
{
    TestDecode();
    TestMalformed();
    TestEncode();
    TestEveryDecodedCharacterEncodes();
}
