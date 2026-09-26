// SPDX-License-Identifier: Apache-2.0
//
// Each step of mmd_export on its own (docs/design/PACKAGING_POLICY.md §14,
// "Unit"): every row of §7, discovery's errors, a lossless conversion that
// keeps each pixel where it was, materialization that changes nothing but
// the renamed asset paths, and the archive and its validation. The inputs
// are .usda layers and images written here, so no MMD plugin is loaded: the
// .pmx end of the tool is tests/test_mmd_export.py's.
#include "Packaging.h"

#include <pxr/base/tf/errorMark.h>
#include <pxr/base/vt/array.h>
#include <pxr/imaging/hio/image.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdf/fileFormat.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/zipFile.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

namespace fs = std::filesystem;
using namespace mmdexport;

namespace {

/// A directory of its own for each test, removed after it.
class TempDir {
public:
    explicit TempDir(const std::string& name)
    {
        std::random_device random;
        _path = fs::temp_directory_path() /
                ("mmd_export_tests-" + name + "-" + std::to_string(random()));
        fs::create_directories(_path);
    }
    ~TempDir()
    {
        std::error_code ec;
        fs::remove_all(_path, ec);
    }
    const fs::path& path() const { return _path; }

private:
    fs::path _path;
};

using Bytes = std::vector<unsigned char>;
using Rgba = std::array<unsigned char, 4>;
using Image = std::vector<std::vector<Rgba>>; // rows top to bottom

void
WriteFile(const fs::path& path, const Bytes& bytes)
{
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

void
Put16(Bytes* out, unsigned v)
{
    out->push_back(static_cast<unsigned char>(v & 0xff));
    out->push_back(static_cast<unsigned char>((v >> 8) & 0xff));
}

void
Put32(Bytes* out, std::uint32_t v)
{
    Put16(out, v & 0xffff);
    Put16(out, v >> 16);
}

const Bytes kPng{0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n', 0, 0, 0, 0};
const Bytes kJpeg{0xff, 0xd8, 0xff, 0xe0, 0, 0};
const Bytes kGif{'G', 'I', 'F', '8', '9', 'a', 1, 0, 1, 0};

/// A BMP stored bottom-up: 24-bit with padded rows, or 32-bit BGRA.
Bytes
Bmp(const Image& image, bool alpha)
{
    const std::uint32_t height = static_cast<std::uint32_t>(image.size());
    const std::uint32_t width = static_cast<std::uint32_t>(image[0].size());
    Bytes rows;
    for (auto row = image.rbegin(); row != image.rend(); ++row) {
        const std::size_t start = rows.size();
        for (const Rgba& p : *row) {
            rows.insert(rows.end(), {p[2], p[1], p[0]});
            if (alpha) {
                rows.push_back(p[3]);
            }
        }
        while ((rows.size() - start) % 4 != 0) {
            rows.push_back(0);
        }
    }
    Bytes out{'B', 'M'};
    Put32(&out, 14 + 40 + static_cast<std::uint32_t>(rows.size()));
    Put32(&out, 0);
    Put32(&out, 14 + 40);
    Put32(&out, 40);
    Put32(&out, width);
    Put32(&out, height);
    Put16(&out, 1);
    Put16(&out, alpha ? 32 : 24);
    Put32(&out, 0);
    Put32(&out, static_cast<std::uint32_t>(rows.size()));
    Put32(&out, 2835);
    Put32(&out, 2835);
    Put32(&out, 0);
    Put32(&out, 0);
    out.insert(out.end(), rows.begin(), rows.end());
    return out;
}

/// An uncompressed 32-bit TGA, from its bottom-left origin.
Bytes
Tga(const Image& image)
{
    Bytes out{0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    Put16(&out, static_cast<unsigned>(image[0].size()));
    Put16(&out, static_cast<unsigned>(image.size()));
    out.push_back(32);
    out.push_back(0x08);
    for (auto row = image.rbegin(); row != image.rend(); ++row) {
        for (const Rgba& p : *row) {
            out.insert(out.end(), {p[2], p[1], p[0], p[3]});
        }
    }
    return out;
}

const Image kOpaque{{{{255, 0, 0, 255}}, {{0, 255, 0, 255}}},
                    {{{0, 0, 255, 255}}, {{255, 255, 0, 255}}}};
const Image kTranslucent{{{{10, 20, 30, 255}}, {{40, 50, 60, 128}}},
                         {{{70, 80, 90, 0}}, {{200, 150, 100, 64}}}};

/// The pixels of an image file as Hio decodes them, as RGBA rows top to
/// bottom; an image without alpha reads as opaque.
Image
Decode(const fs::path& file)
{
    const HioImageSharedPtr image = HioImage::OpenForReading(Utf8(file));
    assert(image);
    const int width = image->GetWidth();
    const int height = image->GetHeight();
    const int channels = HioGetComponentCount(image->GetFormat());
    std::vector<unsigned char> data(static_cast<std::size_t>(width) * height * channels);
    HioImage::StorageSpec spec;
    spec.width = width;
    spec.height = height;
    spec.format = image->GetFormat();
    spec.flipped = false;
    spec.data = data.data();
    assert(image->Read(spec));
    Image out(height, std::vector<Rgba>(width));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const unsigned char* p = &data[(static_cast<std::size_t>(y) * width + x) * channels];
            out[y][x] = {p[0], p[1], p[2], channels == 4 ? p[3] : static_cast<unsigned char>(255)};
        }
    }
    return out;
}

/// A model-shaped layer: the stage metadata and /Asset the importer
/// authors, and one asset-valued attribute per path.
std::string
ModelUsda(const std::vector<std::string>& paths, const std::string& extra = {})
{
    std::string text = "#usda 1.0\n(\n    defaultPrim = \"Asset\"\n    metersPerUnit = 1\n"
                       "    upAxis = \"Y\"\n)\n\ndef Scope \"Asset\" (\n"
                       "    customData = {\n        dictionary mmd = {\n"
                       "            int stageContractVersion = 2\n        }\n    }\n)\n{\n";
    for (std::size_t i = 0; i < paths.size(); ++i) {
        text += "    custom asset tex" + std::to_string(i) + " = @" + paths[i] + "@\n";
    }
    return text + extra + "}\n";
}

SdfLayerRefPtr
WriteLayer(const fs::path& path, const std::string& usda)
{
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << usda;
    SdfLayerRefPtr layer = SdfLayer::FindOrOpen(Utf8(path));
    assert(layer);
    return layer;
}

void
TestClassify()
{
    const TempDir dir("classify");
    const auto kind = [&](const std::string& name, const Bytes& bytes) {
        const fs::path file = dir.path() / PathFromUtf8(name);
        WriteFile(file, bytes);
        return ClassifyTexture(name, file);
    };
    const auto is = [](const TextureKind& k, TextureAction action, const std::string& appended,
                       const std::string& decoder) {
        return k.action == action && k.appended == appended && k.decoder == decoder;
    };

    // Row 1: PNG under .png, JPEG under .jpg or .jpeg, byte for byte.
    assert(is(kind("a.png", kPng), TextureAction::Keep, "", ""));
    assert(is(kind("b.PNG", kPng), TextureAction::Keep, "", ""));
    assert(is(kind("c.jpg", kJpeg), TextureAction::Keep, "", ""));
    assert(is(kind("d.jpeg", kJpeg), TextureAction::Keep, "", ""));
    // Row 2: OpenEXR and AVIF by extension.
    assert(is(kind("e.exr", {'v', '/', '1', 1}), TextureAction::Keep, "", ""));
    assert(is(kind("f.avif", {0, 0, 0, 0x1c}), TextureAction::Keep, "", ""));
    // Row 3: BMP content under any extension, converted.
    assert(is(kind("g.bmp", Bmp(kOpaque, false)), TextureAction::Convert, ".png", "bmp"));
    assert(is(kind("h.spa", Bmp(kOpaque, false)), TextureAction::Convert, ".png", "bmp"));
    assert(is(kind("i.sph", Bmp(kOpaque, false)), TextureAction::Convert, ".png", "bmp"));
    assert(is(kind("j.png", Bmp(kOpaque, false)), TextureAction::Convert, ".png", "bmp"));
    // Row 4: TGA by extension, converted.
    assert(is(kind("k.tga", Tga(kOpaque)), TextureAction::Convert, ".png", "tga"));
    assert(is(kind("l.TGA", Tga(kOpaque)), TextureAction::Convert, ".png", "tga"));
    // Row 5: PNG or JPEG under another extension, renamed.
    assert(is(kind("m.spa", kPng), TextureAction::Rename, ".png", ""));
    assert(is(kind("n.sph", kJpeg), TextureAction::Rename, ".jpg", ""));
    assert(is(kind("o.jpg", kPng), TextureAction::Rename, ".png", ""));
    assert(is(kind("p", kPng), TextureAction::Rename, ".png", ""));
    // Row 6: anything else.
    assert(is(kind("q.gif", kGif), TextureAction::Unsupported, "", ""));
    assert(is(kind("r.dds", {'D', 'D', 'S', ' '}), TextureAction::Unsupported, "", ""));
    assert(is(kind("s.png", {}), TextureAction::Unsupported, "", ""));
    assert(is(ClassifyTexture("t.png", dir.path() / "absent.png"), TextureAction::Unsupported,
              "", ""));
    std::puts("classify: every row of PACKAGING_POLICY.md §7");
}

void
TestDiscoverErrors()
{
    const TempDir dir("discover");
    const fs::path model = dir.path() / PathFromUtf8("モデル");
    WriteFile(model / "tex/a.png", kPng);
    WriteFile(model / "tex/b.bmp", Bmp(kOpaque, false));
    WriteFile(model / "tex/b.bmp.png", kPng); // on disk, never named: still taken
    WriteFile(model / "tex/g.gif", kGif);
    WriteFile(model / "tex/c.sph", kPng);
    WriteFile(model / "tex/c.sph.png", kPng); // named too
    const SdfLayerRefPtr layer = WriteLayer(
        model / "m.usda", ModelUsda({"./tex/a.png", "./tex/b.bmp", "./tex/g.gif",
                                     "./tex/無い.png", "./tex/c.sph", "./tex/c.sph.png",
                                     "../outside.png", "/absolute.png"}));

    Diagnostics diagnostics;
    const PackagePlan plan = Discover(layer, "out.usdc", &diagnostics);
    assert(diagnostics.Count(code::MissingAsset) == 1);
    assert(diagnostics.Count(code::UnsupportedTexture) == 1);
    assert(diagnostics.Count(code::AssetNameCollision) == 2);
    assert(diagnostics.Count(code::UnexpectedDependency) == 2);
    assert(diagnostics.HasErrors() && !diagnostics.HasFatal());
    // Nothing is written by discovery.
    assert(!fs::exists(model / "out.usdc"));

    // A layer beside the input is a dependency no section decides on.
    const fs::path sub = dir.path() / "sub";
    WriteLayer(sub / "other.usda", "#usda 1.0\n");
    const SdfLayerRefPtr withSublayer =
        WriteLayer(sub / "m.usda", "#usda 1.0\n(\n    subLayers = [@./other.usda@]\n)\n");
    Diagnostics sublayer;
    Discover(withSublayer, "out.usdc", &sublayer);
    assert(sublayer.Count(code::UnexpectedDependency) == 1);
    std::puts("discover: missing, unsupported, colliding and unexpected dependencies");
}

void
TestDiscoverPlan()
{
    const TempDir dir("plan");
    WriteFile(dir.path() / "z.png", kPng);
    WriteFile(dir.path() / "a.png", kPng);
    WriteFile(dir.path() / "a/b.bmp", Bmp(kOpaque, false));
    WriteFile(dir.path() / "a/c.spa", kJpeg);
    const SdfLayerRefPtr layer = WriteLayer(
        dir.path() / "m.usda",
        ModelUsda({"./z.png", "./a/b.bmp", "./a.png", "./a/c.spa", "./z.png"}));

    Diagnostics diagnostics;
    const PackagePlan plan = Discover(layer, "out.usdc", &diagnostics);
    assert(!diagnostics.HasErrors());
    assert(plan.rootLayer == "out.usdc");
    // Once each, in byte order of the archive path (§5).
    std::vector<std::string> archive;
    for (const PackageAsset& asset : plan.assets) {
        archive.push_back(asset.archivePath);
    }
    assert((archive == std::vector<std::string>{"a.png", "a/b.bmp.png", "a/c.spa.jpg", "z.png"}));
    assert(plan.assets[1].authoredPath == "./a/b.bmp");
    assert(fs::equivalent(plan.assets[1].source, dir.path() / "a/b.bmp"));
    std::puts("discover: one entry per file, in archive order");
}

void
TestConvert()
{
    const TempDir dir("convert");
    const fs::path model = dir.path() / "model";
    WriteFile(model / "spa/s.spa", Bmp(kOpaque, false));
    WriteFile(model / "tex/t.bmp", Bmp(kTranslucent, true));
    WriteFile(model / "toon/u.tga", Tga(kTranslucent));
    WriteFile(model / "tex/k.png", kPng);
    const SdfLayerRefPtr layer =
        WriteLayer(model / "m.usda",
                   ModelUsda({"./spa/s.spa", "./tex/t.bmp", "./toon/u.tga", "./tex/k.png"}));

    Diagnostics diagnostics;
    const PackagePlan plan = Discover(layer, "out.usdc", &diagnostics);
    const fs::path scratch = dir.path() / "scratch";
    assert(ConvertTextures(plan, scratch, &diagnostics));
    assert(diagnostics.Count(code::TextureConverted) == 3);

    // Lossless, alpha kept, and no row or channel moved.
    const fs::path staged = PackageDirectory(scratch);
    assert(Decode(staged / "spa/s.spa.png") == kOpaque);
    assert(Decode(staged / "tex/t.bmp.png") == kTranslucent);
    assert(Decode(staged / "toon/u.tga.png") == kTranslucent);
    // Kept byte for byte.
    std::ifstream kept(staged / "tex/k.png", std::ios::binary);
    const Bytes bytes((std::istreambuf_iterator<char>(kept)), std::istreambuf_iterator<char>());
    assert(bytes == kPng);
    std::puts("convert: BMP (24- and 32-bit) and TGA to PNG, pixel for pixel");
}

void
TestMaterialize()
{
    const TempDir dir("materialize");
    WriteFile(dir.path() / "a.bmp", Bmp(kOpaque, false));
    WriteFile(dir.path() / "b.png", kPng);
    const std::string extra =
        "    custom asset[] many = [@./a.bmp@, @./b.png@, @@]\n"
        "    custom asset sampled.timeSamples = {\n        1: @./a.bmp@,\n        2: @./b.png@,\n    }\n"
        "    custom string provenance = \"./a.bmp\"\n";
    const SdfLayerRefPtr layer =
        WriteLayer(dir.path() / "m.usda", ModelUsda({"./a.bmp", "./b.png"}, extra));

    Diagnostics diagnostics;
    const PackagePlan plan = Discover(layer, "out.usdc", &diagnostics);
    const fs::path scratch = dir.path() / "scratch";
    const SdfLayerRefPtr root = Materialize(layer, plan, scratch, &diagnostics);
    assert(root && !diagnostics.HasErrors());
    assert(fs::equivalent(PathFromUtf8(root->GetRealPath()), PackageDirectory(scratch) / "out.usdc"));
    assert(root->GetFileFormat()->GetFormatId() == TfToken("usdc"));

    // Spec for spec: every field of every spec is the source's, but for the
    // one converted texture's asset paths.
    const std::map<std::string, std::string> renamed{{"./a.bmp", "./a.bmp.png"}};
    const auto rename = [&](const SdfAssetPath& p) {
        const auto it = renamed.find(p.GetAssetPath());
        return SdfAssetPath(it == renamed.end() ? p.GetAssetPath() : it->second);
    };
    const std::function<VtValue(const VtValue&)> expected = [&](const VtValue& v) -> VtValue {
        if (v.IsHolding<SdfAssetPath>()) {
            return VtValue(rename(v.UncheckedGet<SdfAssetPath>()));
        }
        if (v.IsHolding<VtArray<SdfAssetPath>>()) {
            VtArray<SdfAssetPath> out;
            for (const SdfAssetPath& p : v.UncheckedGet<VtArray<SdfAssetPath>>()) {
                out.push_back(rename(p));
            }
            return VtValue(out);
        }
        if (v.IsHolding<SdfTimeSampleMap>()) {
            SdfTimeSampleMap out;
            for (const auto& [time, sample] : v.UncheckedGet<SdfTimeSampleMap>()) {
                out[time] = expected(sample);
            }
            return VtValue(out);
        }
        return v;
    };
    std::size_t specs = 0;
    std::size_t rewritten = 0;
    layer->Traverse(SdfPath::AbsoluteRootPath(), [&](const SdfPath& path) {
        ++specs;
        assert(root->HasSpec(path));
        std::vector<TfToken> fields = layer->ListFields(path);
        std::vector<TfToken> rootFields = root->ListFields(path);
        std::sort(fields.begin(), fields.end());
        std::sort(rootFields.begin(), rootFields.end());
        assert(rootFields == fields);
        for (const TfToken& field : fields) {
            const VtValue source = layer->GetField(path, field);
            const VtValue want = expected(source);
            const VtValue got = root->GetField(path, field);
            if (got != want) {
                std::fprintf(stderr, "%s %s: %s != %s\n", path.GetText(), field.GetText(),
                             TfStringify(got).c_str(), TfStringify(want).c_str());
            }
            assert(got == want);
            rewritten += want == source ? 0 : 1;
        }
    });
    std::size_t rootSpecs = 0;
    root->Traverse(SdfPath::AbsoluteRootPath(), [&](const SdfPath&) { ++rootSpecs; });
    assert(rootSpecs == specs);
    assert(rewritten == 3); // tex0, many, sampled; provenance is a string
    std::puts("materialize: spec for spec, only the converted texture renamed");
}

/// The whole pipeline over a model-shaped layer, as `main` runs it.
struct Packaged {
    PackagePlan plan;
    fs::path usdz;
    bool ok = false;
};

Packaged
PackageLayer(const SdfLayerRefPtr& layer, const fs::path& scratch, Diagnostics* diagnostics)
{
    Packaged out;
    out.plan = Discover(layer, "out.usdc", diagnostics);
    assert(!diagnostics->HasErrors());
    assert(ConvertTextures(out.plan, scratch, diagnostics));
    const SdfLayerRefPtr root = Materialize(layer, out.plan, scratch, diagnostics);
    assert(root);
    assert(ValidateMaterialized(root, diagnostics));
    out.usdz = scratch / "out.usdz";
    assert(WritePackage(root, out.plan, out.usdz, diagnostics));
    out.ok = ValidatePackage(out.usdz, out.plan, diagnostics);
    return out;
}

void
TestWriteAndValidate()
{
    const TempDir dir("package");
    const fs::path model = dir.path() / PathFromUtf8("モデル-é");
    WriteFile(model / PathFromUtf8("tex/髪.png"), kPng);
    WriteFile(model / "toon/t.bmp", Bmp(kOpaque, false));
    WriteFile(model / "b.png", kPng);
    const SdfLayerRefPtr layer = WriteLayer(
        model / "m.usda", ModelUsda({"./toon/t.bmp", "./tex/髪.png", "./b.png"}));

    Diagnostics diagnostics;
    const fs::path scratch = dir.path() / "scratch";
    const Packaged packaged = PackageLayer(layer, scratch, &diagnostics);
    assert(packaged.ok);
    assert(!diagnostics.HasErrors());

    // The root layer first, then archive order; UTF-8 names as they are.
    {
        const SdfZipFile zip = SdfZipFile::Open(Utf8(packaged.usdz));
        const std::vector<std::string> entries(zip.begin(), zip.end());
        assert((entries ==
                std::vector<std::string>{"out.usdc", "b.png", "tex/髪.png", "toon/t.bmp.png"}));
    }

    // A package that is not the plan fails validation.
    PackagePlan other = packaged.plan;
    other.assets.pop_back();
    Diagnostics mismatch;
    assert(!ValidatePackage(packaged.usdz, other, &mismatch));
    assert(mismatch.Count(code::ValidationFailed) >= 1);

    // A materialized layer that is not the importer's stage fails before
    // anything is archived.
    const SdfLayerRefPtr bare = WriteLayer(dir.path() / "bare/bare.usda", "#usda 1.0\n");
    Diagnostics bareDiagnostics;
    assert(!ValidateMaterialized(bare, &bareDiagnostics));
    assert(bareDiagnostics.Count(code::ValidationFailed) == 4); // defaultPrim, /Asset, upAxis, metersPerUnit
    std::puts("package: archive order, UTF-8 names, and validation");
}

void
TestMoveIntoPlace()
{
    const TempDir dir("move");
    WriteFile(dir.path() / "new.usdz", {1, 2, 3});
    WriteFile(dir.path() / PathFromUtf8("出力/out.usdz"), {9});

    Diagnostics diagnostics;
    assert(MoveIntoPlace(dir.path() / "new.usdz", dir.path() / PathFromUtf8("出力/out.usdz"),
                         &diagnostics));
    std::ifstream in(dir.path() / PathFromUtf8("出力/out.usdz"), std::ios::binary);
    const Bytes bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    assert((bytes == Bytes{1, 2, 3}));
    std::size_t files = 0;
    for ([[maybe_unused]] const auto& entry :
         fs::directory_iterator(dir.path() / PathFromUtf8("出力"))) {
        ++files;
    }
    assert(files == 1); // no partial file left beside it

    Diagnostics failed;
    assert(!MoveIntoPlace(dir.path() / "new.usdz", dir.path() / "absent/out.usdz", &failed));
    assert(failed.Count(code::WriteFailed) == 1 && failed.HasFatal());
    std::puts("move: replaced whole, or not at all");
}

} // namespace

int
main()
{
    TestClassify();
    TestDiscoverErrors();
    TestDiscoverPlan();
    TestConvert();
    TestMaterialize();
    TestWriteAndValidate();
    TestMoveIntoPlace();
    std::puts("mmdExport unit tests passed");
    return 0;
}
