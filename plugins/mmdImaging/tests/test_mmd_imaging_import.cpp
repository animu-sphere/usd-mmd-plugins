// SPDX-License-Identifier: Apache-2.0
//
// mmdImaging over the importer's stages (docs/design/MATERIAL_POLICY.md
// §12.1): what reaches Hydra from a `.pmx` is what the source says.
//
// Every fixture in the importer's manifest that opens is read through
// UsdImaging's stage scene index, with the importer, mmdSchema and mmdImaging
// registered. The expectation is the manifest's, written by the fixture
// generator from the bytes it encoded (tests/fixtures/generate_fixtures.py),
// never read from the stage:
//
//   * the prims under /Asset/mtl with an `mmd` contribution are exactly the
//     source's materials;
//   * `mmd/drawOrder` is each material's table index;
//   * `mmd/material/doubleSided` is the source's flag;
//   * a texture slot is listed exactly when the source names a safe path,
//     with that path, resolving exactly when the file is beside the fixture.
//
// And every listed field holds the value the importer authored for it, so
// the contribution and the stage never disagree.
//
// The plugin is not linked here either; it is reached through
// PXR_PLUGINPATH_NAME, as a runtime composition reaches it.

#include "pxr/pxr.h"

// First, before anything that reaches <windows.h>: hd's material network
// schema has a token named `interface`, which <objbase.h> defines as `struct`.
#include "pxr/imaging/hd/materialNetworkSchema.h"

#include "pxr/base/js/json.h"
#include "pxr/base/js/value.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/vt/value.h"
#include "pxr/imaging/hd/dataSource.h"
#include "pxr/imaging/hd/dataSourceLocator.h"
#include "pxr/usd/sdf/assetPath.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/usd/attribute.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usd/timeCode.h"
#include "pxr/usdImaging/usdImaging/stageSceneIndex.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <set>
#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

const TfToken kMmd("mmd");
const TfToken kMaterial("material");
const TfToken kDrawOrder("drawOrder");
const char* const kSlots[] = {"texture", "sphereTexture", "toonTexture"};

int failures = 0;

void
Expect(bool condition, const std::string& what)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what.c_str());
        ++failures;
    }
}

const JsObject*
Object(const JsObject& object, const char* key)
{
    const auto it = object.find(key);
    return it != object.end() && it->second.IsObject() ? &it->second.GetJsObject()
                                                       : nullptr;
}

// One fixture: its materials as the manifest states them, against the scene
// index's view of the stage the importer authors.
void
CheckFixture(const std::string& fixtures, const std::string& name,
             const JsArray& materials)
{
    const std::string where = name;
    const UsdStageRefPtr stage = UsdStage::Open(fixtures + "/" + name);
    Expect(bool(stage), where + " does not open");
    if (!stage) {
        return;
    }
    const UsdImagingStageSceneIndexRefPtr sceneIndex =
        UsdImagingStageSceneIndex::New();
    sceneIndex->SetStage(stage);
    sceneIndex->SetTime(UsdTimeCode::Default());

    std::set<std::string> expected;
    for (const JsValue& value : materials) {
        const JsObject& m = value.GetJsObject();
        const std::string id = m.at("id").GetString();
        expected.insert(id);
        const std::string path = "/Asset/mtl/" + id;
        const std::string at = where + " " + path;
        const HdContainerDataSourceHandle data =
            sceneIndex->GetPrim(SdfPath(path)).dataSource;

        const HdIntDataSourceHandle drawOrder = HdIntDataSource::Cast(
            HdContainerDataSource::Get(data, HdDataSourceLocator(kMmd, kDrawOrder)));
        Expect(drawOrder && drawOrder->GetTypedValue(0.0f) ==
                                m.at("sourceIndex").GetInt(),
               at + " mmd/drawOrder is not the material's table index");

        const HdContainerDataSourceHandle fields = HdContainerDataSource::Cast(
            HdContainerDataSource::Get(data, HdDataSourceLocator(kMmd, kMaterial)));
        Expect(bool(fields), at + " has no mmd/material");
        if (!fields) {
            continue;
        }
        const HdBoolDataSourceHandle doubleSided =
            HdBoolDataSource::Cast(fields->Get(TfToken("doubleSided")));
        Expect(doubleSided && doubleSided->GetTypedValue(0.0f) ==
                                  m.at("doubleSided").GetBool(),
               at + " mmd/material/doubleSided is not the source's flag");

        const JsObject* textures = Object(m, "textures");
        for (const char* slot : kSlots) {
            const JsObject* want = textures ? Object(*textures, slot) : nullptr;
            const bool authored = want && !want->at("asset").IsNull();
            const auto got = HdTypedSampledDataSource<SdfAssetPath>::Cast(
                fields->Get(TfToken(slot)));
            if (!authored) {
                Expect(!got, at + " lists " + slot + ", which the source does "
                                  "not name safely");
                continue;
            }
            Expect(bool(got), at + " does not list " + slot);
            if (!got) {
                continue;
            }
            const SdfAssetPath asset = got->GetTypedValue(0.0f);
            Expect(asset.GetAssetPath() == want->at("asset").GetString(),
                   at + " " + slot + " is @" + asset.GetAssetPath() + "@");
            Expect(!asset.GetResolvedPath().empty() ==
                       want->at("resolves").GetBool(),
                   at + " " + slot + " resolves to '" +
                       asset.GetResolvedPath() + "'");
        }

        // Every listed field holds the importer's value: the authored one,
        // which a fallback would otherwise stand in for.
        const UsdPrim prim = stage->GetPrimAtPath(SdfPath(path));
        for (const TfToken& field : fields->GetNames()) {
            const HdSampledDataSourceHandle sampled =
                HdSampledDataSource::Cast(fields->Get(field));
            VtValue authored;
            prim.GetAttribute(TfToken("inputs:mmd:material:" + field.GetString()))
                .Get(&authored);
            VtValue hydra = sampled ? sampled->GetValue(0.0f) : VtValue();
            if (hydra.IsHolding<SdfAssetPath>() &&
                authored.IsHolding<SdfAssetPath>()) {
                Expect(hydra.UncheckedGet<SdfAssetPath>().GetAssetPath() ==
                           authored.UncheckedGet<SdfAssetPath>().GetAssetPath(),
                       at + " mmd/material/" + field.GetString());
            } else {
                Expect(hydra == authored,
                       at + " mmd/material/" + field.GetString() +
                           " differs from the stage");
            }
        }
    }

    // No other prim under /Asset/mtl carries the contribution.
    std::set<std::string> contributed;
    for (const SdfPath& child : sceneIndex->GetChildPrimPaths(SdfPath("/Asset/mtl"))) {
        if (HdContainerDataSource::Get(sceneIndex->GetPrim(child).dataSource,
                                       HdDataSourceLocator(kMmd))) {
            contributed.insert(child.GetName());
        }
    }
    Expect(contributed == expected,
           where + ": the prims with an mmd contribution are not the source's "
                   "materials");
    std::printf("  %s: %zu materials\n", name.c_str(), expected.size());
}

} // namespace

int
main(int argc, char** argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <importer fixtures directory>\n", argv[0]);
        return 2;
    }
    const std::string fixtures = argv[1];
    std::ifstream stream(fixtures + "/fixtures.json");
    assert(stream && "no fixtures.json in the fixtures directory");
    JsParseError error;
    const JsValue manifest = JsParseStream(stream, &error);
    assert(manifest.IsObject() && "fixtures.json does not parse");

    int checked = 0;
    for (const auto& [name, entry] : manifest.GetJsObject()) {
        const JsObject& fixture = entry.GetJsObject();
        if (!fixture.at("opens").GetBool()) {
            continue;
        }
        const JsObject* stage = Object(fixture, "stage");
        const auto materials = stage ? stage->find("materials") : JsObject::const_iterator();
        if (!stage || materials == stage->end() || !materials->second.IsArray() ||
            materials->second.GetJsArray().empty()) {
            continue;
        }
        CheckFixture(fixtures, name, materials->second.GetJsArray());
        ++checked;
    }
    assert(checked > 0 && "the manifest names no fixture with materials");
    if (failures) {
        std::fprintf(stderr, "mmdImaging_import: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("mmdImaging_import: passed (%d fixtures)\n", checked);
    return 0;
}
