// SPDX-License-Identifier: Apache-2.0
//
// Texture paths: preserve, then normalize (docs/design/TEXT_ENCODING_POLICY.md
// §7). A pure function of the source string: nothing here touches a
// filesystem, so the same bytes produce the same stage on any machine (§7.3).
#pragma once

#include <string>
#include <string_view>

namespace mmd::detail {

struct TexturePath {
    /// The normalized logical path anchored to the model's layer
    /// ("./tex/髪.png"), or empty when there is nothing to author.
    std::string assetPath;
    /// True when the path is refused (§7.2): absolute, drive-qualified, UNC,
    /// carrying a URI scheme, escaping the model's directory, or holding a
    /// control character (C0, DEL, C1) other than trailing U+0000 padding.
    bool unsafe = false;
};

/// `\` becomes `/`, empty and `.` segments go, `x/..` pairs resolve
/// lexically, trailing U+0000 padding is dropped, and the result is anchored
/// with `./`. Case and Unicode form are kept exactly (§7.1). A path that
/// normalizes to nothing is neither authored nor unsafe.
TexturePath NormalizeTexturePath(std::string_view source);

} // namespace mmd::detail
