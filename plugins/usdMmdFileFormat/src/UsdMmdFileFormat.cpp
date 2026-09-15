// SPDX-License-Identifier: Apache-2.0
#include "UsdMmdFileFormat.h"

#include "usd/UsdMmdAuthorer.h"

#include <mmdPmx/Diagnostic.h>
#include <mmdPmx/Reader.h>

#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/pyLock.h"
#include "pxr/base/tf/registryManager.h"
#include "pxr/base/tf/type.h"
#include "pxr/usd/ar/asset.h"
#include "pxr/usd/ar/resolvedPath.h"
#include "pxr/usd/ar/resolver.h"
#include "pxr/usd/sdf/layer.h"

#include <array>
#include <cstddef>
#include <future>
#include <memory>
#include <span>
#include <string>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PUBLIC_TOKENS(UsdMmdFileFormatTokens, USDMMD_FILE_FORMAT_TOKENS);

// Register the format with the type system so the plug system can find it.
TF_REGISTRY_FUNCTION(TfType)
{
    SDF_DEFINE_FILE_FORMAT(UsdMmdFileFormat, SdfFileFormat);
}

namespace {

// Every read goes through Ar, as OpenUSD's own formats read, and never through
// a narrow path: the path is UTF-8 on every platform, while a plugin runs in a
// host whose code page is not ours (TEXT_ENCODING_POLICY.md §4).
std::shared_ptr<ArAsset>
OpenAsset(const std::string& resolvedPath)
{
    return ArGetResolver().OpenAsset(ArResolvedPath(resolvedPath));
}

}  // namespace

UsdMmdFileFormat::UsdMmdFileFormat()
    : SdfFileFormat(
          UsdMmdFileFormatTokens->Id,
          UsdMmdFileFormatTokens->Version,
          UsdMmdFileFormatTokens->Target,
          UsdMmdFileFormatTokens->Extension)
{
}

UsdMmdFileFormat::~UsdMmdFileFormat() = default;

bool
UsdMmdFileFormat::CanRead(const std::string& file) const
{
    if (SdfFileFormat::GetFileExtension(file) != UsdMmdFileFormatTokens->Extension) {
        return false;
    }
    const std::shared_ptr<ArAsset> asset = OpenAsset(file);
    if (!asset) {
        return false;
    }
    constexpr std::array<char, 4> kSignature{'P', 'M', 'X', ' '};
    std::array<char, 4> signature{};
    return asset->Read(signature.data(), signature.size(), 0) == signature.size()
        && signature == kSignature;
}

bool
UsdMmdFileFormat::Read(
    SdfLayer* layer,
    const std::string& resolvedPath,
    bool metadataOnly) const
{
    // The stage is small until Phase 2 and authored in one pass after it, so
    // there is no cheaper metadata-only path to take.
    (void)metadataOnly;

    const std::shared_ptr<ArAsset> asset = OpenAsset(resolvedPath);
    if (!asset) {
        TF_RUNTIME_ERROR("usdMmdFileFormat: could not open '%s'", resolvedPath.c_str());
        return false;
    }
    const std::size_t size = asset->GetSize();
    std::vector<std::byte> bytes(size);
    if (size > 0 && asset->Read(bytes.data(), size, 0) != size) {
        TF_RUNTIME_ERROR("usdMmdFileFormat: could not read '%s'", resolvedPath.c_str());
        return false;
    }

    auto parsed = mmd::pmx::Read(std::span<const std::byte>(bytes));
    std::vector<mmd::Diagnostic> diagnostics = parsed.diagnostics();

    // Surfacing (DIAGNOSTICS.md §4): a fatal diagnostic fails Read with a
    // runtime error whose text begins with the code; `error` and `warning`
    // are also posted as warnings. Everything recoverable is recorded on the
    // stage by the authorer.
    const auto postWarnings = [&resolvedPath](const std::vector<mmd::Diagnostic>& all,
                                              std::size_t from) {
        for (std::size_t i = from; i < all.size(); ++i) {
            const mmd::Diagnostic& d = all[i];
            if (d.severity == mmd::Severity::Warning || d.severity == mmd::Severity::Error) {
                TF_WARN("%s [%s]", mmd::FormatDiagnostic(d).c_str(), resolvedPath.c_str());
            }
        }
    };
    postWarnings(diagnostics, 0);

    if (!parsed.ok()) {
        TF_RUNTIME_ERROR("%s [%s]", mmd::FormatDiagnostic(*parsed.fatal()).c_str(),
            resolvedPath.c_str());
        return false;
    }

    // SdfLayer::Reload reads file formats under an outer SdfChangeBlock, and a
    // UsdStage authored on the calling thread cannot observe the prims it
    // authors until that block closes. Authoring on another thread keeps the
    // detached stage outside it (usd-vrm-plugins' importer does the same).
    //
    // Authoring sends change notices, and a Python listener registered with
    // Tf.Notice.RegisterGlobally runs on the worker and takes the GIL. Some
    // Python entry points reach Read still holding it -- Sdf.Layer.Reload
    // does -- so the wait releases the GIL, or the two threads wait on each
    // other forever (tests/integration/test_notice_listeners.py). It does
    // nothing when this thread does not hold the GIL.
    const std::size_t parserDiagnostics = diagnostics.size();
    std::string usda;
    const usdmmd::UsdMmdAuthorer authorer;
    auto task = std::async(std::launch::async, [&]() {
        return authorer.WriteToString(parsed.value(), &diagnostics, &usda);
    });
    bool authored = false;
    {
        TF_PY_ALLOW_THREADS_IN_SCOPE();
        authored = task.get();
    }
    if (!authored) {
        TF_RUNTIME_ERROR("usdMmdFileFormat: failed to author USD for '%s'",
            resolvedPath.c_str());
        return false;
    }
    postWarnings(diagnostics, parserDiagnostics);

    const SdfFileFormatConstPtr usdaFormat = SdfFileFormat::FindByExtension("usda");
    const SdfLayerRefPtr generated =
        SdfLayer::CreateAnonymous("usdMmdFileFormat.generated.usda", usdaFormat);
    if (!generated || !generated->ImportFromString(usda)) {
        TF_RUNTIME_ERROR("usdMmdFileFormat: the USD authored for '%s' could not be parsed",
            resolvedPath.c_str());
        return false;
    }

    layer->TransferContent(generated);
    return true;
}

bool
UsdMmdFileFormat::WriteToString(
    const SdfLayer& layer,
    std::string* str,
    const std::string& comment) const
{
    // Nothing writes PMX (DESIGN_POLICY.md §2.4); an imported layer is written
    // out as usda, which is what `usdcat` over a .pmx prints.
    const SdfFileFormatConstPtr usda = SdfFileFormat::FindByExtension("usda");
    if (usda) {
        return usda->WriteToString(layer, str, comment);
    }
    return layer.ExportToString(str);
}

PXR_NAMESPACE_CLOSE_SCOPE
