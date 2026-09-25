//
// Copyright 2016 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef USDMMD_TOKENS_H
#define USDMMD_TOKENS_H

/// \file mmdSchema/tokens.h

// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
// 
// This is an automatically generated file (by usdGenSchema.py).
// Do not hand-edit!
// 
// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

#include "pxr/pxr.h"
#include "./api.h"
#include "pxr/base/tf/staticData.h"
#include "pxr/base/tf/token.h"
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE


/// \class UsdMmdTokensType
///
/// \link UsdMmdTokens \endlink provides static, efficient
/// \link TfToken TfTokens\endlink for use in all public USD API.
///
/// These tokens are auto-generated from the module's schema, representing
/// property names, for when you need to fetch an attribute or relationship
/// directly by name, e.g. UsdPrim::GetAttribute(), in the most efficient
/// manner, and allow the compiler to verify that you spelled the name
/// correctly.
///
/// UsdMmdTokens also contains all of the \em allowedTokens values
/// declared for schema builtin attributes of 'token' scene description type.
/// Use UsdMmdTokens like so:
///
/// \code
///     gprim.GetMyTokenValuedAttr().Set(UsdMmdTokens->add);
/// \endcode
struct UsdMmdTokensType {
    MMDSCHEMA_API UsdMmdTokensType();
    /// \brief "add"
    /// 
    /// Possible value for UsdMmdMaterialAPI::GetSphereModeAttr()
    const TfToken add;
    /// \brief "disabled"
    /// 
    /// Fallback value for UsdMmdMaterialAPI::GetSphereModeAttr()
    const TfToken disabled;
    /// \brief "individual"
    /// 
    /// Possible value for UsdMmdMaterialAPI::GetToonSourceAttr()
    const TfToken individual;
    /// \brief "inputs:mmd:material:ambientColor"
    /// 
    /// UsdMmdMaterialAPI
    const TfToken inputsMmdMaterialAmbientColor;
    /// \brief "inputs:mmd:material:castSelfShadow"
    /// 
    /// UsdMmdMaterialAPI
    const TfToken inputsMmdMaterialCastSelfShadow;
    /// \brief "inputs:mmd:material:diffuseColor"
    /// 
    /// UsdMmdMaterialAPI
    const TfToken inputsMmdMaterialDiffuseColor;
    /// \brief "inputs:mmd:material:doubleSided"
    /// 
    /// UsdMmdMaterialAPI
    const TfToken inputsMmdMaterialDoubleSided;
    /// \brief "inputs:mmd:material:drawEdge"
    /// 
    /// UsdMmdMaterialAPI
    const TfToken inputsMmdMaterialDrawEdge;
    /// \brief "inputs:mmd:material:drawLines"
    /// 
    /// UsdMmdMaterialAPI
    const TfToken inputsMmdMaterialDrawLines;
    /// \brief "inputs:mmd:material:drawPoints"
    /// 
    /// UsdMmdMaterialAPI
    const TfToken inputsMmdMaterialDrawPoints;
    /// \brief "inputs:mmd:material:edgeColor"
    /// 
    /// UsdMmdMaterialAPI
    const TfToken inputsMmdMaterialEdgeColor;
    /// \brief "inputs:mmd:material:edgeSize"
    /// 
    /// UsdMmdMaterialAPI
    const TfToken inputsMmdMaterialEdgeSize;
    /// \brief "inputs:mmd:material:groundShadow"
    /// 
    /// UsdMmdMaterialAPI
    const TfToken inputsMmdMaterialGroundShadow;
    /// \brief "inputs:mmd:material:receiveSelfShadow"
    /// 
    /// UsdMmdMaterialAPI
    const TfToken inputsMmdMaterialReceiveSelfShadow;
    /// \brief "inputs:mmd:material:sharedToonIndex"
    /// 
    /// UsdMmdMaterialAPI
    const TfToken inputsMmdMaterialSharedToonIndex;
    /// \brief "inputs:mmd:material:specularColor"
    /// 
    /// UsdMmdMaterialAPI
    const TfToken inputsMmdMaterialSpecularColor;
    /// \brief "inputs:mmd:material:specularPower"
    /// 
    /// UsdMmdMaterialAPI
    const TfToken inputsMmdMaterialSpecularPower;
    /// \brief "inputs:mmd:material:sphereMode"
    /// 
    /// UsdMmdMaterialAPI
    const TfToken inputsMmdMaterialSphereMode;
    /// \brief "inputs:mmd:material:sphereTexture"
    /// 
    /// UsdMmdMaterialAPI
    const TfToken inputsMmdMaterialSphereTexture;
    /// \brief "inputs:mmd:material:texture"
    /// 
    /// UsdMmdMaterialAPI
    const TfToken inputsMmdMaterialTexture;
    /// \brief "inputs:mmd:material:toonSource"
    /// 
    /// UsdMmdMaterialAPI
    const TfToken inputsMmdMaterialToonSource;
    /// \brief "inputs:mmd:material:toonTexture"
    /// 
    /// UsdMmdMaterialAPI
    const TfToken inputsMmdMaterialToonTexture;
    /// \brief "inputs:mmd:material:vertexColor"
    /// 
    /// UsdMmdMaterialAPI
    const TfToken inputsMmdMaterialVertexColor;
    /// \brief "multiply"
    /// 
    /// Possible value for UsdMmdMaterialAPI::GetSphereModeAttr()
    const TfToken multiply;
    /// \brief "none"
    /// 
    /// Fallback value for UsdMmdMaterialAPI::GetToonSourceAttr()
    const TfToken none;
    /// \brief "shared"
    /// 
    /// Possible value for UsdMmdMaterialAPI::GetToonSourceAttr()
    const TfToken shared;
    /// \brief "subTexture"
    /// 
    /// Possible value for UsdMmdMaterialAPI::GetSphereModeAttr()
    const TfToken subTexture;
    /// \brief "MmdMaterialAPI"
    /// 
    /// Schema identifer and family for UsdMmdMaterialAPI
    const TfToken MmdMaterialAPI;
    /// A vector of all of the tokens listed above.
    const std::vector<TfToken> allTokens;
};

/// \var UsdMmdTokens
///
/// A global variable with static, efficient \link TfToken TfTokens\endlink
/// for use in all public USD API.  \sa UsdMmdTokensType
extern MMDSCHEMA_API TfStaticData<UsdMmdTokensType> UsdMmdTokens;

PXR_NAMESPACE_CLOSE_SCOPE

#endif
