//
// Copyright 2016 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "./tokens.h"

PXR_NAMESPACE_OPEN_SCOPE

UsdMmdTokensType::UsdMmdTokensType() :
    add("add", TfToken::Immortal),
    disabled("disabled", TfToken::Immortal),
    individual("individual", TfToken::Immortal),
    inputsMmdMaterialAmbientColor("inputs:mmd:material:ambientColor", TfToken::Immortal),
    inputsMmdMaterialCastSelfShadow("inputs:mmd:material:castSelfShadow", TfToken::Immortal),
    inputsMmdMaterialDiffuseColor("inputs:mmd:material:diffuseColor", TfToken::Immortal),
    inputsMmdMaterialDoubleSided("inputs:mmd:material:doubleSided", TfToken::Immortal),
    inputsMmdMaterialDrawEdge("inputs:mmd:material:drawEdge", TfToken::Immortal),
    inputsMmdMaterialDrawLines("inputs:mmd:material:drawLines", TfToken::Immortal),
    inputsMmdMaterialDrawPoints("inputs:mmd:material:drawPoints", TfToken::Immortal),
    inputsMmdMaterialEdgeColor("inputs:mmd:material:edgeColor", TfToken::Immortal),
    inputsMmdMaterialEdgeSize("inputs:mmd:material:edgeSize", TfToken::Immortal),
    inputsMmdMaterialGroundShadow("inputs:mmd:material:groundShadow", TfToken::Immortal),
    inputsMmdMaterialReceiveSelfShadow("inputs:mmd:material:receiveSelfShadow", TfToken::Immortal),
    inputsMmdMaterialSharedToonIndex("inputs:mmd:material:sharedToonIndex", TfToken::Immortal),
    inputsMmdMaterialSpecularColor("inputs:mmd:material:specularColor", TfToken::Immortal),
    inputsMmdMaterialSpecularPower("inputs:mmd:material:specularPower", TfToken::Immortal),
    inputsMmdMaterialSphereMode("inputs:mmd:material:sphereMode", TfToken::Immortal),
    inputsMmdMaterialSphereTexture("inputs:mmd:material:sphereTexture", TfToken::Immortal),
    inputsMmdMaterialTexture("inputs:mmd:material:texture", TfToken::Immortal),
    inputsMmdMaterialToonSource("inputs:mmd:material:toonSource", TfToken::Immortal),
    inputsMmdMaterialToonTexture("inputs:mmd:material:toonTexture", TfToken::Immortal),
    inputsMmdMaterialVertexColor("inputs:mmd:material:vertexColor", TfToken::Immortal),
    multiply("multiply", TfToken::Immortal),
    none("none", TfToken::Immortal),
    shared("shared", TfToken::Immortal),
    subTexture("subTexture", TfToken::Immortal),
    MmdMaterialAPI("MmdMaterialAPI", TfToken::Immortal),
    allTokens({
        add,
        disabled,
        individual,
        inputsMmdMaterialAmbientColor,
        inputsMmdMaterialCastSelfShadow,
        inputsMmdMaterialDiffuseColor,
        inputsMmdMaterialDoubleSided,
        inputsMmdMaterialDrawEdge,
        inputsMmdMaterialDrawLines,
        inputsMmdMaterialDrawPoints,
        inputsMmdMaterialEdgeColor,
        inputsMmdMaterialEdgeSize,
        inputsMmdMaterialGroundShadow,
        inputsMmdMaterialReceiveSelfShadow,
        inputsMmdMaterialSharedToonIndex,
        inputsMmdMaterialSpecularColor,
        inputsMmdMaterialSpecularPower,
        inputsMmdMaterialSphereMode,
        inputsMmdMaterialSphereTexture,
        inputsMmdMaterialTexture,
        inputsMmdMaterialToonSource,
        inputsMmdMaterialToonTexture,
        inputsMmdMaterialVertexColor,
        multiply,
        none,
        shared,
        subTexture,
        MmdMaterialAPI
    })
{
}

TfStaticData<UsdMmdTokensType> UsdMmdTokens;

PXR_NAMESPACE_CLOSE_SCOPE
