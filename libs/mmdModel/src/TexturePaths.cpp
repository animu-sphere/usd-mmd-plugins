// SPDX-License-Identifier: Apache-2.0
#include "TexturePaths.h"

#include <vector>

namespace mmd::detail {

namespace {

bool
IsAsciiLetter(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

/// `scheme:` at the start (RFC 3986: a letter, then letters, digits, `+`, `-`
/// or `.`). A drive letter, `C:`, is the one-letter case.
bool
StartsWithScheme(std::string_view path)
{
    if (path.empty() || !IsAsciiLetter(path[0])) {
        return false;
    }
    for (std::size_t i = 1; i < path.size(); ++i) {
        const char c = path[i];
        if (c == ':') {
            return true;
        }
        if (!IsAsciiLetter(c) && !(c >= '0' && c <= '9') && c != '+' && c != '-' && c != '.') {
            return false;
        }
    }
    return false;
}

/// A control character: C0 (U+0000-U+001F), DEL (U+007F) or C1
/// (U+0080-U+009F). No portable filename holds one, and SdfAssetPath refuses
/// a path that does -- it would author `@@`. The text is valid UTF-8, so C1 is
/// exactly a 0xC2 lead byte followed by 0x80-0x9F.
bool
HasControlCharacter(std::string_view text)
{
    for (std::size_t i = 0; i < text.size(); ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c < 0x20 || c == 0x7F) {
            return true;
        }
        if (c == 0xC2 && i + 1 < text.size()) {
            const auto next = static_cast<unsigned char>(text[i + 1]);
            if (next >= 0x80 && next <= 0x9F) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

TexturePath
NormalizeTexturePath(std::string_view source)
{
    TexturePath result;
    // Trailing U+0000 is padding some writers add; any other control
    // character, U+0000 included, is not one a filename can hold.
    std::string_view text = source;
    while (!text.empty() && text.back() == '\0') {
        text.remove_suffix(1);
    }
    if (HasControlCharacter(text)) {
        result.unsafe = true;
        return result;
    }
    std::string path(text);
    for (char& c : path) {
        if (c == '\\') {
            c = '/';
        }
    }
    // Absolute and UNC (`\\server\share` is `//server/share` by now), then a
    // drive or a URI scheme.
    if (!path.empty() && path[0] == '/') {
        result.unsafe = true;
        return result;
    }
    if (StartsWithScheme(path)) {
        result.unsafe = true;
        return result;
    }

    std::vector<std::string_view> segments;
    std::string_view rest = path;
    while (!rest.empty()) {
        const std::size_t slash = rest.find('/');
        const std::string_view segment = rest.substr(0, slash);
        rest = slash == std::string_view::npos ? std::string_view() : rest.substr(slash + 1);
        if (segment.empty() || segment == ".") {
            continue;
        }
        if (segment == ".." && !segments.empty() && segments.back() != "..") {
            segments.pop_back();
            continue;
        }
        segments.push_back(segment);
    }
    if (!segments.empty() && segments.front() == "..") {
        result.unsafe = true; // it escapes the model's directory (TEXT-O1)
        return result;
    }
    if (segments.empty()) {
        return result;
    }
    result.assetPath = ".";
    for (std::string_view segment : segments) {
        result.assetPath += '/';
        result.assetPath += segment;
    }
    return result;
}

} // namespace mmd::detail
