//
// Copyright 2016 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "./mmdMaterialAPI.h"
#include "pxr/usd/usd/schemaRegistry.h"
#include "pxr/usd/usd/typed.h"

#include "pxr/usd/sdf/types.h"
#include "pxr/usd/sdf/assetPath.h"

PXR_NAMESPACE_OPEN_SCOPE

// Register the schema with the TfType system.
TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<UsdMmdMaterialAPI,
        TfType::Bases< UsdAPISchemaBase > >();
    
}

/* virtual */
UsdMmdMaterialAPI::~UsdMmdMaterialAPI()
{
}

/* static */
UsdMmdMaterialAPI
UsdMmdMaterialAPI::Get(const UsdStagePtr &stage, const SdfPath &path)
{
    if (!stage) {
        TF_CODING_ERROR("Invalid stage");
        return UsdMmdMaterialAPI();
    }
    return UsdMmdMaterialAPI(stage->GetPrimAtPath(path));
}


/* virtual */
UsdSchemaKind UsdMmdMaterialAPI::_GetSchemaKind() const
{
    return UsdMmdMaterialAPI::schemaKind;
}

/* static */
bool
UsdMmdMaterialAPI::CanApply(
    const UsdPrim &prim, std::string *whyNot)
{
    return prim.CanApplyAPI<UsdMmdMaterialAPI>(whyNot);
}

/* static */
UsdMmdMaterialAPI
UsdMmdMaterialAPI::Apply(const UsdPrim &prim)
{
    if (prim.ApplyAPI<UsdMmdMaterialAPI>()) {
        return UsdMmdMaterialAPI(prim);
    }
    return UsdMmdMaterialAPI();
}

/* static */
const TfType &
UsdMmdMaterialAPI::_GetStaticTfType()
{
    static TfType tfType = TfType::Find<UsdMmdMaterialAPI>();
    return tfType;
}

/* static */
bool 
UsdMmdMaterialAPI::_IsTypedSchema()
{
    static bool isTyped = _GetStaticTfType().IsA<UsdTyped>();
    return isTyped;
}

/* virtual */
const TfType &
UsdMmdMaterialAPI::_GetTfType() const
{
    return _GetStaticTfType();
}

UsdAttribute
UsdMmdMaterialAPI::GetDiffuseColorAttr() const
{
    return GetPrim().GetAttribute(UsdMmdTokens->inputsMmdMaterialDiffuseColor);
}

UsdAttribute
UsdMmdMaterialAPI::CreateDiffuseColorAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdMmdTokens->inputsMmdMaterialDiffuseColor,
                       SdfValueTypeNames->Color4f,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdMmdMaterialAPI::GetSpecularColorAttr() const
{
    return GetPrim().GetAttribute(UsdMmdTokens->inputsMmdMaterialSpecularColor);
}

UsdAttribute
UsdMmdMaterialAPI::CreateSpecularColorAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdMmdTokens->inputsMmdMaterialSpecularColor,
                       SdfValueTypeNames->Color3f,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdMmdMaterialAPI::GetSpecularPowerAttr() const
{
    return GetPrim().GetAttribute(UsdMmdTokens->inputsMmdMaterialSpecularPower);
}

UsdAttribute
UsdMmdMaterialAPI::CreateSpecularPowerAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdMmdTokens->inputsMmdMaterialSpecularPower,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdMmdMaterialAPI::GetAmbientColorAttr() const
{
    return GetPrim().GetAttribute(UsdMmdTokens->inputsMmdMaterialAmbientColor);
}

UsdAttribute
UsdMmdMaterialAPI::CreateAmbientColorAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdMmdTokens->inputsMmdMaterialAmbientColor,
                       SdfValueTypeNames->Color3f,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdMmdMaterialAPI::GetDoubleSidedAttr() const
{
    return GetPrim().GetAttribute(UsdMmdTokens->inputsMmdMaterialDoubleSided);
}

UsdAttribute
UsdMmdMaterialAPI::CreateDoubleSidedAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdMmdTokens->inputsMmdMaterialDoubleSided,
                       SdfValueTypeNames->Bool,
                       /* custom = */ false,
                       SdfVariabilityUniform,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdMmdMaterialAPI::GetGroundShadowAttr() const
{
    return GetPrim().GetAttribute(UsdMmdTokens->inputsMmdMaterialGroundShadow);
}

UsdAttribute
UsdMmdMaterialAPI::CreateGroundShadowAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdMmdTokens->inputsMmdMaterialGroundShadow,
                       SdfValueTypeNames->Bool,
                       /* custom = */ false,
                       SdfVariabilityUniform,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdMmdMaterialAPI::GetCastSelfShadowAttr() const
{
    return GetPrim().GetAttribute(UsdMmdTokens->inputsMmdMaterialCastSelfShadow);
}

UsdAttribute
UsdMmdMaterialAPI::CreateCastSelfShadowAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdMmdTokens->inputsMmdMaterialCastSelfShadow,
                       SdfValueTypeNames->Bool,
                       /* custom = */ false,
                       SdfVariabilityUniform,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdMmdMaterialAPI::GetReceiveSelfShadowAttr() const
{
    return GetPrim().GetAttribute(UsdMmdTokens->inputsMmdMaterialReceiveSelfShadow);
}

UsdAttribute
UsdMmdMaterialAPI::CreateReceiveSelfShadowAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdMmdTokens->inputsMmdMaterialReceiveSelfShadow,
                       SdfValueTypeNames->Bool,
                       /* custom = */ false,
                       SdfVariabilityUniform,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdMmdMaterialAPI::GetDrawEdgeAttr() const
{
    return GetPrim().GetAttribute(UsdMmdTokens->inputsMmdMaterialDrawEdge);
}

UsdAttribute
UsdMmdMaterialAPI::CreateDrawEdgeAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdMmdTokens->inputsMmdMaterialDrawEdge,
                       SdfValueTypeNames->Bool,
                       /* custom = */ false,
                       SdfVariabilityUniform,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdMmdMaterialAPI::GetVertexColorAttr() const
{
    return GetPrim().GetAttribute(UsdMmdTokens->inputsMmdMaterialVertexColor);
}

UsdAttribute
UsdMmdMaterialAPI::CreateVertexColorAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdMmdTokens->inputsMmdMaterialVertexColor,
                       SdfValueTypeNames->Bool,
                       /* custom = */ false,
                       SdfVariabilityUniform,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdMmdMaterialAPI::GetDrawPointsAttr() const
{
    return GetPrim().GetAttribute(UsdMmdTokens->inputsMmdMaterialDrawPoints);
}

UsdAttribute
UsdMmdMaterialAPI::CreateDrawPointsAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdMmdTokens->inputsMmdMaterialDrawPoints,
                       SdfValueTypeNames->Bool,
                       /* custom = */ false,
                       SdfVariabilityUniform,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdMmdMaterialAPI::GetDrawLinesAttr() const
{
    return GetPrim().GetAttribute(UsdMmdTokens->inputsMmdMaterialDrawLines);
}

UsdAttribute
UsdMmdMaterialAPI::CreateDrawLinesAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdMmdTokens->inputsMmdMaterialDrawLines,
                       SdfValueTypeNames->Bool,
                       /* custom = */ false,
                       SdfVariabilityUniform,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdMmdMaterialAPI::GetEdgeColorAttr() const
{
    return GetPrim().GetAttribute(UsdMmdTokens->inputsMmdMaterialEdgeColor);
}

UsdAttribute
UsdMmdMaterialAPI::CreateEdgeColorAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdMmdTokens->inputsMmdMaterialEdgeColor,
                       SdfValueTypeNames->Color4f,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdMmdMaterialAPI::GetEdgeSizeAttr() const
{
    return GetPrim().GetAttribute(UsdMmdTokens->inputsMmdMaterialEdgeSize);
}

UsdAttribute
UsdMmdMaterialAPI::CreateEdgeSizeAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdMmdTokens->inputsMmdMaterialEdgeSize,
                       SdfValueTypeNames->Float,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdMmdMaterialAPI::GetTextureAttr() const
{
    return GetPrim().GetAttribute(UsdMmdTokens->inputsMmdMaterialTexture);
}

UsdAttribute
UsdMmdMaterialAPI::CreateTextureAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdMmdTokens->inputsMmdMaterialTexture,
                       SdfValueTypeNames->Asset,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdMmdMaterialAPI::GetSphereTextureAttr() const
{
    return GetPrim().GetAttribute(UsdMmdTokens->inputsMmdMaterialSphereTexture);
}

UsdAttribute
UsdMmdMaterialAPI::CreateSphereTextureAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdMmdTokens->inputsMmdMaterialSphereTexture,
                       SdfValueTypeNames->Asset,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdMmdMaterialAPI::GetSphereModeAttr() const
{
    return GetPrim().GetAttribute(UsdMmdTokens->inputsMmdMaterialSphereMode);
}

UsdAttribute
UsdMmdMaterialAPI::CreateSphereModeAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdMmdTokens->inputsMmdMaterialSphereMode,
                       SdfValueTypeNames->Token,
                       /* custom = */ false,
                       SdfVariabilityUniform,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdMmdMaterialAPI::GetToonSourceAttr() const
{
    return GetPrim().GetAttribute(UsdMmdTokens->inputsMmdMaterialToonSource);
}

UsdAttribute
UsdMmdMaterialAPI::CreateToonSourceAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdMmdTokens->inputsMmdMaterialToonSource,
                       SdfValueTypeNames->Token,
                       /* custom = */ false,
                       SdfVariabilityUniform,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdMmdMaterialAPI::GetToonTextureAttr() const
{
    return GetPrim().GetAttribute(UsdMmdTokens->inputsMmdMaterialToonTexture);
}

UsdAttribute
UsdMmdMaterialAPI::CreateToonTextureAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdMmdTokens->inputsMmdMaterialToonTexture,
                       SdfValueTypeNames->Asset,
                       /* custom = */ false,
                       SdfVariabilityVarying,
                       defaultValue,
                       writeSparsely);
}

UsdAttribute
UsdMmdMaterialAPI::GetSharedToonIndexAttr() const
{
    return GetPrim().GetAttribute(UsdMmdTokens->inputsMmdMaterialSharedToonIndex);
}

UsdAttribute
UsdMmdMaterialAPI::CreateSharedToonIndexAttr(VtValue const &defaultValue, bool writeSparsely) const
{
    return UsdSchemaBase::_CreateAttr(UsdMmdTokens->inputsMmdMaterialSharedToonIndex,
                       SdfValueTypeNames->Int,
                       /* custom = */ false,
                       SdfVariabilityUniform,
                       defaultValue,
                       writeSparsely);
}

namespace {
static inline TfTokenVector
_ConcatenateAttributeNames(const TfTokenVector& left,const TfTokenVector& right)
{
    TfTokenVector result;
    result.reserve(left.size() + right.size());
    result.insert(result.end(), left.begin(), left.end());
    result.insert(result.end(), right.begin(), right.end());
    return result;
}
}

/*static*/
const TfTokenVector&
UsdMmdMaterialAPI::GetSchemaAttributeNames(bool includeInherited)
{
    static TfTokenVector localNames = {
        UsdMmdTokens->inputsMmdMaterialDiffuseColor,
        UsdMmdTokens->inputsMmdMaterialSpecularColor,
        UsdMmdTokens->inputsMmdMaterialSpecularPower,
        UsdMmdTokens->inputsMmdMaterialAmbientColor,
        UsdMmdTokens->inputsMmdMaterialDoubleSided,
        UsdMmdTokens->inputsMmdMaterialGroundShadow,
        UsdMmdTokens->inputsMmdMaterialCastSelfShadow,
        UsdMmdTokens->inputsMmdMaterialReceiveSelfShadow,
        UsdMmdTokens->inputsMmdMaterialDrawEdge,
        UsdMmdTokens->inputsMmdMaterialVertexColor,
        UsdMmdTokens->inputsMmdMaterialDrawPoints,
        UsdMmdTokens->inputsMmdMaterialDrawLines,
        UsdMmdTokens->inputsMmdMaterialEdgeColor,
        UsdMmdTokens->inputsMmdMaterialEdgeSize,
        UsdMmdTokens->inputsMmdMaterialTexture,
        UsdMmdTokens->inputsMmdMaterialSphereTexture,
        UsdMmdTokens->inputsMmdMaterialSphereMode,
        UsdMmdTokens->inputsMmdMaterialToonSource,
        UsdMmdTokens->inputsMmdMaterialToonTexture,
        UsdMmdTokens->inputsMmdMaterialSharedToonIndex,
    };
    static TfTokenVector allNames =
        _ConcatenateAttributeNames(
            UsdAPISchemaBase::GetSchemaAttributeNames(true),
            localNames);

    if (includeInherited)
        return allNames;
    else
        return localNames;
}

PXR_NAMESPACE_CLOSE_SCOPE

// ===================================================================== //
// Feel free to add custom code below this line. It will be preserved by
// the code generator.
//
// Just remember to wrap code in the appropriate delimiters:
// 'PXR_NAMESPACE_OPEN_SCOPE', 'PXR_NAMESPACE_CLOSE_SCOPE'.
// ===================================================================== //
// --(BEGIN CUSTOM CODE)--
