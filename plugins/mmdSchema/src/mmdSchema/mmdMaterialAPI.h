//
// Copyright 2016 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef MMDSCHEMA_GENERATED_MMDMATERIALAPI_H
#define MMDSCHEMA_GENERATED_MMDMATERIALAPI_H

/// \file mmdSchema/mmdMaterialAPI.h

#include "pxr/pxr.h"
#include "./api.h"
#include "pxr/usd/usd/apiSchemaBase.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/stage.h"
#include "./tokens.h"

#include "pxr/base/vt/value.h"

#include "pxr/base/gf/vec3d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/gf/matrix4d.h"

#include "pxr/base/tf/token.h"
#include "pxr/base/tf/type.h"

PXR_NAMESPACE_OPEN_SCOPE

class SdfAssetPath;

// -------------------------------------------------------------------------- //
// MMDMATERIALAPI                                                             //
// -------------------------------------------------------------------------- //

/// \class UsdMmdMaterialAPI
///
/// The canonical semantics of one PMX material. Apply to the
/// /Asset/mtl/<materialId> UsdShadeMaterial. It declares the properties the
/// importer authors there; it adds no child and no realization, and bindings
/// and render-context outputs keep targeting the Material. Every property is
/// a Material interface input, because UsdShade connects nothing else: the
/// /preview and /mtlx graphs connect to these values and never store them.
/// Values a material morph modulates are varying, so a runtime override
/// reaches every connected realization.
/// 
/// Colors are authored as PMX stores them, with no declared color space.
/// Fallbacks are neutral values for robust reads; the importer authors every
/// scalar and flag a PMX material has, and an asset slot only when the source
/// names a safe asset path. Provenance (source names, index, memo and the
/// verbatim texture strings) stays in the prim's customData, outside this
/// API. MATERIAL_POLICY.md s4.1 is the property inventory.
///
/// For any described attribute \em Fallback \em Value or \em Allowed \em Values below
/// that are text/tokens, the actual token is published and defined in \ref UsdMmdTokens.
/// So to set an attribute to the value "rightHanded", use UsdMmdTokens->rightHanded
/// as the value.
///
class UsdMmdMaterialAPI : public UsdAPISchemaBase
{
public:
    /// Compile time constant representing what kind of schema this class is.
    ///
    /// \sa UsdSchemaKind
    static const UsdSchemaKind schemaKind = UsdSchemaKind::SingleApplyAPI;

    /// Construct a UsdMmdMaterialAPI on UsdPrim \p prim .
    /// Equivalent to UsdMmdMaterialAPI::Get(prim.GetStage(), prim.GetPath())
    /// for a \em valid \p prim, but will not immediately throw an error for
    /// an invalid \p prim
    explicit UsdMmdMaterialAPI(const UsdPrim& prim=UsdPrim())
        : UsdAPISchemaBase(prim)
    {
    }

    /// Construct a UsdMmdMaterialAPI on the prim held by \p schemaObj .
    /// Should be preferred over UsdMmdMaterialAPI(schemaObj.GetPrim()),
    /// as it preserves SchemaBase state.
    explicit UsdMmdMaterialAPI(const UsdSchemaBase& schemaObj)
        : UsdAPISchemaBase(schemaObj)
    {
    }

    /// Destructor.
    MMDSCHEMA_API
    virtual ~UsdMmdMaterialAPI();

    /// Return a vector of names of all pre-declared attributes for this schema
    /// class and all its ancestor classes.  Does not include attributes that
    /// may be authored by custom/extended methods of the schemas involved.
    MMDSCHEMA_API
    static const TfTokenVector &
    GetSchemaAttributeNames(bool includeInherited=true);

    /// Return a UsdMmdMaterialAPI holding the prim adhering to this
    /// schema at \p path on \p stage.  If no prim exists at \p path on
    /// \p stage, or if the prim at that path does not adhere to this schema,
    /// return an invalid schema object.  This is shorthand for the following:
    ///
    /// \code
    /// UsdMmdMaterialAPI(stage->GetPrimAtPath(path));
    /// \endcode
    ///
    MMDSCHEMA_API
    static UsdMmdMaterialAPI
    Get(const UsdStagePtr &stage, const SdfPath &path);


    /// Returns true if this <b>single-apply</b> API schema can be applied to 
    /// the given \p prim. If this schema can not be a applied to the prim, 
    /// this returns false and, if provided, populates \p whyNot with the 
    /// reason it can not be applied.
    /// 
    /// Note that if CanApply returns false, that does not necessarily imply
    /// that calling Apply will fail. Callers are expected to call CanApply
    /// before calling Apply if they want to ensure that it is valid to 
    /// apply a schema.
    /// 
    /// \sa UsdPrim::GetAppliedSchemas()
    /// \sa UsdPrim::HasAPI()
    /// \sa UsdPrim::CanApplyAPI()
    /// \sa UsdPrim::ApplyAPI()
    /// \sa UsdPrim::RemoveAPI()
    ///
    MMDSCHEMA_API
    static bool 
    CanApply(const UsdPrim &prim, std::string *whyNot=nullptr);

    /// Applies this <b>single-apply</b> API schema to the given \p prim.
    /// This information is stored by adding "MmdMaterialAPI" to the 
    /// token-valued, listOp metadata \em apiSchemas on the prim.
    /// 
    /// \return A valid UsdMmdMaterialAPI object is returned upon success. 
    /// An invalid (or empty) UsdMmdMaterialAPI object is returned upon 
    /// failure. See \ref UsdPrim::ApplyAPI() for conditions 
    /// resulting in failure. 
    /// 
    /// \sa UsdPrim::GetAppliedSchemas()
    /// \sa UsdPrim::HasAPI()
    /// \sa UsdPrim::CanApplyAPI()
    /// \sa UsdPrim::ApplyAPI()
    /// \sa UsdPrim::RemoveAPI()
    ///
    MMDSCHEMA_API
    static UsdMmdMaterialAPI 
    Apply(const UsdPrim &prim);

protected:
    /// Returns the kind of schema this class belongs to.
    ///
    /// \sa UsdSchemaKind
    MMDSCHEMA_API
    UsdSchemaKind _GetSchemaKind() const override;

private:
    // needs to invoke _GetStaticTfType.
    friend class UsdSchemaRegistry;
    MMDSCHEMA_API
    static const TfType &_GetStaticTfType();

    static bool _IsTypedSchema();

    // override SchemaBase virtuals.
    MMDSCHEMA_API
    const TfType &_GetTfType() const override;

public:
    // --------------------------------------------------------------------- //
    // DIFFUSECOLOR 
    // --------------------------------------------------------------------- //
    /// PMX diffuse RGBA.
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `color4f inputs:mmd:material:diffuseColor = (1, 1, 1, 1)` |
    /// | C++ Type | GfVec4f |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Color4f |
    MMDSCHEMA_API
    UsdAttribute GetDiffuseColorAttr() const;

    /// See GetDiffuseColorAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    MMDSCHEMA_API
    UsdAttribute CreateDiffuseColorAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // SPECULARCOLOR 
    // --------------------------------------------------------------------- //
    /// PMX specular RGB.
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `color3f inputs:mmd:material:specularColor = (0, 0, 0)` |
    /// | C++ Type | GfVec3f |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Color3f |
    MMDSCHEMA_API
    UsdAttribute GetSpecularColorAttr() const;

    /// See GetSpecularColorAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    MMDSCHEMA_API
    UsdAttribute CreateSpecularColorAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // SPECULARPOWER 
    // --------------------------------------------------------------------- //
    /// PMX specular power (exponent).
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `float inputs:mmd:material:specularPower = 0` |
    /// | C++ Type | float |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Float |
    MMDSCHEMA_API
    UsdAttribute GetSpecularPowerAttr() const;

    /// See GetSpecularPowerAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    MMDSCHEMA_API
    UsdAttribute CreateSpecularPowerAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // AMBIENTCOLOR 
    // --------------------------------------------------------------------- //
    /// PMX ambient RGB.
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `color3f inputs:mmd:material:ambientColor = (0, 0, 0)` |
    /// | C++ Type | GfVec3f |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Color3f |
    MMDSCHEMA_API
    UsdAttribute GetAmbientColorAttr() const;

    /// See GetAmbientColorAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    MMDSCHEMA_API
    UsdAttribute CreateAmbientColorAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // DOUBLESIDED 
    // --------------------------------------------------------------------- //
    /// PMX drawing flag 0x01, no culling. The mesh's own
    /// doubleSided is what a generic renderer reads
    /// (STAGE_CONTRACT.md s8.5).
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `uniform bool inputs:mmd:material:doubleSided = 0` |
    /// | C++ Type | bool |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Bool |
    /// | \ref SdfVariability "Variability" | SdfVariabilityUniform |
    MMDSCHEMA_API
    UsdAttribute GetDoubleSidedAttr() const;

    /// See GetDoubleSidedAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    MMDSCHEMA_API
    UsdAttribute CreateDoubleSidedAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // GROUNDSHADOW 
    // --------------------------------------------------------------------- //
    /// PMX drawing flag 0x02, ground shadow.
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `uniform bool inputs:mmd:material:groundShadow = 0` |
    /// | C++ Type | bool |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Bool |
    /// | \ref SdfVariability "Variability" | SdfVariabilityUniform |
    MMDSCHEMA_API
    UsdAttribute GetGroundShadowAttr() const;

    /// See GetGroundShadowAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    MMDSCHEMA_API
    UsdAttribute CreateGroundShadowAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // CASTSELFSHADOW 
    // --------------------------------------------------------------------- //
    /// PMX drawing flag 0x04, draws into the self-shadow map.
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `uniform bool inputs:mmd:material:castSelfShadow = 0` |
    /// | C++ Type | bool |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Bool |
    /// | \ref SdfVariability "Variability" | SdfVariabilityUniform |
    MMDSCHEMA_API
    UsdAttribute GetCastSelfShadowAttr() const;

    /// See GetCastSelfShadowAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    MMDSCHEMA_API
    UsdAttribute CreateCastSelfShadowAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // RECEIVESELFSHADOW 
    // --------------------------------------------------------------------- //
    /// PMX drawing flag 0x08, receives self shadow.
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `uniform bool inputs:mmd:material:receiveSelfShadow = 0` |
    /// | C++ Type | bool |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Bool |
    /// | \ref SdfVariability "Variability" | SdfVariabilityUniform |
    MMDSCHEMA_API
    UsdAttribute GetReceiveSelfShadowAttr() const;

    /// See GetReceiveSelfShadowAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    MMDSCHEMA_API
    UsdAttribute CreateReceiveSelfShadowAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // DRAWEDGE 
    // --------------------------------------------------------------------- //
    /// PMX drawing flag 0x10, draws the outline.
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `uniform bool inputs:mmd:material:drawEdge = 0` |
    /// | C++ Type | bool |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Bool |
    /// | \ref SdfVariability "Variability" | SdfVariabilityUniform |
    MMDSCHEMA_API
    UsdAttribute GetDrawEdgeAttr() const;

    /// See GetDrawEdgeAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    MMDSCHEMA_API
    UsdAttribute CreateDrawEdgeAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // VERTEXCOLOR 
    // --------------------------------------------------------------------- //
    /// PMX drawing flag 0x20. PMX 2.1 means vertex color; a PMX 2.0
    /// source's bit is preserved without that meaning.
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `uniform bool inputs:mmd:material:vertexColor = 0` |
    /// | C++ Type | bool |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Bool |
    /// | \ref SdfVariability "Variability" | SdfVariabilityUniform |
    MMDSCHEMA_API
    UsdAttribute GetVertexColorAttr() const;

    /// See GetVertexColorAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    MMDSCHEMA_API
    UsdAttribute CreateVertexColorAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // DRAWPOINTS 
    // --------------------------------------------------------------------- //
    /// PMX drawing flag 0x40. PMX 2.1 means point drawing; a PMX 2.0
    /// source's bit is preserved without that meaning.
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `uniform bool inputs:mmd:material:drawPoints = 0` |
    /// | C++ Type | bool |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Bool |
    /// | \ref SdfVariability "Variability" | SdfVariabilityUniform |
    MMDSCHEMA_API
    UsdAttribute GetDrawPointsAttr() const;

    /// See GetDrawPointsAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    MMDSCHEMA_API
    UsdAttribute CreateDrawPointsAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // DRAWLINES 
    // --------------------------------------------------------------------- //
    /// PMX drawing flag 0x80. PMX 2.1 means line drawing; a PMX 2.0
    /// source's bit is preserved without that meaning.
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `uniform bool inputs:mmd:material:drawLines = 0` |
    /// | C++ Type | bool |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Bool |
    /// | \ref SdfVariability "Variability" | SdfVariabilityUniform |
    MMDSCHEMA_API
    UsdAttribute GetDrawLinesAttr() const;

    /// See GetDrawLinesAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    MMDSCHEMA_API
    UsdAttribute CreateDrawLinesAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // EDGECOLOR 
    // --------------------------------------------------------------------- //
    /// PMX edge RGBA, authored whether or not drawEdge is set.
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `color4f inputs:mmd:material:edgeColor = (0, 0, 0, 0)` |
    /// | C++ Type | GfVec4f |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Color4f |
    MMDSCHEMA_API
    UsdAttribute GetEdgeColorAttr() const;

    /// See GetEdgeColorAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    MMDSCHEMA_API
    UsdAttribute CreateEdgeColorAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // EDGESIZE 
    // --------------------------------------------------------------------- //
    /// PMX edge size, authored whether or not drawEdge is set. The
    /// per-vertex scale is the mesh's primvars:mmd:edgeScale.
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `float inputs:mmd:material:edgeSize = 0` |
    /// | C++ Type | float |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Float |
    MMDSCHEMA_API
    UsdAttribute GetEdgeSizeAttr() const;

    /// See GetEdgeSizeAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    MMDSCHEMA_API
    UsdAttribute CreateEdgeSizeAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // TEXTURE 
    // --------------------------------------------------------------------- //
    /// The base texture, when the PMX slot names a safe asset path.
    /// The verbatim source string is customData mmd:sourceTexturePath.
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `asset inputs:mmd:material:texture = @@` |
    /// | C++ Type | SdfAssetPath |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Asset |
    MMDSCHEMA_API
    UsdAttribute GetTextureAttr() const;

    /// See GetTextureAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    MMDSCHEMA_API
    UsdAttribute CreateTextureAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // SPHERETEXTURE 
    // --------------------------------------------------------------------- //
    /// The sphere texture, when the PMX slot names a safe asset path.
    /// The verbatim source string is customData
    /// mmd:sourceSphereTexturePath.
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `asset inputs:mmd:material:sphereTexture = @@` |
    /// | C++ Type | SdfAssetPath |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Asset |
    MMDSCHEMA_API
    UsdAttribute GetSphereTextureAttr() const;

    /// See GetSphereTextureAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    MMDSCHEMA_API
    UsdAttribute CreateSphereTextureAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // SPHEREMODE 
    // --------------------------------------------------------------------- //
    /// How the sphere texture combines. subTexture samples it as an
    /// ordinary texture through additional UV 1, primvars:mmd:uv1.
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `uniform token inputs:mmd:material:sphereMode = "disabled"` |
    /// | C++ Type | TfToken |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Token |
    /// | \ref SdfVariability "Variability" | SdfVariabilityUniform |
    /// | \ref UsdMmdTokens "Allowed Values" | disabled, multiply, add, subTexture |
    MMDSCHEMA_API
    UsdAttribute GetSphereModeAttr() const;

    /// See GetSphereModeAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    MMDSCHEMA_API
    UsdAttribute CreateSphereModeAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // TOONSOURCE 
    // --------------------------------------------------------------------- //
    /// Which toon ramp the material names: none; individual, in
    /// toonTexture; or shared, MMD's own ramp sharedToonIndex, whose image
    /// the consumer supplies.
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `uniform token inputs:mmd:material:toonSource = "none"` |
    /// | C++ Type | TfToken |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Token |
    /// | \ref SdfVariability "Variability" | SdfVariabilityUniform |
    /// | \ref UsdMmdTokens "Allowed Values" | none, individual, shared |
    MMDSCHEMA_API
    UsdAttribute GetToonSourceAttr() const;

    /// See GetToonSourceAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    MMDSCHEMA_API
    UsdAttribute CreateToonSourceAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // TOONTEXTURE 
    // --------------------------------------------------------------------- //
    /// The individual toon texture, authored only when toonSource is
    /// individual and the path is safe. The verbatim source string is
    /// customData mmd:sourceToonTexturePath.
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `asset inputs:mmd:material:toonTexture = @@` |
    /// | C++ Type | SdfAssetPath |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Asset |
    MMDSCHEMA_API
    UsdAttribute GetToonTextureAttr() const;

    /// See GetToonTextureAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    MMDSCHEMA_API
    UsdAttribute CreateToonTextureAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // --------------------------------------------------------------------- //
    // SHAREDTOONINDEX 
    // --------------------------------------------------------------------- //
    /// MMD's shared toon slot 0-9 (toon01.bmp-toon10.bmp), authored
    /// only when toonSource is shared; -1 when unauthored.
    ///
    /// | ||
    /// | -- | -- |
    /// | Declaration | `uniform int inputs:mmd:material:sharedToonIndex = -1` |
    /// | C++ Type | int |
    /// | \ref Usd_Datatypes "Usd Type" | SdfValueTypeNames->Int |
    /// | \ref SdfVariability "Variability" | SdfVariabilityUniform |
    MMDSCHEMA_API
    UsdAttribute GetSharedToonIndexAttr() const;

    /// See GetSharedToonIndexAttr(), and also 
    /// \ref Usd_Create_Or_Get_Property for when to use Get vs Create.
    /// If specified, author \p defaultValue as the attribute's default,
    /// sparsely (when it makes sense to do so) if \p writeSparsely is \c true -
    /// the default for \p writeSparsely is \c false.
    MMDSCHEMA_API
    UsdAttribute CreateSharedToonIndexAttr(VtValue const &defaultValue = VtValue(), bool writeSparsely=false) const;

public:
    // ===================================================================== //
    // Feel free to add custom code below this line, it will be preserved by 
    // the code generator. 
    //
    // Just remember to: 
    //  - Close the class declaration with }; 
    //  - Close the namespace with PXR_NAMESPACE_CLOSE_SCOPE
    //  - Close the include guard with #endif
    // ===================================================================== //
    // --(BEGIN CUSTOM CODE)--
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
