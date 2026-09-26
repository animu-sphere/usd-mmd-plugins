// SPDX-License-Identifier: Apache-2.0
//
// The steps of docs/design/PACKAGING_POLICY.md §3. Discovery, the archive and
// validation are OpenUSD's, and the image conversion is Hio's: there is no
// ZIP writer, codec or archive layout here (§2).
#include "Packaging.h"

#include <pxr/base/tf/diagnosticMgr.h>
#include <pxr/base/tf/errorMark.h>
#include <pxr/base/vt/dictionary.h>
#include <pxr/imaging/hio/image.h>
#include <pxr/imaging/hio/types.h>
#include <pxr/usd/ar/packageUtils.h>
#include <pxr/usd/ar/resolver.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdf/fileFormat.h>
#include <pxr/usd/sdf/layerUtils.h>
#include <pxr/usd/sdf/primSpec.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/sdf/zipFile.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdUtils/dependencies.h>
#include <pxr/usdValidation/usdValidation/context.h>
#include <pxr/usdValidation/usdValidation/error.h>
#include <pxr/usdValidation/usdValidation/registry.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <map>
#include <random>
#include <set>
#include <system_error>

PXR_NAMESPACE_USING_DIRECTIVE

namespace mmdexport {

namespace {

std::string
Quoted(std::string_view text)
{
    return "'" + std::string(text) + "'";
}

/// Every error the mark holds, as one line, and the mark cleared: each step
/// folds OpenUSD's reason into its own diagnostic rather than letting it
/// print on its own.
std::string
TakeErrors(TfErrorMark& mark)
{
    std::string text;
    for (const TfError& error : mark) {
        text += (text.empty() ? "" : "; ") + error.GetCommentary();
    }
    mark.Clear();
    return text;
}

std::string
WithReason(std::string message, const std::string& reason)
{
    return reason.empty() ? message : message + " (" + reason + ")";
}

std::string
AsciiLower(std::string_view text)
{
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    });
    return out;
}

/// The extension of the last segment, lower case, without its dot.
std::string
Extension(std::string_view name)
{
    const std::size_t slash = name.find_last_of('/');
    const std::string_view base = slash == std::string_view::npos ? name : name.substr(slash + 1);
    const std::size_t dot = base.find_last_of('.');
    return dot == std::string_view::npos ? std::string() : AsciiLower(base.substr(dot + 1));
}

/// "./a/b.png" -> "a/b.png"; empty when the path is not a plain relative
/// path inside the model's directory. The importer authors only such paths
/// (TEXT_ENCODING_POLICY.md §7.2), so anything else means it changed.
std::string
ArchivePathOf(std::string_view authored)
{
    if (authored.size() <= 2 || authored.substr(0, 2) != "./") {
        return {};
    }
    const std::string_view rest = authored.substr(2);
    if (rest.find('\\') != std::string_view::npos || rest.find(':') != std::string_view::npos) {
        return {};
    }
    std::size_t start = 0;
    while (start <= rest.size()) {
        const std::size_t end = std::min(rest.find('/', start), rest.size());
        const std::string_view segment = rest.substr(start, end - start);
        if (segment.empty() || segment == "." || segment == "..") {
            return {};
        }
        start = end + 1;
    }
    return std::string(rest);
}

/// Every asset path `layer` authors, as the dependency walk reports it
/// (§6), with the layers the walk found and what did not resolve.
struct Dependencies {
    std::vector<SdfLayerRefPtr> layers;
    std::set<std::string> assetPaths;
    std::vector<std::string> unresolved;
    std::string errors;
};

Dependencies
ComputeDependencies(const SdfLayerHandle& layer)
{
    Dependencies out;
    std::vector<std::string> resolved;
    TfErrorMark mark;
    UsdUtilsComputeAllDependencies(
        SdfAssetPath(layer->GetIdentifier()), &out.layers, &resolved, &out.unresolved,
        [&out, &layer](const SdfLayerHandle& from, const UsdUtilsDependencyInfo& info) {
            if (get_pointer(from) == get_pointer(layer)) {
                out.assetPaths.insert(info.GetAssetPath());
            }
            return info;
        });
    out.errors = TakeErrors(mark);
    return out;
}

bool
ReadHead(const fs::path& file, std::array<unsigned char, 8>* head, std::size_t* size)
{
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return false;
    }
    in.read(reinterpret_cast<char*>(head->data()), static_cast<std::streamsize>(head->size()));
    *size = static_cast<std::size_t>(in.gcount());
    return true;
}

/// `value` with every asset path `renamed` names replaced, wherever a field
/// can hold one; true when anything changed. Everything else, an empty
/// element of an asset array included, is kept as it is.
/// UsdUtilsModifyAssetPaths is not used: it drops an array's empty
/// elements even when asked to keep them, and §4 drops nothing.
bool
Rename(VtValue* value, const std::map<std::string, std::string>& renamed)
{
    const auto renameOne = [&renamed](const SdfAssetPath& path, SdfAssetPath* out) {
        const auto it = renamed.find(path.GetAssetPath());
        if (it == renamed.end()) {
            return false;
        }
        *out = SdfAssetPath(it->second);
        return true;
    };
    if (value->IsHolding<SdfAssetPath>()) {
        SdfAssetPath out;
        if (renameOne(value->UncheckedGet<SdfAssetPath>(), &out)) {
            *value = VtValue(out);
            return true;
        }
        return false;
    }
    if (value->IsHolding<VtArray<SdfAssetPath>>()) {
        VtArray<SdfAssetPath> array = value->UncheckedGet<VtArray<SdfAssetPath>>();
        bool changed = false;
        for (SdfAssetPath& each : array) {
            changed |= renameOne(each, &each);
        }
        if (changed) {
            *value = VtValue(array);
        }
        return changed;
    }
    if (value->IsHolding<SdfTimeSampleMap>()) {
        SdfTimeSampleMap samples = value->UncheckedGet<SdfTimeSampleMap>();
        bool changed = false;
        for (auto& [time, sample] : samples) {
            changed |= Rename(&sample, renamed);
        }
        if (changed) {
            *value = VtValue(samples);
        }
        return changed;
    }
    if (value->IsHolding<VtDictionary>()) {
        VtDictionary dictionary = value->UncheckedGet<VtDictionary>();
        bool changed = false;
        for (auto& [key, entry] : dictionary) {
            changed |= Rename(&entry, renamed);
        }
        if (changed) {
            *value = VtValue(dictionary);
        }
        return changed;
    }
    return false;
}

void
RenameAssetPaths(const SdfLayerHandle& layer, const std::map<std::string, std::string>& renamed)
{
    std::vector<SdfPath> paths;
    layer->Traverse(SdfPath::AbsoluteRootPath(),
                    [&paths](const SdfPath& path) { paths.push_back(path); });
    for (const SdfPath& path : paths) {
        for (const TfToken& field : layer->ListFields(path)) {
            VtValue value = layer->GetField(path, field);
            if (Rename(&value, renamed)) {
                layer->SetField(path, field, value);
            }
        }
    }
}

bool
Fail(Diagnostics* diagnostics, const Code& code, std::string message)
{
    diagnostics->Add(code, std::move(message));
    return false;
}

} // namespace

std::string
Utf8(const fs::path& path)
{
    const std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
}

fs::path
PathFromUtf8(std::string_view utf8)
{
    return fs::path(std::u8string(utf8.begin(), utf8.end()));
}

fs::path
PackageDirectory(const fs::path& scratch)
{
    return scratch / "package";
}

TextureKind
ClassifyTexture(std::string_view name, const fs::path& file)
{
    std::array<unsigned char, 8> head{};
    std::size_t size = 0;
    if (!ReadHead(file, &head, &size)) {
        return {};
    }
    static constexpr std::array<unsigned char, 8> kPng{0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    const bool png = size >= kPng.size() && std::equal(kPng.begin(), kPng.end(), head.begin());
    const bool jpeg = size >= 3 && head[0] == 0xff && head[1] == 0xd8 && head[2] == 0xff;
    const bool bmp = size >= 2 && head[0] == 'B' && head[1] == 'M';
    const std::string extension = Extension(name);

    if (extension == "exr" || extension == "avif") {
        return {TextureAction::Keep, {}, {}};
    }
    if (png) {
        return extension == "png" ? TextureKind{TextureAction::Keep, {}, {}}
                                  : TextureKind{TextureAction::Rename, ".png", {}};
    }
    if (jpeg) {
        return extension == "jpg" || extension == "jpeg"
                   ? TextureKind{TextureAction::Keep, {}, {}}
                   : TextureKind{TextureAction::Rename, ".jpg", {}};
    }
    if (bmp) {
        return {TextureAction::Convert, ".png", "bmp"};
    }
    if (extension == "tga") {
        return {TextureAction::Convert, ".png", "tga"};
    }
    return {};
}

SdfLayerRefPtr
OpenModel(const fs::path& input, Diagnostics* diagnostics)
{
    std::error_code ec;
    if (!fs::is_regular_file(input, ec)) {
        diagnostics->Add(code::InputUnreadable,
                         Quoted(Utf8(input)) + " is not a file");
        return {};
    }
    const std::string path = Utf8(fs::absolute(input, ec));
    const std::string extension = SdfFileFormat::GetFileExtension(path);
    if (!SdfFileFormat::FindByExtension(extension)) {
        diagnostics->Add(code::InputUnreadable,
                         "no registered file format reads '." + extension +
                             "' files: the importer must be on PXR_PLUGINPATH_NAME");
        return {};
    }

    TfErrorMark mark;
    SdfLayerRefPtr layer = SdfLayer::FindOrOpen(path);
    if (!layer) {
        // The importer's fatal code leads its error's text (DIAGNOSTICS.md
        // §4); it is printed first, as it was raised.
        for (const TfError& error : mark) {
            diagnostics->Relay(error.GetCommentary());
        }
        mark.Clear();
        diagnostics->Add(code::InputUnreadable,
                         Quoted(path) + " could not be read");
        return {};
    }
    mark.Clear();

    // The recoverable diagnostics the importer recorded describe the stage,
    // which is also the package's stage (§11).
    if (const SdfPrimSpecHandle asset = layer->GetPrimAtPath(SdfPath("/Asset"))) {
        const VtDictionary customData = asset->GetCustomData();
        if (const VtValue* recorded = customData.GetValueAtPath("mmd:diagnostics")) {
            if (recorded->IsHolding<VtStringArray>()) {
                for (const std::string& line : recorded->UncheckedGet<VtStringArray>()) {
                    diagnostics->Relay(line);
                }
            }
        }
    }
    return layer;
}

PackagePlan
Discover(const SdfLayerHandle& layer, const std::string& rootLayer, Diagnostics* diagnostics)
{
    PackagePlan plan;
    plan.rootLayer = rootLayer;

    const Dependencies found = ComputeDependencies(layer);
    for (const SdfLayerRefPtr& other : found.layers) {
        if (get_pointer(other) != get_pointer(layer)) {
            diagnostics->Add(code::UnexpectedDependency,
                             "the model's layer depends on the layer " +
                                 Quoted(other->GetIdentifier()) +
                                 ", and PACKAGING_POLICY.md does not say what it becomes");
        }
    }

    for (const std::string& authored : found.assetPaths) {
        const std::string archivePath = ArchivePathOf(authored);
        if (archivePath.empty()) {
            diagnostics->Add(code::UnexpectedDependency,
                             "the asset path " + Quoted(authored) +
                                 " is not a relative path inside the model's directory");
            continue;
        }
        const std::string anchored = SdfComputeAssetPathRelativeToLayer(layer, authored);
        const ArResolvedPath resolved = ArGetResolver().Resolve(anchored);
        const fs::path source = PathFromUtf8(resolved.GetPathString());
        std::error_code ec;
        if (!resolved || !fs::is_regular_file(source, ec)) {
            diagnostics->Add(code::MissingAsset,
                             Quoted(authored) + " does not resolve beside the model");
            continue;
        }
        const TextureKind kind = ClassifyTexture(archivePath, source);
        if (kind.action == TextureAction::Unsupported) {
            diagnostics->Add(code::UnsupportedTexture,
                             Quoted(authored) +
                                 " is not a PNG, JPEG, OpenEXR or AVIF file USDZ holds, "
                                 "nor a BMP or TGA packaging converts");
            continue;
        }
        plan.assets.push_back({authored, archivePath + kind.appended, source, kind});
    }

    // A name §7 makes must not be one the model already has: in the stage,
    // on disk beside the model, or the root layer's.
    std::set<std::string> named{plan.rootLayer};
    for (const PackageAsset& asset : plan.assets) {
        named.insert(asset.authoredPath.substr(2));
    }
    for (const PackageAsset& asset : plan.assets) {
        if (asset.kind.action == TextureAction::Keep) {
            if (asset.archivePath == plan.rootLayer) {
                diagnostics->Add(code::AssetNameCollision,
                                 Quoted(asset.authoredPath) + " has the root layer's name");
            }
            continue;
        }
        std::error_code ec;
        fs::path beside = asset.source;
        beside += PathFromUtf8(asset.kind.appended);
        if (named.count(asset.archivePath) != 0 || fs::exists(beside, ec)) {
            diagnostics->Add(code::AssetNameCollision,
                             Quoted(asset.authoredPath) + " would be stored as " +
                                 Quoted(asset.archivePath) +
                                 ", a name the model already has");
        }
    }

    std::sort(plan.assets.begin(), plan.assets.end(),
              [](const PackageAsset& a, const PackageAsset& b) {
                  return a.archivePath < b.archivePath;
              });
    return plan;
}

bool
ConvertTextures(const PackagePlan& plan, const fs::path& scratch, Diagnostics* diagnostics)
{
    const fs::path package = PackageDirectory(scratch);
    const fs::path work = scratch / "work";
    std::error_code ec;
    fs::create_directories(package, ec);
    fs::create_directories(work, ec);

    bool ok = true;
    for (std::size_t i = 0; i < plan.assets.size(); ++i) {
        const PackageAsset& asset = plan.assets[i];
        const fs::path staged = package / PathFromUtf8(asset.archivePath);
        fs::create_directories(staged.parent_path(), ec);

        if (asset.kind.action != TextureAction::Convert) {
            if (!fs::copy_file(asset.source, staged, fs::copy_options::overwrite_existing, ec)) {
                return Fail(diagnostics, code::WriteFailed,
                            "could not copy " + Quoted(asset.authoredPath) + " (" + ec.message() +
                                ")");
            }
            continue;
        }

        // Hio chooses its reader by extension, and a sphere map is named
        // .spa or .sph whatever it holds, so the source is read from a copy
        // named for its content. The names here are ASCII.
        const std::string stem = std::to_string(i);
        const fs::path in = work / (stem + "." + asset.kind.decoder);
        const fs::path out = work / (stem + ".png");
        if (!fs::copy_file(asset.source, in, fs::copy_options::overwrite_existing, ec)) {
            return Fail(diagnostics, code::WriteFailed,
                        "could not copy " + Quoted(asset.authoredPath) + " (" + ec.message() + ")");
        }

        TfErrorMark mark;
        const HioImageSharedPtr image = HioImage::OpenForReading(Utf8(in));
        const HioFormat format = image ? image->GetFormat() : HioFormatInvalid;
        if (!image || HioGetHioType(format) != HioTypeUnsignedByte) {
            ok = Fail(diagnostics, code::UnsupportedTexture,
                      WithReason(Quoted(asset.authoredPath) + " could not be decoded as an 8-bit " +
                                     (asset.kind.decoder == "bmp" ? "BMP" : "TGA"),
                                 TakeErrors(mark)));
            continue;
        }
        const int width = image->GetWidth();
        const int height = image->GetHeight();
        std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height *
                                          image->GetBytesPerPixel());
        HioImage::StorageSpec spec;
        spec.width = width;
        spec.height = height;
        spec.format = format;
        spec.flipped = false;
        spec.data = pixels.data();
        if (!image->Read(spec)) {
            ok = Fail(diagnostics, code::UnsupportedTexture,
                      WithReason(Quoted(asset.authoredPath) + " could not be decoded",
                                 TakeErrors(mark)));
            continue;
        }
        const HioImageSharedPtr png = HioImage::OpenForWriting(Utf8(out));
        if (!png || !png->Write(spec)) {
            return Fail(diagnostics, code::WriteFailed,
                        WithReason("could not write the PNG of " + Quoted(asset.authoredPath),
                                   TakeErrors(mark)));
        }
        fs::rename(out, staged, ec);
        if (ec) {
            return Fail(diagnostics, code::WriteFailed,
                        "could not stage the PNG of " + Quoted(asset.authoredPath) + " (" +
                            ec.message() + ")");
        }
        diagnostics->Add(code::TextureConverted,
                         Quoted(asset.authoredPath.substr(2)) + " is stored as the PNG " +
                             Quoted(asset.archivePath));
    }
    return ok;
}

SdfLayerRefPtr
Materialize(const SdfLayerHandle& layer, const PackagePlan& plan, const fs::path& scratch,
            Diagnostics* diagnostics)
{
    const fs::path path = PackageDirectory(scratch) / PathFromUtf8(plan.rootLayer);
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);

    TfErrorMark mark;
    SdfLayerRefPtr root = SdfLayer::CreateNew(Utf8(path));
    if (!root) {
        Fail(diagnostics, code::WriteFailed,
             WithReason("could not create " + Quoted(plan.rootLayer), TakeErrors(mark)));
        return {};
    }
    root->TransferContent(layer);

    // The one change packaging makes to the stage (§4): a converted or
    // renamed texture is named by its new name. Provenance keeps the source's.
    std::map<std::string, std::string> renamed;
    for (const PackageAsset& asset : plan.assets) {
        if (asset.kind.action != TextureAction::Keep) {
            renamed.emplace(asset.authoredPath, "./" + asset.archivePath);
        }
    }
    if (!renamed.empty()) {
        RenameAssetPaths(root, renamed);
    }

    if (!root->Save()) {
        Fail(diagnostics, code::WriteFailed,
             WithReason("could not write " + Quoted(plan.rootLayer), TakeErrors(mark)));
        return {};
    }
    const std::string reason = TakeErrors(mark);
    if (!reason.empty()) {
        Fail(diagnostics, code::WriteFailed,
             "materializing " + Quoted(plan.rootLayer) + " raised: " + reason);
        return {};
    }
    return root;
}

bool
ValidateMaterialized(const SdfLayerHandle& root, Diagnostics* diagnostics)
{
    bool ok = true;
    const auto failed = [&](const std::string& check) {
        ok = Fail(diagnostics, code::ValidationFailed,
                  "the materialized layer: " + check);
    };

    TfErrorMark mark;
    const UsdStageRefPtr stage = UsdStage::Open(root, UsdStage::LoadAll);
    if (!stage) {
        failed(WithReason("does not open as a stage", TakeErrors(mark)));
        return false;
    }
    for (const PcpErrorBasePtr& error : stage->GetCompositionErrors()) {
        failed("composition error: " + error->ToString());
    }

    if (root->GetDefaultPrim() != TfToken("Asset")) {
        failed("defaultPrim is " + Quoted(root->GetDefaultPrim().GetString()) + ", not 'Asset'");
    }
    const UsdPrim asset = stage->GetPrimAtPath(SdfPath("/Asset"));
    if (!asset) {
        failed("/Asset does not exist");
    } else if (asset.GetCustomDataByKey(TfToken("mmd:stageContractVersion")).IsEmpty()) {
        failed("/Asset carries no mmd:stageContractVersion");
    }
    VtValue upAxis;
    if (!root->HasField(SdfPath::AbsoluteRootPath(), TfToken("upAxis"), &upAxis) ||
        upAxis != VtValue(TfToken("Y"))) {
        failed("upAxis is not \"Y\"");
    }
    VtValue metersPerUnit;
    if (!root->HasField(SdfPath::AbsoluteRootPath(), TfToken("metersPerUnit"), &metersPerUnit) ||
        !metersPerUnit.IsHolding<double>() || metersPerUnit.UncheckedGet<double>() != 1.0) {
        failed("metersPerUnit is not 1");
    }

    const Dependencies found = ComputeDependencies(root);
    if (found.layers.size() != 1) {
        failed("depends on another layer");
    }
    for (const std::string& path : found.unresolved) {
        failed(Quoted(path) + " does not resolve");
    }
    mark.Clear();
    return ok;
}

bool
WritePackage(const SdfLayerHandle& root, const PackagePlan& plan, const fs::path& usdz,
             Diagnostics* diagnostics)
{
    const fs::path staged = PathFromUtf8(root->GetRealPath()).parent_path();
    TfErrorMark mark;
    SdfZipFileWriter writer = SdfZipFileWriter::CreateNew(Utf8(usdz));
    if (!writer) {
        return Fail(diagnostics, code::WriteFailed,
                    WithReason("could not create the archive", TakeErrors(mark)));
    }
    const auto add = [&](const std::string& archivePath) {
        if (writer.AddFile(Utf8(staged / PathFromUtf8(archivePath)), archivePath).empty()) {
            writer.Discard();
            return Fail(diagnostics, code::WriteFailed,
                        WithReason("could not add " + Quoted(archivePath) + " to the archive",
                                   TakeErrors(mark)));
        }
        return true;
    };
    // The root layer first, as the USDZ specification requires (§5).
    if (!add(plan.rootLayer)) {
        return false;
    }
    for (const PackageAsset& asset : plan.assets) {
        if (!add(asset.archivePath)) {
            return false;
        }
    }
    if (!writer.Save()) {
        return Fail(diagnostics, code::WriteFailed,
                    WithReason("could not save the archive", TakeErrors(mark)));
    }
    mark.Clear();
    return true;
}

bool
ValidatePackage(const fs::path& usdz, const PackagePlan& plan, Diagnostics* diagnostics)
{
    bool ok = true;
    const auto failed = [&](const std::string& check) {
        ok = Fail(diagnostics, code::ValidationFailed, "the package: " + check);
    };
    const std::string path = Utf8(usdz);
    TfErrorMark mark;

    // Exactly the planned entries, the root layer first.
    {
        const SdfZipFile zip = SdfZipFile::Open(path);
        if (!zip) {
            failed(WithReason("is not a ZIP archive", TakeErrors(mark)));
            return false;
        }
        std::vector<std::string> expected{plan.rootLayer};
        for (const PackageAsset& asset : plan.assets) {
            expected.push_back(asset.archivePath);
        }
        const std::vector<std::string> entries(zip.begin(), zip.end());
        if (entries != expected) {
            failed("holds " + std::to_string(entries.size()) + " entries, not the " +
                   std::to_string(expected.size()) + " planned, root layer first");
        }
    }

    const UsdStageRefPtr stage = UsdStage::Open(path, UsdStage::LoadAll);
    if (!stage) {
        failed(WithReason("does not open as a stage", TakeErrors(mark)));
        return false;
    }
    for (const PcpErrorBasePtr& error : stage->GetCompositionErrors()) {
        failed("composition error: " + error->ToString());
    }

    // No layer from outside: nothing the stage needs is left behind, the
    // model least of all. The package's root layer is named by the package's
    // own path, and what it holds by "<package>[<entry>]", in whichever
    // spelling of the package's path the resolver produced.
    const auto samePath = [&usdz](const std::string& path) {
        std::error_code ec;
        return fs::equivalent(PathFromUtf8(path), usdz, ec);
    };
    const auto inside = [&samePath](const std::string& resolved) {
        return ArIsPackageRelativePath(resolved) &&
               samePath(ArSplitPackageRelativePathOuter(resolved).first);
    };
    for (const SdfLayerHandle& layer : stage->GetUsedLayers()) {
        const std::string real = layer->GetRealPath();
        if (!layer->IsAnonymous() && !samePath(real) && !inside(real)) {
            failed("uses the layer " + Quoted(layer->GetIdentifier()) + " from outside it");
        }
    }

    // Every asset value resolves inside the package, and none is absolute.
    const auto check = [&](const SdfAssetPath& value, const UsdAttribute& attribute) {
        const std::string& authored = value.GetAssetPath();
        if (authored.empty()) {
            return;
        }
        const std::string where = attribute.GetPath().GetString();
        if (authored.front() == '/' || authored.front() == '\\' ||
            authored.find(':') != std::string::npos) {
            failed(where + " names the absolute path " + Quoted(authored));
        } else if (!inside(value.GetResolvedPath())) {
            failed(where + ": " + Quoted(authored) + " does not resolve inside the package");
        }
    };
    for (const UsdPrim& prim : UsdPrimRange::Stage(stage, UsdPrimAllPrimsPredicate)) {
        for (const UsdAttribute& attribute : prim.GetAuthoredAttributes()) {
            const SdfValueTypeName type = attribute.GetTypeName();
            if (type != SdfValueTypeNames->Asset && type != SdfValueTypeNames->AssetArray) {
                continue;
            }
            std::vector<UsdTimeCode> times{UsdTimeCode::Default()};
            std::vector<double> samples;
            attribute.GetTimeSamples(&samples);
            times.insert(times.end(), samples.begin(), samples.end());
            for (const UsdTimeCode time : times) {
                VtValue value;
                if (!attribute.Get(&value, time)) {
                    continue;
                }
                if (value.IsHolding<SdfAssetPath>()) {
                    check(value.UncheckedGet<SdfAssetPath>(), attribute);
                } else if (value.IsHolding<VtArray<SdfAssetPath>>()) {
                    for (const SdfAssetPath& each : value.UncheckedGet<VtArray<SdfAssetPath>>()) {
                        check(each, attribute);
                    }
                }
            }
        }
    }

    // What `usdchecker <package>` runs by default: every registered validator
    // but the root-package-only one, and an Error fails (usdchecker.cpp).
    UsdValidationValidatorMetadataVector metadata =
        UsdValidationRegistry::GetInstance().GetAllValidatorMetadata();
    const TfToken rootPackageOnly("usdUtilsValidators:RootPackageValidator");
    metadata.erase(std::remove_if(metadata.begin(), metadata.end(),
                                  [&](const UsdValidationValidatorMetadata& m) {
                                      return m.name == rootPackageOnly;
                                  }),
                   metadata.end());
    const UsdValidationContext context(metadata);
    for (const UsdValidationError& error : context.Validate(stage)) {
        if (error.GetType() == UsdValidationErrorType::Error) {
            failed("usdchecker: " + error.GetErrorAsString());
        }
    }
    mark.Clear();
    return ok;
}

bool
MoveIntoPlace(const fs::path& from, const fs::path& to, Diagnostics* diagnostics)
{
    // A copy beside the output first, so the last step is a rename in one
    // directory: `to` is either what it was or the whole package.
    std::random_device random;
    fs::path partial = to.parent_path() / to.filename();
    partial += PathFromUtf8("." + std::to_string(random()) + ".partial");
    std::error_code ec;
    if (!fs::copy_file(from, partial, fs::copy_options::overwrite_existing, ec)) {
        fs::remove(partial, ec);
        return Fail(diagnostics, code::WriteFailed,
                    "could not write beside " + Quoted(Utf8(to)));
    }
    fs::rename(partial, to, ec);
    if (ec) {
        const std::string reason = ec.message();
        fs::remove(partial, ec);
        return Fail(diagnostics, code::WriteFailed,
                    "could not replace " + Quoted(Utf8(to)) + " (" + reason + ")");
    }
    return true;
}

} // namespace mmdexport
