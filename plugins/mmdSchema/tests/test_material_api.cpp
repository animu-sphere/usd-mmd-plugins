// SPDX-License-Identifier: Apache-2.0
//
// MmdMaterialAPI through its generated C++ accessors, on the hand-authored
// fixture (docs/design/MATERIAL_POLICY.md §4.3): what an MMD-aware consumer
// such as the UsdImaging adapter links against.
//
//   mmdschema_material_api <fixtures/basic.usda>

#include <mmdSchema/mmdMaterialAPI.h>
#include <mmdSchema/tokens.h>

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdShade/material.h>

#include <cassert>
#include <cstdio>
#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

template <typename T>
T
Read(const UsdAttribute& attribute)
{
    T value{};
    const bool ok = attribute.Get(&value);
    assert(ok);
    (void)ok;
    return value;
}

void
AuthoredValues(const UsdStageRefPtr& stage)
{
    const UsdMmdMaterialAPI api(stage->GetPrimAtPath(SdfPath("/Asset/mtl/body")));
    assert(api);
    assert(Read<GfVec4f>(api.GetDiffuseColorAttr()) == GfVec4f(0.8f, 0.7f, 0.6f, 1.0f));
    assert(Read<float>(api.GetSpecularPowerAttr()) == 8.0f);
    assert(Read<bool>(api.GetDrawEdgeAttr()));
    assert(!Read<bool>(api.GetVertexColorAttr()));
    assert(Read<GfVec4f>(api.GetEdgeColorAttr()) == GfVec4f(0.0f, 0.0f, 0.0f, 1.0f));
    assert(Read<TfToken>(api.GetSphereModeAttr()) == UsdMmdTokens->multiply);
    assert(Read<TfToken>(api.GetToonSourceAttr()) == UsdMmdTokens->shared);
    assert(Read<int>(api.GetSharedToonIndexAttr()) == 3);

    // The accessors name the Material's own interface inputs.
    const UsdShadeMaterial material(api.GetPrim());
    assert(api.GetDiffuseColorAttr() ==
           material.GetInput(TfToken("mmd:material:diffuseColor")).GetAttr());
}

void
Fallbacks(const UsdStageRefPtr& stage)
{
    const UsdMmdMaterialAPI api(stage->GetPrimAtPath(SdfPath("/Asset/mtl/unauthored")));
    assert(api);
    assert(!api.GetDiffuseColorAttr().HasAuthoredValue());
    assert(Read<GfVec4f>(api.GetDiffuseColorAttr()) == GfVec4f(1.0f, 1.0f, 1.0f, 1.0f));
    assert(Read<GfVec3f>(api.GetAmbientColorAttr()) == GfVec3f(0.0f, 0.0f, 0.0f));
    assert(Read<float>(api.GetEdgeSizeAttr()) == 0.0f);
    assert(!Read<bool>(api.GetDoubleSidedAttr()));
    assert(Read<SdfAssetPath>(api.GetTextureAttr()).GetAssetPath().empty());
    assert(Read<TfToken>(api.GetSphereModeAttr()) == UsdMmdTokens->disabled);
    assert(Read<TfToken>(api.GetToonSourceAttr()) == UsdMmdTokens->none);
    assert(Read<int>(api.GetSharedToonIndexAttr()) == -1);
}

void
ApplyAndAuthor()
{
    const UsdStageRefPtr stage = UsdStage::CreateInMemory();
    const UsdPrim material = UsdShadeMaterial::Define(stage, SdfPath("/mtl/m")).GetPrim();
    const UsdPrim xform = stage->DefinePrim(SdfPath("/x"), TfToken("Xform"));
    assert(UsdMmdMaterialAPI::CanApply(material));
    assert(!UsdMmdMaterialAPI::CanApply(xform));

    const UsdMmdMaterialAPI api = UsdMmdMaterialAPI::Apply(material);
    assert(api && material.HasAPI<UsdMmdMaterialAPI>());
    api.CreateDiffuseColorAttr(VtValue(GfVec4f(0.25f, 0.5f, 0.75f, 0.5f)));
    api.CreateToonSourceAttr(VtValue(UsdMmdTokens->individual));
    assert(Read<GfVec4f>(api.GetDiffuseColorAttr()) == GfVec4f(0.25f, 0.5f, 0.75f, 0.5f));
    assert(Read<TfToken>(api.GetToonSourceAttr()) == UsdMmdTokens->individual);
    assert(api.GetDiffuseColorAttr().GetVariability() == SdfVariabilityVarying);
    assert(api.GetToonSourceAttr().GetVariability() == SdfVariabilityUniform);
    assert(!api.GetDiffuseColorAttr().IsCustom());
}

} // namespace

int
main(int argc, char** argv)
{
    if (argc != 2) {
        std::fprintf(stderr, "usage: mmdschema_material_api <fixtures/basic.usda>\n");
        return 2;
    }
    const UsdStageRefPtr stage = UsdStage::Open(std::string(argv[1]));
    if (!stage) {
        std::fprintf(stderr, "cannot open %s\n", argv[1]);
        return 1;
    }
    AuthoredValues(stage);
    Fallbacks(stage);
    ApplyAndAuthor();
    std::printf("mmdschema_material_api: ok\n");
    return 0;
}
