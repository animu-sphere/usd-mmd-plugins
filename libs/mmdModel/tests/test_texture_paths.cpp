// SPDX-License-Identifier: Apache-2.0
//
// Texture paths against TEXT_ENCODING_POLICY.md §7.1 and §7.2.
#include "TexturePaths.h"

#include <cassert>
#include <string>

namespace {

using mmd::detail::NormalizeTexturePath;

void
ExpectPath(const std::string& source, const std::string& assetPath)
{
    const auto result = NormalizeTexturePath(source);
    assert(!result.unsafe);
    assert(result.assetPath == assetPath);
}

void
ExpectUnsafe(const std::string& source)
{
    const auto result = NormalizeTexturePath(source);
    assert(result.unsafe);
    assert(result.assetPath.empty());
}

}  // namespace

void
TestTexturePaths()
{
    // Separators, anchoring, and nothing else changed: case, Unicode, spaces.
    ExpectPath("tex\\髪.png", "./tex/髪.png");
    ExpectPath("tex/肌.png", "./tex/肌.png");
    ExpectPath("toon\\ト ゥ ー ン.bmp", "./toon/ト ゥ ー ン.bmp");
    ExpectPath("Tex.PNG", "./Tex.PNG");
    ExpectPath("a\\\\b//c.png", "./a/b/c.png");
    ExpectPath("./a/./b.png", "./a/b.png");
    ExpectPath("a/b/../c.png", "./a/c.png");
    ExpectPath("a/../b.png", "./b.png");
    ExpectPath("dir/", "./dir");
    ExpectPath(std::string("pad.png\0\0", 9), "./pad.png");

    // Nothing to author, and nothing refused.
    ExpectPath("", "");
    ExpectPath(".", "");
    ExpectPath("a/..", "");

    // Refused (§7.2): absolute, UNC, drive-qualified, a scheme, an escape.
    ExpectUnsafe("/abs/tex.png");
    ExpectUnsafe("\\abs\\tex.png");
    ExpectUnsafe("\\\\server\\share\\tex.png");
    ExpectUnsafe("//server/share/tex.png");
    ExpectUnsafe("C:\\tex\\a.png");
    ExpectUnsafe("c:tex.png");
    ExpectUnsafe("http://example.com/a.png");
    ExpectUnsafe("file:///a.png");
    ExpectUnsafe("..\\toon\\toon01.bmp");
    ExpectUnsafe("a/../../b.png");
    ExpectUnsafe("..");
    ExpectUnsafe(std::string("a\0b.png", 7));
}
