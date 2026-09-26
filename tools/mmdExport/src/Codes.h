// SPDX-License-Identifier: Apache-2.0
//
// The diagnostic codes mmd_export raises (docs/design/PACKAGING_POLICY.md
// §11, docs/reference/DIAGNOSTICS.md §5.8). Each code carries its severity:
// a code is never renamed, reused, or given another severity, and
// scripts/check_docs.py fails when this list and DIAGNOSTICS.md §5 disagree.
#pragma once

#include <string_view>

namespace mmdexport {

enum class Severity { Info, Warning, Error, Fatal };

struct Code {
    std::string_view id;
    Severity severity;
};

namespace code {
inline constexpr Code InputUnreadable{"MMD_PKG_INPUT_UNREADABLE", Severity::Fatal};
inline constexpr Code WriteFailed{"MMD_PKG_WRITE_FAILED", Severity::Fatal};
inline constexpr Code MissingAsset{"MMD_PKG_MISSING_ASSET", Severity::Error};
inline constexpr Code UnexpectedDependency{"MMD_PKG_UNEXPECTED_DEPENDENCY", Severity::Error};
inline constexpr Code UnsupportedTexture{"MMD_PKG_UNSUPPORTED_TEXTURE", Severity::Error};
inline constexpr Code AssetNameCollision{"MMD_PKG_ASSET_NAME_COLLISION", Severity::Error};
inline constexpr Code ValidationFailed{"MMD_PKG_VALIDATION_FAILED", Severity::Error};
inline constexpr Code TextureConverted{"MMD_PKG_TEXTURE_CONVERTED", Severity::Info};
} // namespace code

} // namespace mmdexport
