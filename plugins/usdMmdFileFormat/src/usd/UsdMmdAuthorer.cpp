// SPDX-License-Identifier: Apache-2.0
#include "usd/UsdMmdAuthorer.h"

#include "usd/UsdMmdCodes.h"

#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/quatf.h"
#include "pxr/base/gf/vec2f.h"
#include "pxr/base/gf/vec3d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/gf/vec4f.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/vt/array.h"
#include "pxr/base/vt/types.h"
#include "pxr/base/vt/value.h"
#include "pxr/usd/kind/registry.h"
#include "pxr/usd/sdf/assetPath.h"
#include "pxr/usd/sdf/layer.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/sdf/types.h"
#include "pxr/usd/usd/modelAPI.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usdGeom/mesh.h"
#include "pxr/usd/usdGeom/metrics.h"
#include "pxr/usd/usdGeom/primvarsAPI.h"
#include "pxr/usd/usdGeom/scope.h"
#include "pxr/usd/usdGeom/subset.h"
#include "pxr/usd/usdGeom/tokens.h"
#include "pxr/usd/usdGeom/xform.h"
#include "pxr/usd/usdShade/material.h"
#include "pxr/usd/usdShade/materialBindingAPI.h"
#include "pxr/usd/usdShade/nodeGraph.h"
#include "pxr/usd/usdShade/shader.h"
#include "pxr/usd/usdShade/tokens.h"
#include "pxr/usd/usdSkel/bindingAPI.h"
#include "pxr/usd/usdSkel/blendShape.h"
#include "pxr/usd/usdSkel/root.h"
#include "pxr/usd/usdSkel/skeleton.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <string>
#include <type_traits>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdmmd {

namespace {

// customData keys are USD key paths: "mmd:x" is key "x" in an "mmd"
// sub-dictionary, read back with GetCustomDataByKey("mmd:x")
// (STAGE_CONTRACT.md §3).
const TfToken kStageContractVersionKey("mmd:stageContractVersion");
const TfToken kSourceFormatKey("mmd:sourceFormat");
const TfToken kSourceVersionKey("mmd:sourceVersion");
const TfToken kSourceNameKey("mmd:sourceName");
const TfToken kSourceEnglishNameKey("mmd:sourceEnglishName");
const TfToken kSourceCommentKey("mmd:sourceComment");
const TfToken kSourceEnglishCommentKey("mmd:sourceEnglishComment");
const TfToken kSourceIndexKey("mmd:sourceIndex");
const TfToken kSourceTexturePathKey("mmd:sourceTexturePath");
const TfToken kSourceSphereTexturePathKey("mmd:sourceSphereTexturePath");
const TfToken kSourceToonTexturePathKey("mmd:sourceToonTexturePath");
const TfToken kDiagnosticsKey("mmd:diagnostics");

const SdfPath kAssetPath("/Asset");
const SdfPath kGeoPath("/Asset/geo");
const SdfPath kMeshPath("/Asset/geo/Mesh");
const SdfPath kMtlPath("/Asset/mtl");
const SdfPath kSkelPath("/Asset/skel");
const SdfPath kSkeletonPath("/Asset/skel/Skeleton");
const SdfPath kMorphPath("/Asset/morph");
const SdfPath kRigPath("/Asset/rig");
const SdfPath kRigBonesPath("/Asset/rig/Bones");
const SdfPath kRigIkPath("/Asset/rig/ik");

template <class GfType, class Source>
VtArray<GfType>
ToVtArray(const std::vector<Source>& values)
{
    VtArray<GfType> out;
    out.reserve(values.size());
    for (const Source& v : values) {
        if constexpr (std::is_same_v<Source, mmd::Float2>) {
            out.push_back(GfType(v[0], v[1]));
        } else if constexpr (std::is_same_v<Source, mmd::Float3>) {
            out.push_back(GfType(v[0], v[1], v[2]));
        } else if constexpr (std::is_same_v<Source, mmd::Float4>) {
            out.push_back(GfType(v[0], v[1], v[2], v[3]));
        } else {
            out.push_back(static_cast<GfType>(v));
        }
    }
    return out;
}

/// A custom `mmd:`-namespaced attribute (STAGE_CONTRACT.md §3).
void
SetCustom(const UsdPrim& prim, const char* name, const SdfValueTypeName& type, const VtValue& value,
          SdfVariability variability = SdfVariabilityVarying)
{
    prim.CreateAttribute(TfToken(name), type, /*custom=*/true, variability).Set(value);
}

/// A vertex-interpolated primvar in the `mmd:` namespace (STAGE_CONTRACT.md §3).
void
SetVertexPrimvar(const UsdGeomPrimvarsAPI& primvars, const char* name, const SdfValueTypeName& type,
                 const VtValue& value)
{
    primvars.CreatePrimvar(TfToken(name), type, UsdGeomTokens->vertex).Set(value);
}

float
SpecularPowerToRoughness(float power)
{
    if (!std::isfinite(power) || power < 0.0f) {
        power = 0.0f;
    }
    const float alpha = std::sqrt(2.0f / (power + 2.0f));
    return std::clamp(std::sqrt(alpha), 0.05f, 1.0f);
}

const char*
SphereModeName(mmd::SphereMode mode)
{
    switch (mode) {
    case mmd::SphereMode::Multiply:
        return "multiply";
    case mmd::SphereMode::Add:
        return "add";
    case mmd::SphereMode::SubTexture:
        return "subTexture";
    case mmd::SphereMode::Disabled:
    default:
        return "disabled";
    }
}

const char*
MorphTypeName(mmd::MorphType type)
{
    switch (type) {
    case mmd::MorphType::Group:
        return "group";
    case mmd::MorphType::Vertex:
        return "vertex";
    case mmd::MorphType::Bone:
        return "bone";
    case mmd::MorphType::Uv:
        return "uv";
    case mmd::MorphType::AdditionalUv1:
        return "uv1";
    case mmd::MorphType::AdditionalUv2:
        return "uv2";
    case mmd::MorphType::AdditionalUv3:
        return "uv3";
    case mmd::MorphType::AdditionalUv4:
        return "uv4";
    case mmd::MorphType::Material:
        return "material";
    case mmd::MorphType::Flip:
        return "flip";
    case mmd::MorphType::Impulse:
        return "impulse";
    }
    return "other";
}

const char*
MorphPanelName(mmd::MorphPanel panel)
{
    switch (panel) {
    case mmd::MorphPanel::Hidden:
        return "hidden";
    case mmd::MorphPanel::Eyebrow:
        return "eyebrow";
    case mmd::MorphPanel::Eye:
        return "eye";
    case mmd::MorphPanel::Mouth:
        return "mouth";
    case mmd::MorphPanel::Other:
        break;
    }
    return "other";
}

const char*
ToonSourceName(mmd::ToonSource source)
{
    switch (source) {
    case mmd::ToonSource::Individual:
        return "individual";
    case mmd::ToonSource::Shared:
        return "shared";
    case mmd::ToonSource::None:
    default:
        return "none";
    }
}

class Authorer {
public:
    Authorer(const mmd::CanonicalDocument& document, std::vector<mmd::Diagnostic>& diagnostics)
        : _doc(document), _diagnostics(diagnostics)
    {
    }

    bool Author(std::string* outUsda)
    {
        _stage = UsdStage::CreateInMemory();
        if (!_stage) {
            return false;
        }
        // Stage metadata (STAGE_CONTRACT.md §6): Y-up, meters. Every length
        // arrives in meters from canonicalization, never through
        // metersPerUnit.
        UsdGeomSetStageUpAxis(_stage, UsdGeomTokens->y);
        UsdGeomSetStageMetersPerUnit(_stage, UsdGeomLinearUnits::meters);

        // /Asset is the SkelRoot of a model with bones, so every mesh beneath
        // it is skinned in any UsdSkel consumer; an Xform otherwise
        // (STAGE_CONTRACT.md §4.1).
        const bool skinned = !_doc.skeleton.bones.empty();
        const UsdPrim asset = skinned ? UsdSkelRoot::Define(_stage, kAssetPath).GetPrim()
                                      : UsdGeomXform::Define(_stage, kAssetPath).GetPrim();
        if (!asset) {
            return false;
        }
        _stage->SetDefaultPrim(asset);
        UsdModelAPI(asset).SetKind(KindTokens->component);
        _Metadata(asset);

        // Scopes in the contract's order (§4), each only when it has children.
        UsdGeomMesh mesh;
        if (!_doc.mesh.points.empty()) {
            UsdGeomScope::Define(_stage, kGeoPath);
            mesh = _Mesh();
        }
        if (!_doc.materials.empty()) {
            UsdGeomScope::Define(_stage, kMtlPath);
            _Materials();
        }
        if (mesh) {
            _Subsets(mesh);
        }
        if (skinned) {
            UsdGeomScope::Define(_stage, kSkelPath);
            _Skeleton();
        }
        if (!_doc.morphs.empty()) {
            UsdGeomScope::Define(_stage, kMorphPath);
            _Morphs(mesh, skinned);
        }
        if (skinned) {
            UsdGeomScope::Define(_stage, kRigPath);
            _Rig();
        }

        _ImporterDiagnostics(skinned);
        _RecordDiagnostics(asset);
        return _stage->GetRootLayer()->ExportToString(outUsda);
    }

private:
    /// Model metadata and provenance (STAGE_CONTRACT.md §5).
    void _Metadata(const UsdPrim& asset)
    {
        const mmd::Metadata& m = _doc.metadata;
        asset.SetCustomDataByKey(kStageContractVersionKey, VtValue(kStageContractVersion));
        asset.SetCustomDataByKey(kSourceFormatKey, VtValue(std::string("PMX")));
        asset.SetCustomDataByKey(kSourceVersionKey, VtValue(m.sourceVersion));
        asset.SetCustomDataByKey(kSourceNameKey, VtValue(m.name));
        asset.SetCustomDataByKey(kSourceEnglishNameKey, VtValue(m.englishName));
        asset.SetCustomDataByKey(kSourceCommentKey, VtValue(m.comment));
        asset.SetCustomDataByKey(kSourceEnglishCommentKey, VtValue(m.englishComment));
    }

    /// The one mesh (STAGE_CONTRACT.md §8), and its skin binding (§9.4).
    UsdGeomMesh _Mesh()
    {
        const mmd::Mesh& m = _doc.mesh;
        UsdGeomMesh mesh = UsdGeomMesh::Define(_stage, kMeshPath);
        // PMX meshes are final polygons; USD's default would smooth them.
        mesh.CreateSubdivisionSchemeAttr(VtValue(UsdGeomTokens->none));
        if (m.doubleSided) {
            mesh.CreateDoubleSidedAttr(VtValue(true));
        }

        const VtVec3fArray points = ToVtArray<GfVec3f>(m.points);
        VtVec3fArray extent(2);
        if (UsdGeomPointBased::ComputeExtent(points, &extent)) {
            mesh.CreateExtentAttr(VtValue(extent));
        }
        mesh.CreateFaceVertexCountsAttr(VtValue(VtIntArray(m.FaceCount(), 3)));
        mesh.CreateFaceVertexIndicesAttr(VtValue(ToVtArray<int>(m.faceVertexIndices)));
        mesh.CreatePointsAttr(VtValue(points));
        mesh.CreateNormalsAttr(VtValue(ToVtArray<GfVec3f>(m.normals)));
        mesh.SetNormalsInterpolation(UsdGeomTokens->vertex);

        const UsdGeomPrimvarsAPI primvars(mesh);
        SetVertexPrimvar(
            primvars, "st", SdfValueTypeNames->TexCoord2fArray, VtValue(ToVtArray<GfVec2f>(m.st)));
        for (std::size_t k = 0; k < m.additionalUvCount; ++k) {
            const std::string name = "mmd:uv" + std::to_string(k + 1);
            SetVertexPrimvar(primvars,
                             name.c_str(),
                             SdfValueTypeNames->Float4Array,
                             VtValue(ToVtArray<GfVec4f>(m.additionalUv[k])));
        }
        SetVertexPrimvar(primvars,
                         "mmd:edgeScale",
                         SdfValueTypeNames->FloatArray,
                         VtValue(ToVtArray<float>(m.edgeScale)));

        if (m.influencesPerVertex > 0) {
            const UsdSkelBindingAPI binding = UsdSkelBindingAPI::Apply(mesh.GetPrim());
            binding.CreateSkeletonRel().SetTargets({kSkeletonPath});
            const int n = static_cast<int>(m.influencesPerVertex);
            binding.CreateJointIndicesPrimvar(/*constant=*/false, n)
                .Set(ToVtArray<int>(m.jointIndices));
            binding.CreateJointWeightsPrimvar(/*constant=*/false, n)
                .Set(ToVtArray<float>(m.jointWeights));

            VtIntArray deformTypes;
            deformTypes.reserve(m.deformTypes.size());
            for (mmd::DeformType type : m.deformTypes) {
                deformTypes.push_back(static_cast<int>(type));
            }
            SetVertexPrimvar(
                primvars, "mmd:deformType", SdfValueTypeNames->IntArray, VtValue(deformTypes));
            if (!m.sdefC.empty()) {
                SetVertexPrimvar(primvars,
                                 "mmd:sdefC",
                                 SdfValueTypeNames->Point3fArray,
                                 VtValue(ToVtArray<GfVec3f>(m.sdefC)));
                SetVertexPrimvar(primvars,
                                 "mmd:sdefR0",
                                 SdfValueTypeNames->Point3fArray,
                                 VtValue(ToVtArray<GfVec3f>(m.sdefR0)));
                SetVertexPrimvar(primvars,
                                 "mmd:sdefR1",
                                 SdfValueTypeNames->Point3fArray,
                                 VtValue(ToVtArray<GfVec3f>(m.sdefR1)));
            }
        }
        return mesh;
    }

    UsdShadeShader _Shader(const SdfPath& graphPath, const char* name, const char* id)
    {
        UsdShadeShader shader =
            UsdShadeShader::Define(_stage, graphPath.AppendChild(TfToken(name)));
        shader.CreateIdAttr(VtValue(TfToken(id)));
        return shader;
    }

    const mmd::Texture* _TextureWithAsset(std::int32_t index) const
    {
        if (index == mmd::kNone || index < 0 ||
            static_cast<std::size_t>(index) >= _doc.textures.size()) {
            return nullptr;
        }
        const mmd::Texture& texture = _doc.textures[static_cast<std::size_t>(index)];
        return texture.assetPath.empty() ? nullptr : &texture;
    }

    void _MaterialSemantics(const UsdPrim& prim, const mmd::Material& m)
    {
        SetCustom(prim,
                  "mmd:material:diffuseColor",
                  SdfValueTypeNames->Color4f,
                  VtValue(GfVec4f(
                      m.diffuseColor[0], m.diffuseColor[1], m.diffuseColor[2], m.diffuseColor[3])),
                  SdfVariabilityUniform);
        SetCustom(prim,
                  "mmd:material:specularColor",
                  SdfValueTypeNames->Color3f,
                  VtValue(GfVec3f(m.specularColor[0], m.specularColor[1], m.specularColor[2])),
                  SdfVariabilityUniform);
        SetCustom(prim,
                  "mmd:material:specularPower",
                  SdfValueTypeNames->Float,
                  VtValue(m.specularPower),
                  SdfVariabilityUniform);
        SetCustom(prim,
                  "mmd:material:ambientColor",
                  SdfValueTypeNames->Color3f,
                  VtValue(GfVec3f(m.ambientColor[0], m.ambientColor[1], m.ambientColor[2])),
                  SdfVariabilityUniform);

        const auto boolean = [&](const char* name, bool value) {
            SetCustom(prim, name, SdfValueTypeNames->Bool, VtValue(value), SdfVariabilityUniform);
        };
        boolean("mmd:material:doubleSided", m.doubleSided);
        boolean("mmd:material:groundShadow", m.groundShadow);
        boolean("mmd:material:castSelfShadow", m.castSelfShadow);
        boolean("mmd:material:receiveSelfShadow", m.receiveSelfShadow);
        boolean("mmd:material:drawEdge", m.drawEdge);
        boolean("mmd:material:vertexColor", m.vertexColor);
        boolean("mmd:material:drawPoints", m.drawPoints);
        boolean("mmd:material:drawLines", m.drawLines);

        SetCustom(prim,
                  "mmd:material:edgeColor",
                  SdfValueTypeNames->Color4f,
                  VtValue(GfVec4f(m.edgeColor[0], m.edgeColor[1], m.edgeColor[2], m.edgeColor[3])),
                  SdfVariabilityUniform);
        SetCustom(prim,
                  "mmd:material:edgeSize",
                  SdfValueTypeNames->Float,
                  VtValue(m.edgeSize),
                  SdfVariabilityUniform);
        SetCustom(prim,
                  "mmd:material:sphereMode",
                  SdfValueTypeNames->Token,
                  VtValue(TfToken(SphereModeName(m.sphereMode))),
                  SdfVariabilityUniform);
        SetCustom(prim,
                  "mmd:material:toonSource",
                  SdfValueTypeNames->Token,
                  VtValue(TfToken(ToonSourceName(m.toonSource))),
                  SdfVariabilityUniform);
        if (m.toonSource == mmd::ToonSource::Shared) {
            SetCustom(prim,
                      "mmd:material:sharedToonIndex",
                      SdfValueTypeNames->Int,
                      VtValue(m.sharedToonIndex),
                      SdfVariabilityUniform);
        }
    }

    void _AuthorPreview(UsdShadeMaterial material, const mmd::Material& m)
    {
        const SdfPath graphPath = material.GetPath().AppendChild(TfToken("preview"));
        UsdShadeNodeGraph graph = UsdShadeNodeGraph::Define(_stage, graphPath);
        UsdShadeShader surface = _Shader(graphPath, "surface", "UsdPreviewSurface");
        surface.CreateInput(TfToken("diffuseColor"), SdfValueTypeNames->Color3f).Set(GfVec3f(0.0f));
        surface.CreateInput(TfToken("emissiveColor"), SdfValueTypeNames->Color3f)
            .Set(GfVec3f(m.diffuseColor[0], m.diffuseColor[1], m.diffuseColor[2]));
        surface.CreateInput(TfToken("opacity"), SdfValueTypeNames->Float).Set(m.diffuseColor[3]);
        surface.CreateInput(TfToken("useSpecularWorkflow"), SdfValueTypeNames->Int).Set(0);
        surface.CreateInput(TfToken("metallic"), SdfValueTypeNames->Float).Set(0.0f);
        surface.CreateInput(TfToken("roughness"), SdfValueTypeNames->Float).Set(1.0f);

        const mmd::Texture* texture = _TextureWithAsset(m.texture);
        if (texture) {
            UsdShadeShader stReader = _Shader(graphPath, "stReader", "UsdPrimvarReader_float2");
            stReader.CreateInput(TfToken("varname"), SdfValueTypeNames->String)
                .Set(std::string("st"));
            const UsdShadeOutput st =
                stReader.CreateOutput(TfToken("result"), SdfValueTypeNames->Float2);

            UsdShadeShader image = _Shader(graphPath, "baseTexture", "UsdUVTexture");
            UsdShadeInput file = image.CreateInput(TfToken("file"), SdfValueTypeNames->Asset);
            file.Set(SdfAssetPath(texture->assetPath));
            file.GetAttr().SetColorSpace(TfToken("sRGB"));
            image.CreateInput(TfToken("st"), SdfValueTypeNames->Float2).ConnectToSource(st);
            image.CreateInput(TfToken("wrapS"), SdfValueTypeNames->Token).Set(TfToken("repeat"));
            image.CreateInput(TfToken("wrapT"), SdfValueTypeNames->Token).Set(TfToken("repeat"));
            image.CreateInput(TfToken("sourceColorSpace"), SdfValueTypeNames->Token)
                .Set(TfToken("sRGB"));
            image.CreateInput(TfToken("scale"), SdfValueTypeNames->Float4)
                .Set(GfVec4f(
                    m.diffuseColor[0], m.diffuseColor[1], m.diffuseColor[2], m.diffuseColor[3]));
            const UsdShadeOutput rgb =
                image.CreateOutput(TfToken("rgb"), SdfValueTypeNames->Float3);
            const UsdShadeOutput alpha = image.CreateOutput(TfToken("a"), SdfValueTypeNames->Float);
            surface.GetInput(TfToken("emissiveColor")).ConnectToSource(rgb);
            surface.GetInput(TfToken("opacity")).ConnectToSource(alpha);
        }

        const UsdShadeOutput shaderOut =
            surface.CreateOutput(TfToken("surface"), SdfValueTypeNames->Token);
        const UsdShadeOutput graphOut =
            graph.CreateOutput(TfToken("surface"), SdfValueTypeNames->Token);
        graphOut.ConnectToSource(shaderOut);
        material.CreateSurfaceOutput().ConnectToSource(graphOut);
    }

    void _AuthorMtlx(UsdShadeMaterial material, const mmd::Material& m)
    {
        const SdfPath graphPath = material.GetPath().AppendChild(TfToken("mtlx"));
        UsdShadeNodeGraph graph = UsdShadeNodeGraph::Define(_stage, graphPath);
        UsdShadeShader surface = _Shader(graphPath, "surface", "ND_gltf_pbr_surfaceshader");
        surface.CreateInput(TfToken("base_color"), SdfValueTypeNames->Color3f).Set(GfVec3f(0.0f));
        surface.CreateInput(TfToken("metallic"), SdfValueTypeNames->Float).Set(0.0f);
        surface.CreateInput(TfToken("roughness"), SdfValueTypeNames->Float).Set(1.0f);
        surface.CreateInput(TfToken("specular"), SdfValueTypeNames->Float).Set(0.0f);
        surface.CreateInput(TfToken("emissive"), SdfValueTypeNames->Color3f)
            .Set(GfVec3f(m.diffuseColor[0], m.diffuseColor[1], m.diffuseColor[2]));
        surface.CreateInput(TfToken("ior"), SdfValueTypeNames->Float).Set(1.5f);

        const mmd::Texture* texture = _TextureWithAsset(m.texture);
        const bool hasTexture = m.texture != mmd::kNone;
        const int alphaMode = hasTexture || m.diffuseColor[3] < 1.0f ? 2 : 0;
        surface.CreateInput(TfToken("alpha_mode"), SdfValueTypeNames->Int).Set(alphaMode);
        surface.CreateInput(TfToken("alpha"), SdfValueTypeNames->Float).Set(m.diffuseColor[3]);

        if (texture) {
            UsdShadeShader st = _Shader(graphPath, "st", "ND_texcoord_vector2");
            st.CreateInput(TfToken("index"), SdfValueTypeNames->Int).Set(0);
            const UsdShadeOutput stOut = st.CreateOutput(TfToken("out"), SdfValueTypeNames->Float2);

            UsdShadeShader image = _Shader(graphPath, "baseTexture", "ND_image_color4");
            UsdShadeInput file = image.CreateInput(TfToken("file"), SdfValueTypeNames->Asset);
            file.Set(SdfAssetPath(texture->assetPath));
            file.GetAttr().SetColorSpace(TfToken("srgb_texture"));
            image.CreateInput(TfToken("default"), SdfValueTypeNames->Color4f)
                .Set(GfVec4f(0.0f, 0.0f, 0.0f, 1.0f));
            image.CreateInput(TfToken("texcoord"), SdfValueTypeNames->Float2)
                .ConnectToSource(stOut);
            image.CreateInput(TfToken("uaddressmode"), SdfValueTypeNames->String)
                .Set(std::string("periodic"));
            image.CreateInput(TfToken("vaddressmode"), SdfValueTypeNames->String)
                .Set(std::string("periodic"));

            UsdShadeShader factor = _Shader(graphPath, "baseColorFactor", "ND_multiply_color4");
            factor.CreateInput(TfToken("in1"), SdfValueTypeNames->Color4f)
                .ConnectToSource(image.CreateOutput(TfToken("out"), SdfValueTypeNames->Color4f));
            factor.CreateInput(TfToken("in2"), SdfValueTypeNames->Color4f)
                .Set(GfVec4f(
                    m.diffuseColor[0], m.diffuseColor[1], m.diffuseColor[2], m.diffuseColor[3]));

            UsdShadeShader split = _Shader(graphPath, "baseColorSplit", "ND_separate4_color4");
            split.CreateInput(TfToken("in"), SdfValueTypeNames->Color4f)
                .ConnectToSource(factor.CreateOutput(TfToken("out"), SdfValueTypeNames->Color4f));
            const UsdShadeOutput r = split.CreateOutput(TfToken("outr"), SdfValueTypeNames->Float);
            const UsdShadeOutput g = split.CreateOutput(TfToken("outg"), SdfValueTypeNames->Float);
            const UsdShadeOutput b = split.CreateOutput(TfToken("outb"), SdfValueTypeNames->Float);
            const UsdShadeOutput a = split.CreateOutput(TfToken("outa"), SdfValueTypeNames->Float);

            UsdShadeShader rgb = _Shader(graphPath, "baseColorRgb", "ND_combine3_color3");
            rgb.CreateInput(TfToken("in1"), SdfValueTypeNames->Float).ConnectToSource(r);
            rgb.CreateInput(TfToken("in2"), SdfValueTypeNames->Float).ConnectToSource(g);
            rgb.CreateInput(TfToken("in3"), SdfValueTypeNames->Float).ConnectToSource(b);
            surface.GetInput(TfToken("emissive"))
                .ConnectToSource(rgb.CreateOutput(TfToken("out"), SdfValueTypeNames->Color3f));
            surface.GetInput(TfToken("alpha")).ConnectToSource(a);
        }

        const UsdShadeOutput shaderOut =
            surface.CreateOutput(TfToken("surface"), SdfValueTypeNames->Token);
        const UsdShadeOutput graphOut =
            graph.CreateOutput(TfToken("surface"), SdfValueTypeNames->Token);
        graphOut.ConnectToSource(shaderOut);
        material.CreateSurfaceOutput(TfToken("mtlx")).ConnectToSource(graphOut);

        if (material.GetPrim().ApplyAPI(TfToken("MaterialXConfigAPI"))) {
            material.GetPrim()
                .CreateAttribute(TfToken("config:mtlx:version"),
                                 SdfValueTypeNames->String,
                                 /*custom=*/false,
                                 SdfVariabilityUniform)
                .Set(std::string("1.39"));
        }
    }

    /// One UsdShadeMaterial per PMX material, in material-table order, with
    /// canonical semantics and the two portable realization graphs.
    void _Materials()
    {
        for (const mmd::Material& m : _doc.materials) {
            const SdfPath path = kMtlPath.AppendChild(TfToken(m.name.stableId));
            const UsdShadeMaterial material = UsdShadeMaterial::Define(_stage, path);
            const UsdPrim prim = material.GetPrim();
            prim.SetCustomDataByKey(kSourceNameKey, VtValue(m.name.source));
            prim.SetCustomDataByKey(kSourceEnglishNameKey, VtValue(m.name.english));
            prim.SetCustomDataByKey(kSourceIndexKey, VtValue(static_cast<int>(m.sourceIndex)));
            prim.SetCustomDataByKey(TfToken("mmd:sourceMemo"), VtValue(m.memo));

            _MaterialSemantics(prim, m);
            _TextureSlot(prim, m.texture, "mmd:material:texture", kSourceTexturePathKey);
            _TextureSlot(
                prim, m.sphereTexture, "mmd:material:sphereTexture", kSourceSphereTexturePathKey);
            if (m.toonSource == mmd::ToonSource::Individual) {
                _TextureSlot(
                    prim, m.toonTexture, "mmd:material:toonTexture", kSourceToonTexturePathKey);
            }
            _AuthorPreview(material, m);
            _AuthorMtlx(material, m);
        }
    }

    /// A texture slot: the verbatim source path as provenance whenever the
    /// slot names a texture, and the anchored asset path only when it is safe
    /// (TEXT_ENCODING_POLICY.md §7).
    void _TextureSlot(const UsdPrim& prim, std::int32_t texture, const char* attribute,
                      const TfToken& provenanceKey)
    {
        if (texture == mmd::kNone) {
            return;
        }
        const mmd::Texture& t = _doc.textures[static_cast<std::size_t>(texture)];
        prim.SetCustomDataByKey(provenanceKey, VtValue(t.sourcePath));
        if (!t.assetPath.empty()) {
            SetCustom(
                prim, attribute, SdfValueTypeNames->Asset, VtValue(SdfAssetPath(t.assetPath)));
        }
    }

    /// One materialBind subset per material that owns a face
    /// (STAGE_CONTRACT.md §8.1).
    void _Subsets(const UsdGeomMesh& mesh)
    {
        bool any = false;
        for (const mmd::Material& m : _doc.materials) {
            if (m.faceCount == 0) {
                continue;
            }
            VtIntArray faces(m.faceCount);
            for (std::size_t i = 0; i < m.faceCount; ++i) {
                faces[i] = static_cast<int>(m.firstFace + i);
            }
            const UsdGeomSubset subset =
                UsdGeomSubset::CreateGeomSubset(mesh,
                                                TfToken(m.name.stableId),
                                                UsdGeomTokens->face,
                                                faces,
                                                UsdShadeTokens->materialBind);
            const UsdShadeMaterial material(
                _stage->GetPrimAtPath(kMtlPath.AppendChild(TfToken(m.name.stableId))));
            UsdShadeMaterialBindingAPI::Apply(subset.GetPrim()).Bind(material);
            any = true;
        }
        if (any) {
            UsdGeomSubset::SetFamilyType(mesh,
                                         UsdShadeTokens->materialBind,
                                         _doc.mesh.materialsCoverFaces
                                             ? UsdGeomTokens->partition
                                             : UsdGeomTokens->nonOverlapping);
        }
    }

    /// The deformation skeleton (STAGE_CONTRACT.md §9.2, §9.3). PMX bones have
    /// a position and no orientation, so every rest and bind rotation is the
    /// identity.
    void _Skeleton()
    {
        const UsdSkelSkeleton skeleton = UsdSkelSkeleton::Define(_stage, kSkeletonPath);

        VtTokenArray joints;
        VtMatrix4dArray bind;
        VtMatrix4dArray rest;
        VtIntArray sourceIndex;
        VtStringArray sourceName;
        VtStringArray sourceEnglishName;
        for (const mmd::Bone& bone : _doc.skeleton.bones) {
            joints.push_back(TfToken(bone.jointPath));
            GfMatrix4d world(1.0);
            world.SetTranslateOnly(GfVec3d(bone.position[0], bone.position[1], bone.position[2]));
            bind.push_back(world);
            GfMatrix4d local(1.0);
            local.SetTranslateOnly(GfVec3d(
                bone.localTranslation[0], bone.localTranslation[1], bone.localTranslation[2]));
            rest.push_back(local);
            sourceIndex.push_back(static_cast<int>(bone.sourceIndex));
            sourceName.push_back(bone.name.source);
            sourceEnglishName.push_back(bone.name.english);
        }
        skeleton.CreateJointsAttr(VtValue(joints));
        skeleton.CreateBindTransformsAttr(VtValue(bind));
        skeleton.CreateRestTransformsAttr(VtValue(rest));

        const UsdPrim prim = skeleton.GetPrim();
        SetCustom(prim,
                  "mmd:bone:sourceIndex",
                  SdfValueTypeNames->IntArray,
                  VtValue(sourceIndex),
                  SdfVariabilityUniform);
        SetCustom(prim,
                  "mmd:bone:sourceName",
                  SdfValueTypeNames->StringArray,
                  VtValue(sourceName),
                  SdfVariabilityUniform);
        SetCustom(prim,
                  "mmd:bone:sourceEnglishName",
                  SdfValueTypeNames->StringArray,
                  VtValue(sourceEnglishName),
                  SdfVariabilityUniform);
    }

    /// Every morph, one prim under /Asset/morph in morph-table order
    /// (STAGE_CONTRACT.md §11): a UsdSkelBlendShape for a vertex morph, a
    /// typeless prim carrying `mmd:morph:*` for every other type. Nothing is
    /// evaluated -- no group is expanded, no bone morph moves the rest
    /// skeleton, no material morph edits a material.
    void _Morphs(const UsdGeomMesh& mesh, bool skinned)
    {
        // Blend shapes deform only beneath a SkelRoot, which a model with no
        // bones does not author, and only a mesh can name them
        // (STAGE_CONTRACT.md §4.1, §11.1). Either way the vertex morph falls
        // back to a typeless prim, and _ImporterDiagnostics records it.
        _blendShapes = skinned && bool(mesh);
        VtTokenArray names;
        SdfPathVector targets;
        for (const mmd::Morph& m : _doc.morphs) {
            const SdfPath path = kMorphPath.AppendChild(TfToken(m.name.stableId));
            const bool vertex = m.type == mmd::MorphType::Vertex;
            const UsdSkelBlendShape shape = vertex && _blendShapes
                                                ? UsdSkelBlendShape::Define(_stage, path)
                                                : UsdSkelBlendShape();
            const UsdPrim prim = shape ? shape.GetPrim() : _stage->DefinePrim(path);
            // The mesh must never name a blend shape that is not there, so
            // nothing is listed before the prim exists.
            if (!prim) {
                continue;
            }
            if (shape) {
                VtVec3fArray offsets;
                VtIntArray points;
                offsets.reserve(m.vertexOffsets.size());
                points.reserve(m.vertexOffsets.size());
                for (const mmd::MorphVertexOffset& o : m.vertexOffsets) {
                    offsets.push_back(GfVec3f(o.offset[0], o.offset[1], o.offset[2]));
                    points.push_back(o.vertex);
                }
                shape.CreateOffsetsAttr(VtValue(offsets));
                shape.CreatePointIndicesAttr(VtValue(points));
                names.push_back(TfToken(m.name.stableId));
                targets.push_back(path);
            }
            prim.SetCustomDataByKey(kSourceNameKey, VtValue(m.name.source));
            prim.SetCustomDataByKey(kSourceEnglishNameKey, VtValue(m.name.english));
            prim.SetCustomDataByKey(kSourceIndexKey, VtValue(static_cast<int>(m.sourceIndex)));
            _MorphToken(prim, "mmd:morph:type", MorphTypeName(m.type));
            _MorphToken(prim, "mmd:morph:panel", MorphPanelName(m.panel));

            switch (m.type) {
            case mmd::MorphType::Group:
            case mmd::MorphType::Flip:
                _MorphMembers(prim, m);
                break;
            case mmd::MorphType::Vertex:
                // The schema's own offsets when it is a blend shape, the same
                // two arrays under `mmd:morph:*` when it cannot be one.
                if (!shape) {
                    _MorphVertexOffsets(prim, m);
                }
                break;
            case mmd::MorphType::Bone:
                _MorphBoneOffsets(prim, m);
                break;
            case mmd::MorphType::Uv:
            case mmd::MorphType::AdditionalUv1:
            case mmd::MorphType::AdditionalUv2:
            case mmd::MorphType::AdditionalUv3:
            case mmd::MorphType::AdditionalUv4:
                _MorphUvOffsets(prim, m);
                break;
            case mmd::MorphType::Material:
                _MorphMaterialOffsets(prim, m);
                break;
            case mmd::MorphType::Impulse:
                _MorphImpulseOffsets(prim, m);
                break;
            }
        }
        if (names.empty()) {
            return;
        }
        // The mesh names its blend shapes in the same order as the prims it
        // targets (STAGE_CONTRACT.md §11).
        const UsdSkelBindingAPI binding = UsdSkelBindingAPI::Apply(mesh.GetPrim());
        binding.CreateBlendShapesAttr(VtValue(names));
        binding.CreateBlendShapeTargetsRel().SetTargets(targets);
    }

    void _MorphToken(const UsdPrim& prim, const char* name, const char* value)
    {
        SetCustom(
            prim, name, SdfValueTypeNames->Token, VtValue(TfToken(value)), SdfVariabilityUniform);
    }

    /// A morph's declarative payload: an array per field, parallel, in the
    /// source's offset order, and uniform because a PMX stage has no time
    /// samples (STAGE_CONTRACT.md §3).
    template <class Array>
    void _MorphArray(const UsdPrim& prim, const char* name, const SdfValueTypeName& type,
                     const Array& values)
    {
        SetCustom(prim, name, type, VtValue(values), SdfVariabilityUniform);
    }

    void _MorphMembers(const UsdPrim& prim, const mmd::Morph& m)
    {
        SdfPathVector members;
        VtFloatArray weights;
        members.reserve(m.members.size());
        weights.reserve(m.members.size());
        for (const mmd::MorphMember& member : m.members) {
            const mmd::Morph& target = _doc.morphs[static_cast<std::size_t>(member.morph)];
            members.push_back(kMorphPath.AppendChild(TfToken(target.name.stableId)));
            weights.push_back(member.weight);
        }
        prim.CreateRelationship(TfToken("mmd:morph:members"), /*custom=*/true).SetTargets(members);
        _MorphArray(prim, "mmd:morph:weights", SdfValueTypeNames->FloatArray, weights);
    }

    /// A vertex morph of a model with no skeleton: the same data a
    /// UsdSkelBlendShape would carry, preserved on a typeless prim.
    void _MorphVertexOffsets(const UsdPrim& prim, const mmd::Morph& m)
    {
        VtVec3fArray offsets;
        VtIntArray points;
        offsets.reserve(m.vertexOffsets.size());
        points.reserve(m.vertexOffsets.size());
        for (const mmd::MorphVertexOffset& o : m.vertexOffsets) {
            offsets.push_back(GfVec3f(o.offset[0], o.offset[1], o.offset[2]));
            points.push_back(o.vertex);
        }
        _MorphArray(prim, "mmd:morph:offsets", SdfValueTypeNames->Vector3fArray, offsets);
        _MorphArray(prim, "mmd:morph:pointIndices", SdfValueTypeNames->IntArray, points);
    }

    void _MorphBoneOffsets(const UsdPrim& prim, const mmd::Morph& m)
    {
        VtIntArray joints;
        VtVec3fArray translations;
        VtQuatfArray rotations;
        joints.reserve(m.boneOffsets.size());
        translations.reserve(m.boneOffsets.size());
        rotations.reserve(m.boneOffsets.size());
        for (const mmd::MorphBoneOffset& o : m.boneOffsets) {
            joints.push_back(o.joint);
            translations.push_back(GfVec3f(o.translation[0], o.translation[1], o.translation[2]));
            rotations.push_back(
                GfQuatf(o.rotation[3], o.rotation[0], o.rotation[1], o.rotation[2]));
        }
        _MorphArray(prim, "mmd:morph:joints", SdfValueTypeNames->IntArray, joints);
        _MorphArray(prim, "mmd:morph:translations", SdfValueTypeNames->Vector3fArray, translations);
        _MorphArray(prim, "mmd:morph:rotations", SdfValueTypeNames->QuatfArray, rotations);
    }

    void _MorphUvOffsets(const UsdPrim& prim, const mmd::Morph& m)
    {
        VtIntArray points;
        VtVec4fArray deltas;
        points.reserve(m.uvOffsets.size());
        deltas.reserve(m.uvOffsets.size());
        for (const mmd::MorphUvOffset& o : m.uvOffsets) {
            points.push_back(o.vertex);
            deltas.push_back(GfVec4f(o.delta[0], o.delta[1], o.delta[2], o.delta[3]));
        }
        _MorphArray(prim, "mmd:morph:pointIndices", SdfValueTypeNames->IntArray, points);
        _MorphArray(prim, "mmd:morph:uvOffsets", SdfValueTypeNames->Float4Array, deltas);
    }

    void _MorphMaterialOffsets(const UsdPrim& prim, const mmd::Morph& m)
    {
        const std::size_t n = m.materialOffsets.size();
        VtIntArray materials;
        VtTokenArray operations;
        VtVec4fArray diffuse;
        VtVec3fArray specular;
        VtFloatArray specularPower;
        VtVec3fArray ambient;
        VtVec4fArray edgeColor;
        VtFloatArray edgeSize;
        VtVec4fArray textureTint;
        VtVec4fArray sphereTint;
        VtVec4fArray toonTint;
        materials.reserve(n);
        operations.reserve(n);
        for (const mmd::MorphMaterialOffset& o : m.materialOffsets) {
            // -1 is "every material", as it is in the source (PMX §10).
            materials.push_back(o.material);
            operations.push_back(
                TfToken(o.operation == mmd::MaterialMorphOperation::Add ? "add" : "multiply"));
            diffuse.push_back(GfVec4f(
                o.diffuseColor[0], o.diffuseColor[1], o.diffuseColor[2], o.diffuseColor[3]));
            specular.push_back(GfVec3f(o.specularColor[0], o.specularColor[1], o.specularColor[2]));
            specularPower.push_back(o.specularPower);
            ambient.push_back(GfVec3f(o.ambientColor[0], o.ambientColor[1], o.ambientColor[2]));
            edgeColor.push_back(
                GfVec4f(o.edgeColor[0], o.edgeColor[1], o.edgeColor[2], o.edgeColor[3]));
            edgeSize.push_back(o.edgeSize);
            textureTint.push_back(
                GfVec4f(o.textureTint[0], o.textureTint[1], o.textureTint[2], o.textureTint[3]));
            sphereTint.push_back(
                GfVec4f(o.sphereTint[0], o.sphereTint[1], o.sphereTint[2], o.sphereTint[3]));
            toonTint.push_back(GfVec4f(o.toonTint[0], o.toonTint[1], o.toonTint[2], o.toonTint[3]));
        }
        _MorphArray(prim, "mmd:morph:materialIndices", SdfValueTypeNames->IntArray, materials);
        _MorphArray(
            prim, "mmd:morph:materialOperations", SdfValueTypeNames->TokenArray, operations);
        _MorphArray(prim, "mmd:morph:diffuseColors", SdfValueTypeNames->Color4fArray, diffuse);
        _MorphArray(prim, "mmd:morph:specularColors", SdfValueTypeNames->Color3fArray, specular);
        _MorphArray(prim, "mmd:morph:specularPowers", SdfValueTypeNames->FloatArray, specularPower);
        _MorphArray(prim, "mmd:morph:ambientColors", SdfValueTypeNames->Color3fArray, ambient);
        _MorphArray(prim, "mmd:morph:edgeColors", SdfValueTypeNames->Color4fArray, edgeColor);
        _MorphArray(prim, "mmd:morph:edgeSizes", SdfValueTypeNames->FloatArray, edgeSize);
        _MorphArray(prim, "mmd:morph:textureTints", SdfValueTypeNames->Float4Array, textureTint);
        _MorphArray(prim, "mmd:morph:sphereTints", SdfValueTypeNames->Float4Array, sphereTint);
        _MorphArray(prim, "mmd:morph:toonTints", SdfValueTypeNames->Float4Array, toonTint);
    }

    /// An impulse morph names rigid bodies, which are Phase 6's: until
    /// /Asset/physics exists, each one is kept by its source-table index.
    void _MorphImpulseOffsets(const UsdPrim& prim, const mmd::Morph& m)
    {
        VtIntArray bodies;
        VtBoolArray local;
        VtVec3fArray velocities;
        VtVec3fArray torques;
        bodies.reserve(m.impulseOffsets.size());
        local.reserve(m.impulseOffsets.size());
        velocities.reserve(m.impulseOffsets.size());
        torques.reserve(m.impulseOffsets.size());
        for (const mmd::MorphImpulseOffset& o : m.impulseOffsets) {
            bodies.push_back(o.rigidBody);
            local.push_back(o.local);
            velocities.push_back(GfVec3f(o.velocity[0], o.velocity[1], o.velocity[2]));
            torques.push_back(GfVec3f(o.torque[0], o.torque[1], o.torque[2]));
        }
        _MorphArray(prim, "mmd:morph:rigidBodyIndices", SdfValueTypeNames->IntArray, bodies);
        _MorphArray(prim, "mmd:morph:impulseLocal", SdfValueTypeNames->BoolArray, local);
        _MorphArray(prim, "mmd:morph:velocities", SdfValueTypeNames->Vector3fArray, velocities);
        _MorphArray(prim, "mmd:morph:torques", SdfValueTypeNames->Vector3fArray, torques);
    }

    /// The control rig (STAGE_CONTRACT.md §12): every joint's control
    /// semantics as arrays parallel to the Skeleton's joints on
    /// /Asset/rig/Bones, and one typeless prim per IK chain under
    /// /Asset/rig/ik. Every joint index is canonical. Nothing is solved or
    /// applied, and the joint hierarchy is not duplicated.
    void _Rig()
    {
        const UsdPrim bones = _stage->DefinePrim(kRigBonesPath);
        if (!bones) {
            return;
        }
        VtIntArray layers, tailJoints, appendSources, externalParentKeys;
        VtBoolArray afterPhysics, rotatable, translatable, visible, operable;
        VtBoolArray appendRotation, appendTranslation, appendLocal;
        VtBoolArray hasFixedAxis, hasLocalAxes, hasExternalParent;
        VtFloatArray appendRatios;
        VtVec3fArray tailOffsets, fixedAxes, localAxesX, localAxesZ;
        const auto vec = [](const mmd::Float3& v) { return GfVec3f(v[0], v[1], v[2]); };
        for (const mmd::BoneControl& b : _doc.rig.bones) {
            layers.push_back(b.transformLayer);
            afterPhysics.push_back(b.deformAfterPhysics);
            rotatable.push_back(b.rotatable);
            translatable.push_back(b.translatable);
            visible.push_back(b.visible);
            operable.push_back(b.operable);
            tailJoints.push_back(b.tailJoint);
            tailOffsets.push_back(vec(b.tailOffset));
            appendSources.push_back(b.appendSource);
            appendRatios.push_back(b.appendRatio);
            appendRotation.push_back(b.appendRotation);
            appendTranslation.push_back(b.appendTranslation);
            appendLocal.push_back(b.appendLocal);
            hasFixedAxis.push_back(b.hasFixedAxis);
            fixedAxes.push_back(vec(b.fixedAxis));
            hasLocalAxes.push_back(b.hasLocalAxes);
            localAxesX.push_back(vec(b.localAxisX));
            localAxesZ.push_back(vec(b.localAxisZ));
            hasExternalParent.push_back(b.hasExternalParent);
            externalParentKeys.push_back(b.externalParentKey);
        }
        const auto uniform =
            [&](const char* name, const SdfValueTypeName& type, const VtValue& value) {
                SetCustom(bones, name, type, value, SdfVariabilityUniform);
            };
        const SdfValueTypeName& ints = SdfValueTypeNames->IntArray;
        const SdfValueTypeName& bools = SdfValueTypeNames->BoolArray;
        const SdfValueTypeName& vectors = SdfValueTypeNames->Vector3fArray;
        uniform("mmd:rig:transformLayers", ints, VtValue(layers));
        uniform("mmd:rig:deformAfterPhysics", bools, VtValue(afterPhysics));
        uniform("mmd:rig:rotatable", bools, VtValue(rotatable));
        uniform("mmd:rig:translatable", bools, VtValue(translatable));
        uniform("mmd:rig:visible", bools, VtValue(visible));
        uniform("mmd:rig:operable", bools, VtValue(operable));
        uniform("mmd:rig:tailJoints", ints, VtValue(tailJoints));
        uniform("mmd:rig:tailOffsets", vectors, VtValue(tailOffsets));
        uniform("mmd:rig:appendSources", ints, VtValue(appendSources));
        uniform("mmd:rig:appendRatios", SdfValueTypeNames->FloatArray, VtValue(appendRatios));
        uniform("mmd:rig:appendRotation", bools, VtValue(appendRotation));
        uniform("mmd:rig:appendTranslation", bools, VtValue(appendTranslation));
        uniform("mmd:rig:appendLocal", bools, VtValue(appendLocal));
        uniform("mmd:rig:hasFixedAxis", bools, VtValue(hasFixedAxis));
        uniform("mmd:rig:fixedAxes", vectors, VtValue(fixedAxes));
        uniform("mmd:rig:hasLocalAxes", bools, VtValue(hasLocalAxes));
        uniform("mmd:rig:localAxesX", vectors, VtValue(localAxesX));
        uniform("mmd:rig:localAxesZ", vectors, VtValue(localAxesZ));
        uniform("mmd:rig:hasExternalParent", bools, VtValue(hasExternalParent));
        uniform("mmd:rig:externalParentKeys", ints, VtValue(externalParentKeys));

        if (_doc.rig.ikChains.empty()) {
            return;
        }
        UsdGeomScope::Define(_stage, kRigIkPath);
        for (const mmd::IkChain& chain : _doc.rig.ikChains) {
            const mmd::Bone& bone = _doc.skeleton.bones[static_cast<std::size_t>(chain.joint)];
            const UsdPrim prim =
                _stage->DefinePrim(kRigIkPath.AppendChild(TfToken(bone.name.stableId)));
            if (!prim) {
                continue;
            }
            // Provenance is the IK bone's, the element the chain comes from.
            prim.SetCustomDataByKey(kSourceNameKey, VtValue(bone.name.source));
            prim.SetCustomDataByKey(kSourceEnglishNameKey, VtValue(bone.name.english));
            prim.SetCustomDataByKey(kSourceIndexKey, VtValue(static_cast<int>(bone.sourceIndex)));

            VtIntArray linkJoints;
            VtBoolArray linkHasLimits;
            VtVec3fArray lower, upper;
            for (const mmd::IkLink& link : chain.links) {
                linkJoints.push_back(link.joint);
                linkHasLimits.push_back(link.hasLimits);
                lower.push_back(vec(link.lowerLimit));
                upper.push_back(vec(link.upperLimit));
            }
            const auto set =
                [&](const char* name, const SdfValueTypeName& type, const VtValue& value) {
                    SetCustom(prim, name, type, value, SdfVariabilityUniform);
                };
            set("mmd:rig:joint", SdfValueTypeNames->Int, VtValue(chain.joint));
            set("mmd:rig:effector", SdfValueTypeNames->Int, VtValue(chain.effector));
            set("mmd:rig:loopCount", SdfValueTypeNames->Int, VtValue(chain.loopCount));
            set("mmd:rig:limitAngle", SdfValueTypeNames->Float, VtValue(chain.limitAngle));
            set("mmd:rig:linkJoints", ints, VtValue(linkJoints));
            set("mmd:rig:linkHasLimits", bools, VtValue(linkHasLimits));
            set("mmd:rig:linkLowerLimits", vectors, VtValue(lower));
            set("mmd:rig:linkUpperLimits", vectors, VtValue(upper));
        }
    }

    /// What the stage approximates or leaves out, said once per import.
    void _ImporterDiagnostics(bool skinned)
    {
        const mmd::Mesh& m = _doc.mesh;
        const auto vertices = [](std::size_t n) {
            return n == 1 ? std::string("1 vertex is") : std::to_string(n) + " vertices are";
        };
        mmd::Location onVertices;
        onVertices.table = "vertices";
        if (skinned && m.sdefVertexCount > 0) {
            _diagnostics.push_back(
                mmd::MakeDiagnostic(codes::SkelSdefApproximated,
                                    vertices(m.sdefVertexCount) +
                                        " SDEF, skinned here by linear blending; C, R0 and "
                                        "R1 are preserved in primvars:mmd:sdefC, sdefR0 and sdefR1",
                                    onVertices));
        }
        if (skinned && m.qdefVertexCount > 0) {
            _diagnostics.push_back(
                mmd::MakeDiagnostic(codes::SkelQdefApproximated,
                                    vertices(m.qdefVertexCount) +
                                        " QDEF, skinned here by linear blending; no "
                                        "dual-quaternion consumer has been verified against it",
                                    onVertices));
        }
        // Vertex morphs are blend shapes only beneath a SkelRoot that holds a
        // mesh to name them: a model with no bones authors none, and neither
        // does one with no mesh (STAGE_CONTRACT.md §4.1, §11.1). The
        // condition is the one _Morphs authored by, so the fallback is never
        // silent.
        if (!_blendShapes) {
            std::size_t preserved = 0;
            for (const mmd::Morph& morph : _doc.morphs) {
                if (morph.type == mmd::MorphType::Vertex) {
                    ++preserved;
                }
            }
            if (preserved > 0) {
                mmd::Location where;
                where.table = "morphs";
                _diagnostics.push_back(mmd::MakeDiagnostic(
                    codes::MorphNoSkeleton,
                    (preserved == 1 ? std::string("1 vertex morph is")
                                    : std::to_string(preserved) + " vertex morphs are") +
                        " preserved without a UsdSkelBlendShape: " +
                        (skinned ? "the model has no mesh to name one"
                                 : "the model has no bones, so /Asset is no SkelRoot") +
                        " and a blend shape would not deform",
                    std::move(where)));
            }
        }
        // Rigid bodies and joints are Phase 6's and raise nothing yet: they
        // are reserved, not refused (PMX_CONTRACT.md §12).
        if (const std::size_t n = _doc.metadata.softBodyCount; n > 0) {
            mmd::Location where;
            where.table = "softBodies";
            _diagnostics.push_back(mmd::MakeDiagnostic(
                codes::PhysicsSoftBodyUnsupported,
                (n == 1 ? std::string("1 soft body is") : std::to_string(n) + " soft bodies are") +
                    " read and not authored; no stage contract carries soft bodies",
                std::move(where)));
        }
    }

    /// Every recoverable diagnostic, in emission order, as "CODE: message".
    /// The key is authored only when there is something to record.
    void _RecordDiagnostics(const UsdPrim& asset)
    {
        if (_diagnostics.empty()) {
            return;
        }
        VtStringArray recorded;
        recorded.reserve(_diagnostics.size());
        for (const mmd::Diagnostic& d : _diagnostics) {
            recorded.push_back(mmd::FormatDiagnostic(d));
        }
        asset.SetCustomDataByKey(kDiagnosticsKey, VtValue(recorded));
    }

    const mmd::CanonicalDocument& _doc;
    std::vector<mmd::Diagnostic>& _diagnostics;
    UsdStageRefPtr _stage;
    /// Whether _Morphs authored vertex morphs as UsdSkelBlendShapes; false
    /// until it runs, which is only when the model has a morph.
    bool _blendShapes = false;
};

} // namespace

bool
UsdMmdAuthorer::WriteToString(const mmd::CanonicalDocument& document,
                              std::vector<mmd::Diagnostic>* diagnostics, std::string* outUsda) const
{
    if (!diagnostics || !outUsda) {
        return false;
    }
    return Authorer(document, *diagnostics).Author(outUsda);
}

} // namespace usdmmd
