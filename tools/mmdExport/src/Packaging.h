// SPDX-License-Identifier: Apache-2.0
//
// The steps of mmd_export, each a function of its own
// (docs/design/PACKAGING_POLICY.md §3, §12). Private to the tool: nothing
// here is installed, and `main` and the unit tests are its only callers.
//
// The model is opened through the registered SdfFileFormat, as any OpenUSD
// host opens it, so the package is the stage `Usd.Stage.Open("model.pmx")`
// gives: nothing here links the parser, the canonical model or the importer
// (WORKSPACE.md §2.1).
//
// Every path is UTF-8 inside a std::string, OpenUSD's convention, and a
// std::filesystem::path is built from one only through PathFromUtf8
// (TEXT_ENCODING_POLICY.md §4).
#pragma once

#include "Diagnostics.h"

#include <pxr/pxr.h>
#include <pxr/usd/sdf/layer.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace mmdexport {

namespace fs = std::filesystem;

std::string Utf8(const fs::path& path);
fs::path PathFromUtf8(std::string_view utf8);

/// What a texture becomes in the package (§7).
enum class TextureAction {
    Keep,        ///< stored byte for byte, under its own name
    Rename,      ///< stored byte for byte, its content's extension appended
    Convert,     ///< decoded and stored as PNG, ".png" appended
    Unsupported, ///< USDZ cannot hold it and packaging does not convert it
};

struct TextureKind {
    TextureAction action = TextureAction::Unsupported;
    /// What Rename and Convert append to the name: ".png" or ".jpg".
    std::string appended;
    /// For Convert, the extension Hio reads the source as: "bmp" or "tga".
    std::string decoder;
};

/// The row of §7 a texture falls in, from its name's extension and the first
/// bytes of the file.
TextureKind ClassifyTexture(std::string_view name, const fs::path& file);

struct PackageAsset {
    /// The asset path as the stage authors it: "./spa/光沢.spa".
    std::string authoredPath;
    /// Where the file is stored in the archive: the authored path less its
    /// "./", with what §7 appends, e.g. "spa/光沢.spa.png".
    std::string archivePath;
    /// The file the authored path resolves to, beside the model.
    fs::path source;
    TextureKind kind;
};

struct PackagePlan {
    /// "<output stem>.usdc", the package's first entry (§4, §5).
    std::string rootLayer;
    /// Every file the stage names, once each, in byte order of archivePath.
    std::vector<PackageAsset> assets;
};

/// Scratch layout: the root layer and every texture at its archive path are
/// staged in PackageDirectory(scratch); conversions happen beside it.
fs::path PackageDirectory(const fs::path& scratch);

/// Opens the model through the registered file formats, and relays the
/// diagnostics the importer recorded on the stage. Null, with
/// MMD_PKG_INPUT_UNREADABLE, when it cannot.
PXR_NS::SdfLayerRefPtr OpenModel(const fs::path& input, Diagnostics* diagnostics);

/// Every file the layer names, resolved beside it, with its archive path and
/// what §7 does with it. An error for each file that is missing (§6),
/// unsupported or would collide (§7), and for any layer beside the input.
PackagePlan Discover(const PXR_NS::SdfLayerHandle& layer, const std::string& rootLayer,
                     Diagnostics* diagnostics);

/// Stages every asset of the plan in PackageDirectory(scratch) at its archive
/// path: a copy, or a lossless PNG through Hio for a Convert.
bool ConvertTextures(const PackagePlan& plan, const fs::path& scratch, Diagnostics* diagnostics);

/// The layer's content, spec for spec, as PackageDirectory(scratch)/rootLayer,
/// with only the asset paths of renamed and converted textures rewritten.
PXR_NS::SdfLayerRefPtr Materialize(const PXR_NS::SdfLayerHandle& layer, const PackagePlan& plan,
                                   const fs::path& scratch, Diagnostics* diagnostics);

/// §9's checks on the materialized layer, before anything is archived.
bool ValidateMaterialized(const PXR_NS::SdfLayerHandle& root, Diagnostics* diagnostics);

/// The archive: the root layer first, then every asset in the plan's order,
/// each from where ConvertTextures staged it.
bool WritePackage(const PXR_NS::SdfLayerHandle& root, const PackagePlan& plan,
                  const fs::path& usdz, Diagnostics* diagnostics);

/// §9's checks on the written package, the usdchecker validators included.
bool ValidatePackage(const fs::path& usdz, const PackagePlan& plan, Diagnostics* diagnostics);

/// Replaces `to` with `from`, or leaves `to` as it was.
bool MoveIntoPlace(const fs::path& from, const fs::path& to, Diagnostics* diagnostics);

} // namespace mmdexport
