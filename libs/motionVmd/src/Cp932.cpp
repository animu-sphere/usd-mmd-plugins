// SPDX-License-Identifier: Apache-2.0
#include "motionVmd/Cp932.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace motionVmd {

namespace {

#include "Cp932Table.inc"

/// The row of kDoubleByte a lead byte starts, or nothing for any other byte.
std::optional<std::size_t>
LeadRow(std::uint8_t lead)
{
    if (lead >= 0x81 && lead <= 0x9F) {
        return lead - 0x81;
    }
    if (lead >= 0xE0 && lead <= 0xFC) {
        return (0x9F - 0x81 + 1) + (lead - 0xE0);
    }
    return std::nullopt;
}

void
AppendUtf8(std::string& out, std::uint32_t cp)
{
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        // The table holds BMP code points only, and no surrogate.
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

/// The next code point of valid UTF-8 at `i`, advancing it; nothing when the
/// bytes there are not well-formed UTF-8.
std::optional<std::uint32_t>
NextCodePoint(std::string_view s, std::size_t& i)
{
    const auto byte = [&](std::size_t k) { return static_cast<std::uint8_t>(s[k]); };
    const std::uint8_t b0 = byte(i);
    std::size_t length = 0;
    std::uint32_t cp = 0;
    std::uint8_t lo = 0x80;
    std::uint8_t hi = 0xBF;
    if (b0 < 0x80) {
        ++i;
        return b0;
    } else if (b0 >= 0xC2 && b0 <= 0xDF) {
        length = 2;
        cp = b0 & 0x1F;
    } else if (b0 >= 0xE0 && b0 <= 0xEF) {
        length = 3;
        cp = b0 & 0x0F;
        lo = b0 == 0xE0 ? 0xA0 : 0x80;
        hi = b0 == 0xED ? 0x9F : 0xBF;
    } else if (b0 >= 0xF0 && b0 <= 0xF4) {
        length = 4;
        cp = b0 & 0x07;
        lo = b0 == 0xF0 ? 0x90 : 0x80;
        hi = b0 == 0xF4 ? 0x8F : 0xBF;
    } else {
        return std::nullopt;
    }
    if (s.size() - i < length) {
        return std::nullopt;
    }
    for (std::size_t k = 1; k < length; ++k) {
        const std::uint8_t b = byte(i + k);
        if (b < (k == 1 ? lo : 0x80) || b > (k == 1 ? hi : 0xBF)) {
            return std::nullopt;
        }
        cp = (cp << 6) | (b & 0x3F);
    }
    i += length;
    return cp;
}

} // namespace

Cp932Decoded
DecodeCp932(std::span<const std::byte> bytes)
{
    Cp932Decoded out;
    std::size_t i = 0;
    while (i < bytes.size()) {
        const auto b = std::to_integer<std::uint8_t>(bytes[i]);
        const std::uint16_t single = kSingleByte[b];
        if (single != kLeadByte) {
            AppendUtf8(out.text, single);
            ++i;
            continue;
        }
        if (i + 1 == bytes.size()) {
            out.truncated = true;
            break;
        }
        const auto trail = std::to_integer<std::uint8_t>(bytes[i + 1]);
        std::uint16_t cp = 0;
        if (trail >= kTrailFirst && trail <= kTrailLast) {
            cp = kDoubleByte[*LeadRow(b) * kTrailCount + (trail - kTrailFirst)];
        }
        if (cp == 0) {
            out.text.clear();
            out.truncated = false;
            out.invalidAt = i;
            return out;
        }
        AppendUtf8(out.text, cp);
        i += 2;
    }
    return out;
}

std::optional<std::string>
EncodeCp932(std::string_view utf8)
{
    std::string out;
    std::size_t i = 0;
    while (i < utf8.size()) {
        const auto cp = NextCodePoint(utf8, i);
        if (!cp || *cp > 0xFFFF) {
            return std::nullopt;
        }
        const auto it = std::lower_bound(
            kEncode.begin(), kEncode.end(), *cp, [](const EncodeEntry& entry, std::uint32_t value) {
                return entry.codePoint < value;
            });
        if (it == kEncode.end() || it->codePoint != *cp) {
            return std::nullopt;
        }
        if (it->bytes > 0xFF) {
            out += static_cast<char>(it->bytes >> 8);
        }
        out += static_cast<char>(it->bytes & 0xFF);
    }
    return out;
}

} // namespace motionVmd
