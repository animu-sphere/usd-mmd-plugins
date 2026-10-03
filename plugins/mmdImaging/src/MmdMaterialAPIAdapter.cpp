// SPDX-License-Identifier: Apache-2.0
//
// `MmdMaterialAPI` as Hydra data (docs/design/MATERIAL_POLICY.md §12.1).
//
// The contribution sits on the material prim, beside UsdImaging's own
// `material` container and never inside it, so `/preview` and `/mtlx` reach
// Hydra as the importer connected them. It takes the shape usd-vrm-plugins'
// vrmImaging froze for `vrm/<group>/<field>`, so a renderer reads MMD and
// MToon by one path.
#include "MmdMaterialAPIAdapter.h"

#include <pxr/base/tf/type.h>
#include <pxr/base/vt/value.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdf/valueTypeName.h>
#include <pxr/usd/usd/primDefinition.h>
#include <pxr/usd/usd/schemaRegistry.h>
#include <pxr/usdImaging/usdImaging/dataSourceAttribute.h>
#include <pxr/usdImaging/usdImaging/dataSourceStageGlobals.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

TF_REGISTRY_FUNCTION(TfType)
{
    using Adapter = UsdMmdImagingMaterialAPIAdapter;
    TfType t = TfType::Define<Adapter, TfType::Bases<Adapter::BaseAdapter>>();
    t.SetFactory<UsdImagingAPISchemaAdapterFactory<Adapter>>();
}

namespace {

const TfToken& _MmdToken()
{
    static const TfToken mmd("mmd");
    return mmd;
}

const TfToken& _MaterialToken()
{
    static const TfToken material("material");
    return material;
}

const TfToken& _DrawOrderToken()
{
    static const TfToken drawOrder("drawOrder");
    return drawOrder;
}

// The provenance key draw order is read from (MATERIAL_POLICY.md §4.2, §10):
// the one value this adapter reads by name.
const TfToken& _SourceIndexKey()
{
    static const TfToken key("mmd:sourceIndex");
    return key;
}

const std::string& _Prefix()
{
    static const std::string prefix("inputs:mmd:material:");
    return prefix;
}

bool _StartsWith(const std::string& s, const std::string& prefix)
{
    return s.compare(0, prefix.size(), prefix) == 0;
}

} // namespace

struct UsdMmdImagingMaterialAPIAdapter::Schema
{
    // The definition's fields, sorted.
    TfTokenVector fields;
    // The asset-valued ones, sorted: listed only when authored, so the
    // schema's empty fallback never reads as a texture (§12.1 item 3).
    TfTokenVector assetFields;

    bool IsField(const TfToken& field) const
    {
        return std::binary_search(fields.begin(), fields.end(), field);
    }

    bool IsAssetField(const TfToken& field) const
    {
        return std::binary_search(assetFields.begin(), assetFields.end(), field);
    }
};

namespace {

using Schema = UsdMmdImagingMaterialAPIAdapter::Schema;

HdDataSourceLocator _FieldLocator(const TfToken& field)
{
    return HdDataSourceLocator(_MmdToken(), _MaterialToken(), field);
}

// A container of named children, which is all `mmd` is.
// HdRetainedContainerDataSource would do, but hd/retainedDataSource.h does not
// compile as C++20 under GCC 13 -- OpenUSD 26.08 declares a constructor there
// as `HdRetainedTypedSampledDataSource<bool>(...)`, a template-id C++20 no
// longer accepts in that position -- as usd-vrm-plugins measured.
class _NamedContainer : public HdContainerDataSource
{
public:
    HD_DECLARE_DATASOURCE(_NamedContainer);

    TfTokenVector GetNames() override
    {
        TfTokenVector names;
        for (const auto& entry : _children) {
            names.push_back(entry.first);
        }
        return names;
    }

    HdDataSourceBaseHandle Get(const TfToken& name) override
    {
        for (const auto& entry : _children) {
            if (entry.first == name) {
                return entry.second;
            }
        }
        return nullptr;
    }

private:
    explicit _NamedContainer(
        std::vector<std::pair<TfToken, HdDataSourceBaseHandle>> children)
        : _children(std::move(children))
    {
    }

    std::vector<std::pair<TfToken, HdDataSourceBaseHandle>> _children;
};

// An int that does not vary: the draw order, which is metadata and has no
// time samples. Typed, so a consumer casts it to HdIntDataSource.
class _ConstantInt : public HdTypedSampledDataSource<int>
{
public:
    HD_DECLARE_DATASOURCE(_ConstantInt);

    VtValue GetValue(Time) override { return VtValue(_value); }
    int GetTypedValue(Time) override { return _value; }

    bool GetContributingSampleTimesForInterval(
        Time, Time, std::vector<Time>*) override
    {
        return false;
    }

private:
    explicit _ConstantInt(int value) : _value(value) {}

    int _value;
};

// `mmd/material`: each field the attribute's resolved value at the scene
// index's time, the schema fallback included, so a consumer never restates
// the schema's defaults. Built lazily, and sampled rather than copied, so a
// time-sampled attribute stays time-sampled (§12.1 items 2, 4).
class _FieldsDataSource : public HdContainerDataSource
{
public:
    HD_DECLARE_DATASOURCE(_FieldsDataSource);

    TfTokenVector GetNames() override
    {
        TfTokenVector names;
        for (const TfToken& field : _schema->fields) {
            if (_Resolves(field)) {
                names.push_back(field);
            }
        }
        return names;
    }

    HdDataSourceBaseHandle Get(const TfToken& name) override
    {
        if (!_schema->IsField(name) || !_Resolves(name)) {
            return nullptr;
        }
        // An asset-valued field comes back as UsdImaging's asset-path data
        // source, authored and resolved path both, with nothing loaded.
        return UsdImagingDataSourceAttributeNew(
            _Attribute(name), _stageGlobals, _prim.GetPath(),
            _FieldLocator(name));
    }

private:
    _FieldsDataSource(const std::shared_ptr<const Schema>& schema,
                      const UsdPrim& prim,
                      const UsdImagingDataSourceStageGlobals& stageGlobals)
        : _schema(schema), _prim(prim), _stageGlobals(stageGlobals)
    {
    }

    UsdAttribute _Attribute(const TfToken& field) const
    {
        return _prim.GetAttribute(TfToken(_Prefix() + field.GetString()));
    }

    // A texture slot only when authored; every other field whenever it has a
    // value, which the schema fallback gives it.
    bool _Resolves(const TfToken& field) const
    {
        const UsdAttribute attribute = _Attribute(field);
        if (!attribute) {
            return false;
        }
        return _schema->IsAssetField(field) ? attribute.HasAuthoredValue()
                                            : attribute.HasValue();
    }

    std::shared_ptr<const Schema> _schema;
    UsdPrim _prim;
    const UsdImagingDataSourceStageGlobals& _stageGlobals;
};

std::shared_ptr<const Schema> _ReadSchema()
{
    auto schema = std::make_shared<Schema>();
    const UsdPrimDefinition* definition =
        UsdSchemaRegistry::GetInstance().FindAppliedAPIPrimDefinition(
            TfToken("MmdMaterialAPI"));
    if (definition) {
        for (const TfToken& name : definition->GetPropertyNames()) {
            if (!_StartsWith(name.GetString(), _Prefix())) {
                continue;
            }
            const TfToken field(name.GetString().substr(_Prefix().size()));
            schema->fields.push_back(field);
            const UsdPrimDefinition::Attribute attribute =
                definition->GetAttributeDefinition(name);
            if (attribute &&
                attribute.GetTypeName().GetScalarType() ==
                    SdfValueTypeNames->Asset) {
                schema->assetFields.push_back(field);
            }
        }
    }
    std::sort(schema->fields.begin(), schema->fields.end());
    std::sort(schema->assetFields.begin(), schema->assetFields.end());
    return schema;
}

} // namespace

UsdMmdImagingMaterialAPIAdapter::UsdMmdImagingMaterialAPIAdapter()
    : _schema(_ReadSchema())
{
}

HdContainerDataSourceHandle
UsdMmdImagingMaterialAPIAdapter::GetImagingSubprimData(
    UsdPrim const& prim,
    TfToken const& subprim,
    TfToken const& appliedInstanceName,
    const UsdImagingDataSourceStageGlobals& stageGlobals)
{
    // A contribution to the material prim itself: no child Hydra prim. The
    // schema is single-apply, so an instance name is never this adapter's.
    if (!subprim.IsEmpty() || !appliedInstanceName.IsEmpty()) {
        return nullptr;
    }
    std::vector<std::pair<TfToken, HdDataSourceBaseHandle>> children;
    children.emplace_back(
        _MaterialToken(),
        _FieldsDataSource::New(_schema, prim, stageGlobals));
    // Provenance stays outside the API, but customData never reaches Hydra,
    // and alpha-blended MMD rendering needs the order (§12.1 item 6).
    const VtValue sourceIndex = prim.GetCustomDataByKey(_SourceIndexKey());
    if (sourceIndex.IsHolding<int>()) {
        children.emplace_back(
            _DrawOrderToken(),
            _ConstantInt::New(sourceIndex.UncheckedGet<int>()));
    }
    std::vector<std::pair<TfToken, HdDataSourceBaseHandle>> root;
    root.emplace_back(_MmdToken(), _NamedContainer::New(std::move(children)));
    return _NamedContainer::New(std::move(root));
}

HdDataSourceLocatorSet
UsdMmdImagingMaterialAPIAdapter::InvalidateImagingSubprim(
    UsdPrim const& prim,
    TfToken const& subprim,
    TfToken const& appliedInstanceName,
    TfTokenVector const& properties,
    const UsdImagingPropertyInvalidationType invalidationType)
{
    HdDataSourceLocatorSet result;
    if (!subprim.IsEmpty() || !appliedInstanceName.IsEmpty()) {
        return result;
    }
    // One locator per changed field, never the whole contribution (§12.1
    // item 5). An authored value appearing or disappearing (a resync) dirties
    // the same locator as a value changing: whether a name is listed follows
    // from its value, so the one locator covers both.
    for (const TfToken& property : properties) {
        if (!_StartsWith(property.GetString(), _Prefix())) {
            continue;
        }
        const TfToken field(property.GetString().substr(_Prefix().size()));
        if (_schema->IsField(field)) {
            result.insert(_FieldLocator(field));
        }
    }
    return result;
}

PXR_NAMESPACE_CLOSE_SCOPE
