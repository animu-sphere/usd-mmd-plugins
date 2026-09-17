// SPDX-License-Identifier: Apache-2.0
#include "Text.h"

#include <cstdint>

namespace mmd::pmx::detail {

namespace {

std::uint8_t
At(std::span<const std::byte> bytes, std::size_t i)
{
    return std::to_integer<std::uint8_t>(bytes[i]);
}

void
AppendUtf8(std::string& out, std::uint32_t cp)
{
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

std::optional<DecodeError>
Fail(std::string& out, std::size_t offset, const char* reason)
{
    out.clear();
    return DecodeError{offset, reason};
}

} // namespace

std::string
ReplaceInvalidUtf8(std::string_view text)
{
    const std::span<const std::byte> bytes(reinterpret_cast<const std::byte*>(text.data()),
                                           text.size());
    std::string out;
    std::string piece;
    std::size_t i = 0;
    while (i < bytes.size()) {
        // The shortest slice that decodes is the one well-formed sequence
        // starting at i, if there is one.
        std::size_t length = 0;
        for (std::size_t n = 1; n <= 4 && i + n <= bytes.size(); ++n) {
            if (!DecodeUtf8(bytes.subspan(i, n), piece)) {
                length = n;
                break;
            }
        }
        if (length == 0) {
            out += "\xEF\xBF\xBD"; // U+FFFD
            ++i;
        } else {
            out.append(text.substr(i, length));
            i += length;
        }
    }
    return out;
}

std::optional<DecodeError>
DecodeUtf8(std::span<const std::byte> bytes, std::string& out)
{
    out.clear();
    const std::size_t n = bytes.size();
    std::size_t i = 0;
    while (i < n) {
        const std::uint8_t lead = At(bytes, i);
        if (lead < 0x80) {
            ++i;
            continue;
        }
        // The well-formed sequences of the Unicode Standard's Table 3-7: the
        // second byte's range depends on the lead, which is what excludes
        // overlong forms, surrogates and code points above U+10FFFF.
        std::size_t length = 0;
        std::uint8_t low = 0x80;
        std::uint8_t high = 0xBF;
        if (lead >= 0xC2 && lead <= 0xDF) {
            length = 2;
        } else if (lead == 0xE0) {
            length = 3;
            low = 0xA0;
        } else if ((lead >= 0xE1 && lead <= 0xEC) || lead == 0xEE || lead == 0xEF) {
            length = 3;
        } else if (lead == 0xED) {
            length = 3;
            high = 0x9F;
        } else if (lead == 0xF0) {
            length = 4;
            low = 0x90;
        } else if (lead >= 0xF1 && lead <= 0xF3) {
            length = 4;
        } else if (lead == 0xF4) {
            length = 4;
            high = 0x8F;
        } else {
            return Fail(out,
                        i,
                        lead < 0xC2 && lead >= 0xC0 ? "an overlong two-byte form"
                        : lead < 0xC0               ? "a continuation byte where a character starts"
                                                    : "a byte that starts no UTF-8 sequence");
        }
        if (n - i < length) {
            return Fail(out, i, "a sequence truncated by the end of the string");
        }
        const std::uint8_t second = At(bytes, i + 1);
        if (second < low || second > high) {
            return Fail(out,
                        i + 1,
                        second < 0x80 || second > 0xBF ? "a missing continuation byte"
                        : lead == 0xED                 ? "an encoded surrogate"
                        : lead == 0xF4                 ? "a code point above U+10FFFF"
                                                       : "an overlong form");
        }
        for (std::size_t k = 2; k < length; ++k) {
            const std::uint8_t next = At(bytes, i + k);
            if (next < 0x80 || next > 0xBF) {
                return Fail(out, i + k, "a missing continuation byte");
            }
        }
        i += length;
    }
    out.assign(reinterpret_cast<const char*>(bytes.data()), n);
    return std::nullopt;
}

std::optional<DecodeError>
DecodeUtf16Le(std::span<const std::byte> bytes, std::string& out)
{
    out.clear();
    if (bytes.size() % 2 != 0) {
        return Fail(out, bytes.size() - 1, "an odd byte length");
    }
    out.reserve(bytes.size() + bytes.size() / 2);
    const auto unit = [&](std::size_t i) {
        return static_cast<std::uint32_t>(At(bytes, i) | (At(bytes, i + 1) << 8));
    };
    std::size_t i = 0;
    while (i < bytes.size()) {
        const std::uint32_t u = unit(i);
        if (u >= 0xDC00 && u <= 0xDFFF) {
            return Fail(out, i, "a low surrogate with no high surrogate before it");
        }
        if (u >= 0xD800 && u <= 0xDBFF) {
            if (bytes.size() - i < 4) {
                return Fail(out, i, "a high surrogate at the end of the string");
            }
            const std::uint32_t low = unit(i + 2);
            if (low < 0xDC00 || low > 0xDFFF) {
                return Fail(out, i, "a high surrogate with no low surrogate after it");
            }
            AppendUtf8(out, 0x10000 + ((u - 0xD800) << 10) + (low - 0xDC00));
            i += 4;
            continue;
        }
        AppendUtf8(out, u);
        i += 2;
    }
    return std::nullopt;
}

} // namespace mmd::pmx::detail
