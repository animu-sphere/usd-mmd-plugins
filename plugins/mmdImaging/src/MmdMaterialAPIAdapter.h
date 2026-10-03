// SPDX-License-Identifier: Apache-2.0
//
// The UsdImaging API-schema adapter for `MmdMaterialAPI`: the canonical MMD
// material values as Hydra data on the material prim, under
// `mmd/material/<field>`, and the material's draw order as `mmd/drawOrder`
// (docs/design/MATERIAL_POLICY.md §12.1).
//
// The field names are derived, never listed: `inputs:mmd:material:<field>` of
// the registered definition is `mmd/material/<field>`. mmdSchema is not
// linked; the definition is read from the schema registry by the schema's
// name, which is what mmdSchema's plugInfo.json registers.
#ifndef USD_MMD_IMAGING_MATERIAL_API_ADAPTER_H
#define USD_MMD_IMAGING_MATERIAL_API_ADAPTER_H

#include <pxr/pxr.h>
#include <pxr/usdImaging/usdImaging/apiSchemaAdapter.h>

#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

class UsdMmdImagingMaterialAPIAdapter : public UsdImagingAPISchemaAdapter
{
public:
    using BaseAdapter = UsdImagingAPISchemaAdapter;

    UsdMmdImagingMaterialAPIAdapter();

    HdContainerDataSourceHandle GetImagingSubprimData(
        UsdPrim const& prim,
        TfToken const& subprim,
        TfToken const& appliedInstanceName,
        const UsdImagingDataSourceStageGlobals& stageGlobals) override;

    HdDataSourceLocatorSet InvalidateImagingSubprim(
        UsdPrim const& prim,
        TfToken const& subprim,
        TfToken const& appliedInstanceName,
        TfTokenVector const& properties,
        UsdImagingPropertyInvalidationType invalidationType) override;

    // The schema's fields, shared with every data source the adapter hands
    // out, which may outlive it.
    struct Schema;

private:
    std::shared_ptr<const Schema> _schema;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
