// SPDX-License-Identifier: Apache-2.0
//
// mmdImaging (docs/design/MATERIAL_POLICY.md §12.1): an `MmdMaterialAPI`
// value reaches a Hydra consumer through UsdImaging's own stage scene index,
// and an edit of it dirties the one locator it feeds.
//
// The consumer here is the scene index a Hydra renderer is handed. It reads
// prims and data sources and observes notices; the stage is only ever
// written, as an editor or a runtime would write it. The plugin is not
// linked: it is found through PXR_PLUGINPATH_NAME and the staged
// plugInfo.json, and loaded by UsdImaging's adapter registry when the first
// prim with `MmdMaterialAPI` is populated -- the path a packaged plugin takes.
//
// What is measured:
//
//   * canonical -> Hydra: authored values and schema fallbacks under
//     `mmd/material/<field>`, the field set being the schema's and nothing
//     else, an unauthored texture slot absent, an authored one an asset path
//     with nothing loaded, beside a `material` container the adapter leaves
//     alone (items 1-4);
//   * draw order: `mmd/drawOrder` from `customData` `mmd:sourceIndex`, absent
//     without it (item 6);
//   * invalidation: an edit dirties exactly `mmd/material/<field>` of the
//     contribution, a texture slot appearing dirties its own locator, and a
//     property the schema does not define dirties nothing under `mmd` (item
//     5). UsdImaging's own material adapter dirties the whole `material`
//     locator beside it on every such edit, which is pinned;
//   * a `customData` edit: UsdImaging drops a change to a built-in prim field,
//     so it reaches no adapter, which is pinned too;
//   * time samples: moving the scene index's time dirties the sampled field
//     and nothing else, and the value follows (item 4);
//   * with `--without-schema`, under an environment that registers no
//     mmdSchema: no contribution at all.

#include "pxr/pxr.h"

// First, before anything that reaches <windows.h>: hd's material network
// schema has a token named `interface`, which <objbase.h> defines as `struct`.
#include "pxr/imaging/hd/materialNetworkSchema.h"
#include "pxr/imaging/hd/materialSchema.h"

#include "pxr/base/gf/vec3f.h"
#include "pxr/base/gf/vec4f.h"
#include "pxr/base/plug/plugin.h"
#include "pxr/base/plug/registry.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/vt/value.h"
#include "pxr/imaging/hd/dataSource.h"
#include "pxr/imaging/hd/dataSourceLocator.h"
#include "pxr/imaging/hd/sceneIndexObserver.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/usd/sdf/assetPath.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/sdf/types.h"
#include "pxr/usd/usd/attribute.h"
#include "pxr/usd/usd/primDefinition.h"
#include "pxr/usd/usd/schemaRegistry.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usd/timeCode.h"
#include "pxr/usdImaging/usdImaging/adapterRegistry.h"
#include "pxr/usdImaging/usdImaging/stageSceneIndex.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

const TfToken kMmd("mmd");
const TfToken kMaterial("material");
const char* const kFace = "/Asset/mtl/face";
const char* const kSkin = "/Asset/mtl/skin";
const char* const kNoProvenance = "/Asset/mtl/noProvenance";
const char* const kPlain = "/Asset/mtl/plain";
const std::string kPrefix = "inputs:mmd:material:";

HdDataSourceLocator
FieldLocator(const char* field)
{
    return HdDataSourceLocator(kMmd, kMaterial, TfToken(field));
}

HdDataSourceLocator
DrawOrderLocator()
{
    return HdDataSourceLocator(kMmd, TfToken("drawOrder"));
}

// Every dirty notice, merged per prim, between two Clear()s.
class Recorder : public HdSceneIndexObserver
{
public:
    void PrimsAdded(const HdSceneIndexBase&, const AddedPrimEntries&) override {}
    void PrimsRemoved(const HdSceneIndexBase&, const RemovedPrimEntries&) override {}
    void PrimsRenamed(const HdSceneIndexBase&, const RenamedPrimEntries&) override {}
    void PrimsDirtied(const HdSceneIndexBase&,
                      const DirtiedPrimEntries& entries) override
    {
        for (const DirtiedPrimEntry& entry : entries) {
            dirtied[entry.primPath].insert(entry.dirtyLocators);
        }
    }

    HdDataSourceLocatorSet At(const char* path) const
    {
        const auto it = dirtied.find(SdfPath(path));
        return it == dirtied.end() ? HdDataSourceLocatorSet() : it->second;
    }

    void Clear() { dirtied.clear(); }

    std::map<SdfPath, HdDataSourceLocatorSet> dirtied;
};

struct Session
{
    UsdStageRefPtr stage;
    UsdImagingStageSceneIndexRefPtr sceneIndex;
    Recorder recorder;
};

void
Open(Session& session, const std::string& fixture)
{
    session.stage = UsdStage::Open(fixture);
    assert(session.stage && "the fixture does not open");
    session.sceneIndex = UsdImagingStageSceneIndex::New();
    session.sceneIndex->SetStage(session.stage);
    session.sceneIndex->SetTime(UsdTimeCode(0.0));
    session.sceneIndex->AddObserver(HdSceneIndexObserverPtr(&session.recorder));
}

HdContainerDataSourceHandle
PrimData(const Session& session, const char* path)
{
    return session.sceneIndex->GetPrim(SdfPath(path)).dataSource;
}

HdDataSourceBaseHandle
At(const Session& session, const char* path, const HdDataSourceLocator& locator)
{
    return HdContainerDataSource::Get(PrimData(session, path), locator);
}

VtValue
ValueAt(const Session& session, const char* path,
        const HdDataSourceLocator& locator)
{
    const HdSampledDataSourceHandle sampled =
        HdSampledDataSource::Cast(At(session, path, locator));
    return sampled ? sampled->GetValue(0.0f) : VtValue();
}

template <typename T>
T
TypedAt(const Session& session, const char* path, const char* field)
{
    const auto typed = HdTypedSampledDataSource<T>::Cast(
        At(session, path, FieldLocator(field)));
    if (!typed) {
        std::fprintf(stderr, "%s: mmd/material/%s is missing or not of its "
                             "attribute's type\n", path, field);
    }
    assert(typed);
    return typed->GetTypedValue(0.0f);
}

bool
Near(const GfVec4f& a, const GfVec4f& b)
{
    return (a - b).GetLength() < 1e-6f;
}

UsdAttribute
Attribute(const Session& session, const char* path, const std::string& name)
{
    const UsdAttribute attribute =
        session.stage->GetPrimAtPath(SdfPath(path)).GetAttribute(TfToken(name));
    assert(attribute && "the fixture lost an attribute this test edits");
    return attribute;
}

void
PrintLocators(const char* what, const HdDataSourceLocatorSet& locators)
{
    std::fprintf(stderr, "%s dirtied:\n", what);
    for (const HdDataSourceLocator& locator : locators) {
        std::fprintf(stderr, "  %s\n", locator.GetString().c_str());
    }
}

const UsdPrimDefinition*
Definition()
{
    return UsdSchemaRegistry::GetInstance().FindAppliedAPIPrimDefinition(
        TfToken("MmdMaterialAPI"));
}

void
TestTheAdapterIsDiscovered()
{
    assert(PlugRegistry::GetInstance().GetPluginWithName("MmdImaging") &&
           "PXR_PLUGINPATH_NAME does not reach mmdImaging's plugInfo.json");
    assert(UsdImagingAdapterRegistry::GetInstance().HasAPISchemaAdapter(
               TfToken("MmdMaterialAPI")) &&
           "UsdImaging registered no adapter for MmdMaterialAPI");
}

// ---------------------------------------------------------------------------
// Items 1-4: canonical -> Hydra
// ---------------------------------------------------------------------------
void
TestCanonicalValuesReachHydra(const Session& session)
{
    const HdSceneIndexPrim face = session.sceneIndex->GetPrim(SdfPath(kFace));
    assert(face.primType == HdPrimTypeTokens->material);

    // Authored, of each value type the schema uses, each castable to its
    // attribute's type.
    assert(Near(TypedAt<GfVec4f>(session, kFace, "diffuseColor"),
                GfVec4f(0.8f, 0.7f, 0.6f, 0.9f)));
    assert(TypedAt<GfVec3f>(session, kFace, "specularColor") ==
           GfVec3f(0.1f, 0.2f, 0.3f));
    assert(TypedAt<float>(session, kFace, "specularPower") == 12.5f);
    assert(TypedAt<bool>(session, kFace, "drawEdge"));
    assert(TypedAt<TfToken>(session, kFace, "sphereMode") == TfToken("multiply"));
    assert(TypedAt<int>(session, kFace, "sharedToonIndex") == 4);

    // Unauthored: the schema fallback, read from the definition rather than
    // restated here.
    const UsdPrimDefinition* definition = Definition();
    assert(definition);
    VtValue fallback;
    assert(definition->GetAttributeFallbackValue(
        TfToken(kPrefix + "ambientColor"), &fallback));
    assert(TypedAt<GfVec3f>(session, kFace, "ambientColor") ==
           fallback.Get<GfVec3f>());
    assert(TypedAt<bool>(session, kFace, "receiveSelfShadow") == false);
    assert(TypedAt<int>(session, kNoProvenance, "sharedToonIndex") == -1);
    assert(TypedAt<TfToken>(session, kNoProvenance, "toonSource") ==
           TfToken("none"));

    // An authored texture slot: the authored path and the resolved one.
    const SdfAssetPath texture = TypedAt<SdfAssetPath>(session, kFace, "texture");
    assert(texture.GetAssetPath() == "textures/base.png");
    assert(!texture.GetResolvedPath().empty() &&
           "the authored texture did not resolve");
    std::printf("texture: %s -> %s\n", texture.GetAssetPath().c_str(),
                texture.GetResolvedPath().c_str());

    // The field set is the schema's: every `inputs:mmd:material:<field>` of
    // the definition, except the texture slots this prim does not author, and
    // no property it merely authors under that prefix.
    const HdContainerDataSourceHandle fields = HdContainerDataSource::Cast(
        At(session, kFace, HdDataSourceLocator(kMmd, kMaterial)));
    assert(fields && "no mmd/material on a material with MmdMaterialAPI");
    size_t schemaFields = 0;
    for (const TfToken& name : definition->GetPropertyNames()) {
        if (name.GetString().compare(0, kPrefix.size(), kPrefix) == 0) {
            ++schemaFields;
        }
    }
    assert(schemaFields == 20 && "MATERIAL_POLICY.md §4.1 lists 20 fields");
    const TfTokenVector names = fields->GetNames();
    assert(names.size() == schemaFields - 2);
    for (const TfToken& name : names) {
        assert(fields->Get(name) && "a listed field has no data source");
    }
    assert(!fields->Get(TfToken("sphereTexture")) &&
           "an unauthored texture slot is exposed");
    assert(!fields->Get(TfToken("toonTexture")) &&
           "an unauthored texture slot is exposed");
    assert(!fields->Get(TfToken("notASchemaField")));

    // A prim that authors no slot lists none.
    const HdContainerDataSourceHandle bare = HdContainerDataSource::Cast(
        At(session, kNoProvenance, HdDataSourceLocator(kMmd, kMaterial)));
    assert(bare && bare->GetNames().size() == schemaFields - 3);

    // The realization beside it is UsdImaging's, untouched.
    assert(HdContainerDataSource::Get(face.dataSource,
                                      HdMaterialSchema::GetDefaultLocator()) &&
           "the material network is gone");

    // A material without the schema carries no MMD contribution.
    assert(!At(session, kPlain, HdDataSourceLocator(kMmd)));
    std::printf("canonical -> Hydra: %zu of %zu fields, authored and fallback\n",
                names.size(), schemaFields);
}

// ---------------------------------------------------------------------------
// Item 6: draw order
// ---------------------------------------------------------------------------
void
TestDrawOrderReachesHydra(const Session& session)
{
    const HdIntDataSourceHandle face =
        HdIntDataSource::Cast(At(session, kFace, DrawOrderLocator()));
    assert(face && face->GetTypedValue(0.0f) == 3);
    const HdIntDataSourceHandle skin =
        HdIntDataSource::Cast(At(session, kSkin, DrawOrderLocator()));
    assert(skin && skin->GetTypedValue(0.0f) == 0);
    assert(!At(session, kNoProvenance, DrawOrderLocator()) &&
           "a draw order without mmd:sourceIndex");

    const HdContainerDataSourceHandle mmd = HdContainerDataSource::Cast(
        At(session, kFace, HdDataSourceLocator(kMmd)));
    assert(mmd && mmd->GetNames().size() == 2);
    std::printf("draw order: from mmd:sourceIndex\n");
}

// ---------------------------------------------------------------------------
// Item 5: invalidation
// ---------------------------------------------------------------------------
void
TestAnEditDirtiesItsOwnLocator(Session& session)
{
    const struct {
        const char* field;
        VtValue value;
    } edits[] = {
        {"diffuseColor", VtValue(GfVec4f(0.1f, 0.2f, 0.3f, 1.0f))},
        {"specularPower", VtValue(3.0f)},
        {"edgeSize", VtValue(1.5f)},
        {"sharedToonIndex", VtValue(7)},
    };
    for (const auto& edit : edits) {
        session.recorder.Clear();
        assert(Attribute(session, kFace, kPrefix + edit.field).Set(edit.value));
        session.sceneIndex->ApplyPendingUpdates();

        // The one MMD locator -- not `mmd/material`, not `mmd` -- and the
        // whole `material`, which is not this plugin's. OpenUSD 26.08's
        // material prim dirties its entire network whenever any interface
        // input changes, connected or not, and every canonical value is an
        // interface input (§12.1). Pinned so a runtime that narrows it is
        // noticed: the network on this prim reads no canonical input, so the
        // narrow answer is the MMD locator alone.
        HdDataSourceLocatorSet expected;
        expected.insert(FieldLocator(edit.field));
        expected.insert(HdMaterialSchema::GetDefaultLocator());
        const HdDataSourceLocatorSet got = session.recorder.At(kFace);
        if (got != expected) {
            PrintLocators(edit.field, got);
        }
        assert(got == expected);
        assert(session.recorder.dirtied.size() == 1);
        assert(ValueAt(session, kFace, FieldLocator(edit.field)) == edit.value);
    }

    // A texture slot appearing: its own locator, and the name is listed.
    session.recorder.Clear();
    const UsdAttribute sphere = session.stage->GetPrimAtPath(SdfPath(kFace))
        .CreateAttribute(TfToken(kPrefix + "sphereTexture"),
                         SdfValueTypeNames->Asset);
    assert(sphere.Set(SdfAssetPath("textures/base.png")));
    session.sceneIndex->ApplyPendingUpdates();
    const HdDataSourceLocatorSet appeared = session.recorder.At(kFace);
    if (!appeared.Contains(FieldLocator("sphereTexture")) ||
        appeared.Intersects(HdDataSourceLocator(kMmd, TfToken("drawOrder")))) {
        PrintLocators("sphereTexture", appeared);
    }
    assert(appeared.Contains(FieldLocator("sphereTexture")));
    assert(!appeared.Contains(HdDataSourceLocator(kMmd, kMaterial)) &&
           "a slot appearing dirtied the whole contribution");
    assert(TypedAt<SdfAssetPath>(session, kFace, "sphereTexture")
               .GetAssetPath() == "textures/base.png");

    // Under the prefix, but not a field of the schema.
    session.recorder.Clear();
    assert(Attribute(session, kFace, kPrefix + "notASchemaField").Set(2.0f));
    session.sceneIndex->ApplyPendingUpdates();
    assert(!session.recorder.At(kFace).Intersects(HdDataSourceLocator(kMmd)));

    std::printf("invalidation: one locator per edit\n");
}

// ---------------------------------------------------------------------------
// A customData edit: dropped by UsdImaging, pinned
// ---------------------------------------------------------------------------
void
TestACustomDataEditReachesNoAdapter(Session& session)
{
    // UsdImagingStageSceneIndex resyncs a prim on a change of a *plugin*
    // metadata field and ignores a built-in one, and customData is built in.
    // So the edit reaches no adapter and dirties nothing: the draw order a
    // consumer holds is the one read when the prim was populated, until
    // something else resyncs it. Draw order is the material-table index, which
    // nothing edits after import; pinned so a runtime that starts forwarding
    // it is noticed.
    session.recorder.Clear();
    session.stage->GetPrimAtPath(SdfPath(kFace))
        .SetCustomDataByKey(TfToken("mmd:sourceIndex"), VtValue(9));
    session.sceneIndex->ApplyPendingUpdates();
    const HdDataSourceLocatorSet got = session.recorder.At(kFace);
    if (!got.IsEmpty()) {
        PrintLocators("customData", got);
    }
    assert(got.IsEmpty() && "a customData edit now reaches Hydra");
    std::printf("customData edit: not forwarded by UsdImaging\n");
}

// ---------------------------------------------------------------------------
// Item 4: time samples
// ---------------------------------------------------------------------------
void
TestATimeSampledFieldFollowsTime(Session& session)
{
    // Read first: UsdImaging records a field as time-varying when its data
    // source is built, so a field no consumer has pulled is never dirtied by
    // time. A renderer reads what it draws before the time moves.
    assert(Near(TypedAt<GfVec4f>(session, kSkin, "diffuseColor"),
                GfVec4f(1.0f, 1.0f, 1.0f, 1.0f)));
    TypedAt<float>(session, kFace, "specularPower");

    session.recorder.Clear();
    session.sceneIndex->SetTime(UsdTimeCode(10.0));
    HdDataSourceLocatorSet expected;
    expected.insert(FieldLocator("diffuseColor"));
    const HdDataSourceLocatorSet got = session.recorder.At(kSkin);
    if (got != expected) {
        PrintLocators("time", got);
    }
    assert(got == expected && "time dirtied more than the sampled field");
    assert(session.recorder.At(kFace).IsEmpty() &&
           "time dirtied a field that has no time samples");
    assert(Near(TypedAt<GfVec4f>(session, kSkin, "diffuseColor"),
                GfVec4f(0.5f, 0.25f, 0.0f, 0.5f)));

    session.sceneIndex->SetTime(UsdTimeCode(5.0));
    assert(Near(TypedAt<GfVec4f>(session, kSkin, "diffuseColor"),
                GfVec4f(0.75f, 0.625f, 0.5f, 0.75f)));
    std::printf("time samples: sampled, not copied\n");
}

// ---------------------------------------------------------------------------
// --without-schema: mmdSchema is not in the session
// ---------------------------------------------------------------------------
void
TestWithoutTheSchemaThereIsNoContribution(const std::string& fixture)
{
    assert(!Definition() &&
           "mmdSchema is registered after all, so this run measures nothing");
    // The adapter is registered; it is simply never asked, because no prim's
    // definition includes an API schema the registry does not know.
    TestTheAdapterIsDiscovered();

    Session session;
    Open(session, fixture);
    assert(!At(session, kFace, HdDataSourceLocator(kMmd)));
    assert(At(session, kFace, HdMaterialSchema::GetDefaultLocator()));
    std::printf("without mmdSchema: no mmd contribution\n");
}

} // namespace

int
main(int argc, char** argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <materials.usda> [--without-schema]\n",
                     argv[0]);
        return 2;
    }
    const std::string fixture = argv[1];
    if (argc > 2 && std::string(argv[2]) == "--without-schema") {
        TestWithoutTheSchemaThereIsNoContribution(fixture);
        std::printf("mmdImaging_material_without_schema: passed\n");
        return 0;
    }

    TestTheAdapterIsDiscovered();
    Session session;
    Open(session, fixture);
    TestCanonicalValuesReachHydra(session);
    TestDrawOrderReachesHydra(session);
    TestATimeSampledFieldFollowsTime(session);
    TestAnEditDirtiesItsOwnLocator(session);
    TestACustomDataEditReachesNoAdapter(session);
    std::printf("mmdImaging_material: passed\n");
    return 0;
}
