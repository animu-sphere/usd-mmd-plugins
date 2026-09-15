// SPDX-License-Identifier: Apache-2.0
#include "mmdModel/Canonicalize.h"

#include "mmdModel/Basis.h"
#include "mmdModel/Codes.h"

#include "Identifiers.h"
#include "Skeleton.h"
#include "TexturePaths.h"

#include <mmdPmx/DiagnosticList.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mmd {

namespace {

/// The tolerance within which a vertex's weights already sum to 1
/// (STAGE_CONTRACT.md §9.5; STAGE-O5).
constexpr double kWeightTolerance = 1e-5;

Location
At(const char* table, std::optional<std::size_t> index = {}, const char* field = "")
{
    Location where;
    where.table = table;
    if (index) {
        where.index = *index;
    }
    where.field = field;
    return where;
}

/// A display name with the trailing U+0000 padding some writers add dropped,
/// and MMD_TEXT_TRAILING_NUL when there was any (TEXT_ENCODING_POLICY.md §3).
/// The parser has already validated the UTF-8, where U+0000 is one zero byte.
std::string
DisplayName(const std::string& text, Location where, DiagnosticList& diagnostics)
{
    std::size_t end = text.size();
    while (end > 0 && text[end - 1] == '\0') {
        --end;
    }
    if (end != text.size()) {
        const std::size_t dropped = text.size() - end;
        diagnostics.Add(codes::TextTrailingNul,
            std::to_string(dropped) + " trailing U+0000 "
                + (dropped == 1 ? "character is" : "characters are")
                + " dropped from a display name",
            std::move(where));
    }
    return text.substr(0, end);
}

Float3
ToFloat3(const pmx::Vec3& v)
{
    return {v[0], v[1], v[2]};
}

std::size_t
InfluenceSlots(pmx::DeformType type)
{
    switch (type) {
    case pmx::DeformType::Bdef1:
        return 1;
    case pmx::DeformType::Bdef2:
    case pmx::DeformType::Sdef:
        return 2;
    case pmx::DeformType::Bdef4:
    case pmx::DeformType::Qdef:
        return 4;
    }
    return 4;
}

class Canonicalizer {
public:
    explicit Canonicalizer(const pmx::Document& document) : _doc(document) {}

    Result<CanonicalDocument> Run()
    {
        _Metadata();
        _Identity();          // PMX_CONTRACT.md §14 step 1
        _Skeleton();          // steps 2 and 3, for bones
        _MeshAndSkinning();   // steps 2, 4 and 5
        _Materials();         // step 6
        return Result<CanonicalDocument>::Success(std::move(_out), _diagnostics.Take());
    }

private:
    void _Metadata()
    {
        Metadata& m = _out.metadata;
        m.sourceVersion = std::string(pmx::ToString(_doc.header.version));
        m.name = DisplayName(_doc.model.name, At("model", {}, "name"), _diagnostics);
        m.englishName =
            DisplayName(_doc.model.englishName, At("model", {}, "englishName"), _diagnostics);
        m.comment = _doc.model.comment;
        m.englishComment = _doc.model.englishComment;
        m.softBodyCount = _doc.softBodies.size();
    }

    /// Display names and stable identifiers for every element kind the stage
    /// authors, each kind in source order (TEXT_ENCODING_POLICY.md §6).
    void _Identity()
    {
        _materialNames = _Names(_doc.materials, "materials", "material");
        _boneNames = _Names(_doc.bones, "bones", "bone");
    }

    template <class Element>
    std::vector<Name> _Names(const std::vector<Element>& table, const char* tableName,
        const char* kind)
    {
        std::vector<Name> names(table.size());
        std::vector<std::string> english(table.size());
        for (std::size_t i = 0; i < table.size(); ++i) {
            names[i].source = DisplayName(table[i].name, At(tableName, i, "name"), _diagnostics);
            names[i].english =
                DisplayName(table[i].englishName, At(tableName, i, "englishName"), _diagnostics);
            english[i] = names[i].english;
        }
        const std::vector<std::string> ids =
            detail::AssignIdentifiers(kind, english, tableName, _diagnostics);
        for (std::size_t i = 0; i < table.size(); ++i) {
            names[i].stableId = ids[i];
        }
        return names;
    }

    /// The deformation skeleton from positions and parents only
    /// (PMX_CONTRACT.md §9), in canonical joint order (STAGE_CONTRACT.md §9.1).
    void _Skeleton()
    {
        const std::size_t n = _doc.bones.size();
        std::vector<std::int32_t> parents(n);
        for (std::size_t i = 0; i < n; ++i) {
            parents[i] = _doc.bones[i].parent;
        }
        const detail::JointOrder order = detail::OrderJoints(parents, _diagnostics);

        Skeleton& skeleton = _out.skeleton;
        skeleton.jointOfSourceBone.assign(n, kNone);
        for (std::size_t c = 0; c < n; ++c) {
            skeleton.jointOfSourceBone[order.order[c]] = static_cast<std::int32_t>(c);
        }
        skeleton.bones.resize(n);
        for (std::size_t c = 0; c < n; ++c) {
            const std::size_t source = order.order[c];
            Bone& bone = skeleton.bones[c];
            bone.name = _boneNames[source];
            bone.sourceIndex = source;
            const std::int32_t parent = order.parents[source];
            bone.parent = parent == -1 ? kNone
                                       : skeleton.jointOfSourceBone[static_cast<std::size_t>(parent)];
            bone.jointPath = bone.parent == kNone
                ? bone.name.stableId
                : skeleton.bones[static_cast<std::size_t>(bone.parent)].jointPath + "/"
                    + bone.name.stableId;
            // The rest translation is the source offset, taken in double from
            // the floats as stored (exactly, for any two positions within a
            // model's scale), and converted once.
            const pmx::Vec3& p = _doc.bones[source].position;
            bone.position = basis::PointD({p[0], p[1], p[2]});
            if (parent == -1) {
                bone.localTranslation = bone.position;
            } else {
                const pmx::Vec3& q = _doc.bones[static_cast<std::size_t>(parent)].position;
                bone.localTranslation = basis::PointD({static_cast<double>(p[0]) - q[0],
                    static_cast<double>(p[1]) - q[1], static_cast<double>(p[2]) - q[2]});
            }
        }
    }

    void _MeshAndSkinning()
    {
        const std::size_t n = _doc.vertices.size();
        Mesh& mesh = _out.mesh;
        mesh.additionalUvCount = std::min<std::uint8_t>(_doc.header.globals.additionalVec4Count, 4);
        mesh.points.reserve(n);
        mesh.normals.reserve(n);
        mesh.st.reserve(n);
        mesh.edgeScale.reserve(n);
        for (std::size_t k = 0; k < mesh.additionalUvCount; ++k) {
            mesh.additionalUv[k].reserve(n);
        }
        for (const pmx::Vertex& v : _doc.vertices) {
            mesh.points.push_back(basis::Point(ToFloat3(v.position)));
            mesh.normals.push_back(basis::Direction(ToFloat3(v.normal)));
            mesh.st.push_back(basis::St({v.uv[0], v.uv[1]}));
            for (std::size_t k = 0; k < mesh.additionalUvCount; ++k) {
                const pmx::Vec4& a = v.additionalVec4[k];
                mesh.additionalUv[k].push_back({a[0], a[1], a[2], a[3]});
            }
            mesh.edgeScale.push_back(v.edgeScale);
        }

        // Triangles, winding reversed. Every face index names a vertex and the
        // count is a multiple of 3 in any document the parser returns; a stray
        // tail would be dropped rather than read past.
        const std::size_t triangles = _doc.faces.size() / 3;
        mesh.faceVertexIndices.reserve(triangles * 3);
        for (std::size_t t = 0; t < triangles; ++t) {
            const auto tri = basis::Triangle(static_cast<std::int32_t>(_doc.faces[3 * t]),
                static_cast<std::int32_t>(_doc.faces[3 * t + 1]),
                static_cast<std::int32_t>(_doc.faces[3 * t + 2]));
            mesh.faceVertexIndices.insert(mesh.faceVertexIndices.end(), tri.begin(), tri.end());
        }

        if (!_doc.bones.empty()) {
            _Skinning(mesh);
        }
    }

    /// Explicit influences in canonical joint order, normalized
    /// (STAGE_CONTRACT.md §9.4, §9.5; PMX_CONTRACT.md §5).
    void _Skinning(Mesh& mesh)
    {
        const std::size_t n = _doc.vertices.size();
        const std::size_t bones = _doc.bones.size();
        const std::vector<std::int32_t>& jointOf = _out.skeleton.jointOfSourceBone;

        std::size_t slots = 0;
        for (const pmx::Vertex& v : _doc.vertices) {
            slots = std::max(slots, InfluenceSlots(v.deform.type));
        }
        mesh.influencesPerVertex = slots;
        mesh.deformTypes.reserve(n);
        mesh.jointIndices.assign(n * slots, 0);
        mesh.jointWeights.assign(n * slots, 0.0f);

        std::size_t normalized = 0;
        std::size_t zero = 0;
        std::optional<std::size_t> firstNormalized;
        std::optional<std::size_t> firstZero;

        struct Influence {
            std::int32_t joint;
            double weight;
        };
        std::vector<Influence> kept;
        for (std::size_t i = 0; i < n; ++i) {
            const pmx::Deform& d = _doc.vertices[i].deform;
            mesh.deformTypes.push_back(static_cast<DeformType>(d.type));

            // The influences as stored: BDEF1's implicit weight is 1, BDEF2's
            // and SDEF's second is 1 - weight1.
            std::array<std::int32_t, 4> stored{kNone, kNone, kNone, kNone};
            std::array<double, 4> weights{};
            const std::size_t count = InfluenceSlots(d.type);
            switch (count) {
            case 1:
                stored[0] = d.bones[0];
                weights[0] = 1.0;
                break;
            case 2:
                stored = {d.bones[0], d.bones[1], kNone, kNone};
                weights = {d.weights[0], 1.0 - static_cast<double>(d.weights[0]), 0.0, 0.0};
                break;
            default:
                stored = d.bones;
                weights = {d.weights[0], d.weights[1], d.weights[2], d.weights[3]};
                break;
            }

            // A bone that names nothing is dropped whatever its weight (the
            // parser reported the weighted ones); a negative or non-finite
            // weight is clamped to 0 before either rule.
            kept.clear();
            std::int32_t firstBone = kNone;
            for (std::size_t k = 0; k < count; ++k) {
                const std::int32_t b = stored[k];
                if (b < 0 || static_cast<std::size_t>(b) >= bones) {
                    continue;
                }
                const std::int32_t joint = jointOf[static_cast<std::size_t>(b)];
                if (firstBone == kNone) {
                    firstBone = joint;
                }
                const double w = std::isfinite(weights[k]) && weights[k] > 0.0 ? weights[k] : 0.0;
                kept.push_back({joint, w});
            }
            double sum = 0.0;
            for (const Influence& influence : kept) {
                sum += influence.weight;
            }
            if (sum == 0.0) {
                // Bound fully to its first bone, or to the root joint when no
                // stored bone names one.
                kept.assign(1, Influence{firstBone == kNone ? 0 : firstBone, 1.0});
                ++zero;
                if (!firstZero) {
                    firstZero = i;
                }
            } else if (std::abs(sum - 1.0) > kWeightTolerance) {
                for (Influence& influence : kept) {
                    influence.weight /= sum;
                }
                ++normalized;
                if (!firstNormalized) {
                    firstNormalized = i;
                }
            }
            for (std::size_t k = 0; k < kept.size(); ++k) {
                mesh.jointIndices[i * slots + k] = kept[k].joint;
                mesh.jointWeights[i * slots + k] = static_cast<float>(kept[k].weight);
            }

            if (d.type == pmx::DeformType::Sdef) {
                ++mesh.sdefVertexCount;
            } else if (d.type == pmx::DeformType::Qdef) {
                ++mesh.qdefVertexCount;
            }
        }

        if (mesh.sdefVertexCount > 0) {
            mesh.sdefC.assign(n, Float3{});
            mesh.sdefR0.assign(n, Float3{});
            mesh.sdefR1.assign(n, Float3{});
            for (std::size_t i = 0; i < n; ++i) {
                const pmx::Deform& d = _doc.vertices[i].deform;
                if (d.type == pmx::DeformType::Sdef) {
                    mesh.sdefC[i] = basis::Point(ToFloat3(d.sdefC));
                    mesh.sdefR0[i] = basis::Point(ToFloat3(d.sdefR0));
                    mesh.sdefR1[i] = basis::Point(ToFloat3(d.sdefR1));
                }
            }
        }

        // Once per import, with the count (DIAGNOSTICS.md §4).
        if (normalized > 0) {
            _diagnostics.Add(codes::SkelWeightsNormalized,
                std::to_string(normalized) + (normalized == 1 ? " vertex's" : " vertices'")
                    + " weights are rescaled to sum to 1",
                At("vertices", firstNormalized));
        }
        if (zero > 0) {
            _diagnostics.Add(codes::SkelZeroWeights,
                std::to_string(zero) + (zero == 1 ? " vertex has" : " vertices have")
                    + " no positive weight on any bone and "
                    + (zero == 1 ? "is" : "are") + " bound fully to its first bone",
                At("vertices", firstZero));
        }
    }

    /// Face ranges, culling and texture slots; the texture table normalized
    /// (PMX_CONTRACT.md §8, TEXT_ENCODING_POLICY.md §7).
    void _Materials()
    {
        _out.textures.reserve(_doc.textures.size());
        for (std::size_t i = 0; i < _doc.textures.size(); ++i) {
            const detail::TexturePath path = detail::NormalizeTexturePath(_doc.textures[i]);
            if (path.unsafe) {
                _diagnostics.Add(codes::PathUnsafeTexturePath,
                    "the texture path is absolute, names a drive, a server or a URI scheme, or "
                    "leaves the model's directory; it is kept as provenance and not followed",
                    At("textures", i));
            }
            _out.textures.push_back(Texture{_doc.textures[i], path.assetPath});
        }
        const auto texture = [this](std::int32_t index) {
            return index >= 0 && static_cast<std::size_t>(index) < _out.textures.size() ? index
                                                                                      : kNone;
        };

        const std::size_t faceIndices = _out.mesh.faceVertexIndices.size();
        std::size_t consumed = 0;
        _out.materials.reserve(_doc.materials.size());
        for (std::size_t i = 0; i < _doc.materials.size(); ++i) {
            const pmx::Material& source = _doc.materials[i];
            Material m;
            m.name = _materialNames[i];
            m.sourceIndex = i;
            // Materials consume the face table in order. The parser has
            // checked every count is a non-negative multiple of 3 and that the
            // counts fit the table; the clamp keeps this total regardless.
            const std::size_t wanted =
                source.faceCount > 0 ? static_cast<std::size_t>(source.faceCount) / 3 * 3 : 0;
            const std::size_t taken = std::min(wanted, faceIndices - consumed);
            m.firstFace = consumed / 3;
            m.faceCount = taken / 3;
            consumed += taken;
            m.diffuseColor = {source.diffuse[0], source.diffuse[1], source.diffuse[2],
                source.diffuse[3]};
            m.specularColor = ToFloat3(source.specular);
            m.specularPower = source.specularPower;
            m.ambientColor = ToFloat3(source.ambient);
            m.doubleSided = (source.flags & pmx::MaterialFlag::NoCulling) != 0;
            m.groundShadow = (source.flags & pmx::MaterialFlag::GroundShadow) != 0;
            m.castSelfShadow = (source.flags & pmx::MaterialFlag::CastsSelfShadow) != 0;
            m.receiveSelfShadow = (source.flags & pmx::MaterialFlag::ReceivesSelfShadow) != 0;
            m.drawEdge = (source.flags & pmx::MaterialFlag::DrawsEdge) != 0;
            m.vertexColor = (source.flags & pmx::MaterialFlag::VertexColor) != 0;
            m.drawPoints = (source.flags & pmx::MaterialFlag::PointDrawing) != 0;
            m.drawLines = (source.flags & pmx::MaterialFlag::LineDrawing) != 0;
            m.edgeColor = {source.edgeColor[0], source.edgeColor[1], source.edgeColor[2],
                source.edgeColor[3]};
            m.edgeSize = source.edgeSize;
            m.texture = texture(source.texture);
            m.sphereTexture = texture(source.sphereTexture);
            switch (source.sphereMode) {
            case 0:
                m.sphereMode = SphereMode::Disabled;
                break;
            case 1:
                m.sphereMode = SphereMode::Multiply;
                break;
            case 2:
                m.sphereMode = SphereMode::Add;
                break;
            case 3:
                m.sphereMode = SphereMode::SubTexture;
                break;
            default:
                _diagnostics.Add(codes::MaterialUnsupportedSphereMode,
                    "sphere mode " + std::to_string(source.sphereMode)
                        + " is unsupported and is treated as disabled",
                    At("materials", i, "sphereMode"));
                break;
            }
            if (source.toonReference == pmx::ToonReference::Texture) {
                m.toonTexture = texture(source.toonTexture);
                m.toonSource = m.toonTexture == kNone ? ToonSource::None : ToonSource::Individual;
            } else {
                if (source.sharedToon <= 9) {
                    m.toonSource = ToonSource::Shared;
                    m.sharedToonIndex = static_cast<std::int32_t>(source.sharedToon);
                } else {
                    _diagnostics.Add(codes::MaterialUnsupportedToonSlot,
                        "shared toon slot " + std::to_string(source.sharedToon)
                            + " is outside 0-9 and is treated as absent",
                        At("materials", i, "sharedToon"));
                }
            }
            m.memo = source.memo;
            // A material that draws nothing makes no face double-sided.
            if (m.doubleSided && m.faceCount > 0) {
                _out.mesh.doubleSided = true;
            }
            _out.materials.push_back(std::move(m));
        }
        _out.mesh.materialsCoverFaces = consumed == faceIndices;
    }

    const pmx::Document& _doc;
    CanonicalDocument _out;
    DiagnosticList _diagnostics;
    std::vector<Name> _materialNames;
    std::vector<Name> _boneNames;
};

}  // namespace

Result<CanonicalDocument>
Canonicalize(const pmx::Document& document)
{
    return Canonicalizer(document).Run();
}

}  // namespace mmd
