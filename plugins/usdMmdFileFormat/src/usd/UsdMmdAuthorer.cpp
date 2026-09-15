// SPDX-License-Identifier: Apache-2.0
#include "usd/UsdMmdAuthorer.h"

#include "usd/UsdMmdCodes.h"

#include "pxr/base/gf/matrix4d.h"
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
#include "pxr/usd/usdShade/tokens.h"
#include "pxr/usd/usdSkel/bindingAPI.h"
#include "pxr/usd/usdSkel/root.h"
#include "pxr/usd/usdSkel/skeleton.h"

#include <cstddef>
#include <cstdint>
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
SetCustom(const UsdPrim& prim, const char* name, const SdfValueTypeName& type,
    const VtValue& value, SdfVariability variability = SdfVariabilityVarying)
{
    prim.CreateAttribute(TfToken(name), type, /*custom=*/true, variability).Set(value);
}

/// A vertex-interpolated primvar in the `mmd:` namespace (STAGE_CONTRACT.md §3).
void
SetVertexPrimvar(const UsdGeomPrimvarsAPI& primvars, const char* name,
    const SdfValueTypeName& type, const VtValue& value)
{
    primvars.CreatePrimvar(TfToken(name), type, UsdGeomTokens->vertex).Set(value);
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
        SetVertexPrimvar(primvars, "st", SdfValueTypeNames->TexCoord2fArray,
            VtValue(ToVtArray<GfVec2f>(m.st)));
        for (std::size_t k = 0; k < m.additionalUvCount; ++k) {
            const std::string name = "mmd:uv" + std::to_string(k + 1);
            SetVertexPrimvar(primvars, name.c_str(), SdfValueTypeNames->Float4Array,
                VtValue(ToVtArray<GfVec4f>(m.additionalUv[k])));
        }
        SetVertexPrimvar(primvars, "mmd:edgeScale", SdfValueTypeNames->FloatArray,
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
            SetVertexPrimvar(primvars, "mmd:deformType", SdfValueTypeNames->IntArray,
                VtValue(deformTypes));
            if (!m.sdefC.empty()) {
                SetVertexPrimvar(primvars, "mmd:sdefC", SdfValueTypeNames->Point3fArray,
                    VtValue(ToVtArray<GfVec3f>(m.sdefC)));
                SetVertexPrimvar(primvars, "mmd:sdefR0", SdfValueTypeNames->Point3fArray,
                    VtValue(ToVtArray<GfVec3f>(m.sdefR0)));
                SetVertexPrimvar(primvars, "mmd:sdefR1", SdfValueTypeNames->Point3fArray,
                    VtValue(ToVtArray<GfVec3f>(m.sdefR1)));
            }
        }
        return mesh;
    }

    /// One UsdShadeMaterial per PMX material, in material-table order, with
    /// its provenance and the semantics the stage carries so far
    /// (STAGE_CONTRACT.md §10, MATERIAL_POLICY.md §4).
    void _Materials()
    {
        for (const mmd::Material& m : _doc.materials) {
            const SdfPath path = kMtlPath.AppendChild(TfToken(m.name.stableId));
            const UsdPrim prim = UsdShadeMaterial::Define(_stage, path).GetPrim();
            prim.SetCustomDataByKey(kSourceNameKey, VtValue(m.name.source));
            prim.SetCustomDataByKey(kSourceEnglishNameKey, VtValue(m.name.english));
            prim.SetCustomDataByKey(kSourceIndexKey, VtValue(static_cast<int>(m.sourceIndex)));

            SetCustom(prim, "mmd:material:doubleSided", SdfValueTypeNames->Bool,
                VtValue(m.doubleSided), SdfVariabilityUniform);
            _TextureSlot(prim, m.texture, "mmd:material:texture", kSourceTexturePathKey);
            _TextureSlot(prim, m.sphereTexture, "mmd:material:sphereTexture",
                kSourceSphereTexturePathKey);
            _TextureSlot(prim, m.toonTexture, "mmd:material:toonTexture",
                kSourceToonTexturePathKey);
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
            SetCustom(prim, attribute, SdfValueTypeNames->Asset,
                VtValue(SdfAssetPath(t.assetPath)));
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
            const UsdGeomSubset subset = UsdGeomSubset::CreateGeomSubset(mesh,
                TfToken(m.name.stableId), UsdGeomTokens->face, faces,
                UsdShadeTokens->materialBind);
            const UsdShadeMaterial material(
                _stage->GetPrimAtPath(kMtlPath.AppendChild(TfToken(m.name.stableId))));
            UsdShadeMaterialBindingAPI::Apply(subset.GetPrim()).Bind(material);
            any = true;
        }
        if (any) {
            UsdGeomSubset::SetFamilyType(mesh, UsdShadeTokens->materialBind,
                _doc.mesh.materialsCoverFaces ? UsdGeomTokens->partition
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
            local.SetTranslateOnly(GfVec3d(bone.localTranslation[0], bone.localTranslation[1],
                bone.localTranslation[2]));
            rest.push_back(local);
            sourceIndex.push_back(static_cast<int>(bone.sourceIndex));
            sourceName.push_back(bone.name.source);
            sourceEnglishName.push_back(bone.name.english);
        }
        skeleton.CreateJointsAttr(VtValue(joints));
        skeleton.CreateBindTransformsAttr(VtValue(bind));
        skeleton.CreateRestTransformsAttr(VtValue(rest));

        const UsdPrim prim = skeleton.GetPrim();
        SetCustom(prim, "mmd:bone:sourceIndex", SdfValueTypeNames->IntArray,
            VtValue(sourceIndex), SdfVariabilityUniform);
        SetCustom(prim, "mmd:bone:sourceName", SdfValueTypeNames->StringArray,
            VtValue(sourceName), SdfVariabilityUniform);
        SetCustom(prim, "mmd:bone:sourceEnglishName", SdfValueTypeNames->StringArray,
            VtValue(sourceEnglishName), SdfVariabilityUniform);
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
            _diagnostics.push_back(mmd::MakeDiagnostic(codes::SkelSdefApproximated,
                vertices(m.sdefVertexCount) + " SDEF, skinned here by linear blending; C, R0 and "
                    "R1 are preserved in primvars:mmd:sdefC, sdefR0 and sdefR1",
                onVertices));
        }
        if (skinned && m.qdefVertexCount > 0) {
            _diagnostics.push_back(mmd::MakeDiagnostic(codes::SkelQdefApproximated,
                vertices(m.qdefVertexCount) + " QDEF, skinned here by linear blending; no "
                    "dual-quaternion consumer has been verified against it",
                onVertices));
        }
        // Rigid bodies and joints are Phase 6's and raise nothing yet: they
        // are reserved, not refused (PMX_CONTRACT.md §12).
        if (const std::size_t n = _doc.metadata.softBodyCount; n > 0) {
            mmd::Location where;
            where.table = "softBodies";
            _diagnostics.push_back(mmd::MakeDiagnostic(codes::PhysicsSoftBodyUnsupported,
                (n == 1 ? std::string("1 soft body is") : std::to_string(n) + " soft bodies are")
                    + " read and not authored; no stage contract carries soft bodies",
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
};

}  // namespace

bool
UsdMmdAuthorer::WriteToString(
    const mmd::CanonicalDocument& document,
    std::vector<mmd::Diagnostic>* diagnostics,
    std::string* outUsda) const
{
    if (!diagnostics || !outUsda) {
        return false;
    }
    return Authorer(document, *diagnostics).Author(outUsda);
}

}  // namespace usdmmd
