// SPDX-License-Identifier: Apache-2.0
//
// The installed mmdSchema, as a consumer outside the repository uses it: the
// generated accessors from the package, and the schema definition from the
// prefix's plugInfo.json. An unauthored property reads its fallback only
// through the registered definition, so the fallback proves registration.

#include <mmdSchema/mmdMaterialAPI.h>
#include <mmdSchema/tokens.h>

#include <pxr/usd/usd/stage.h>

#include <cstdio>

PXR_NAMESPACE_USING_DIRECTIVE

int
main()
{
    const UsdStageRefPtr stage = UsdStage::CreateInMemory();
    const UsdPrim prim = stage->DefinePrim(SdfPath("/m"), TfToken("Material"));
    if (!UsdMmdMaterialAPI::CanApply(prim)) {
        std::printf("schema=unregistered\n");
        return 1;
    }
    const UsdMmdMaterialAPI api = UsdMmdMaterialAPI::Apply(prim);
    TfToken sphereMode;
    api.GetSphereModeAttr().Get(&sphereMode);
    std::printf("schema=%s inputs=%zu sphereMode=%s\n",
                UsdMmdTokens->MmdMaterialAPI.GetText(),
                UsdMmdMaterialAPI::GetSchemaAttributeNames(false).size(),
                sphereMode.GetText());
    return 0;
}
