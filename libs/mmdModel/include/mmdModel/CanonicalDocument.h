// SPDX-License-Identifier: Apache-2.0
//
// mmd::CanonicalDocument: a model as the stage contract means it -- every
// spatial value already in the USD basis and in meters, every element named
// by a stable identifier, the skeleton in canonical joint order, skinning
// normalized -- and nothing a PMX reader would have to explain
// (docs/design/DESIGN_POLICY.md §5.2, docs/design/PMX_CONTRACT.md §14).
//
// It holds what the current stage authors: metadata, textures, the mesh, the
// materials' identity, face ranges and texture slots, the deformation
// skeleton, and every morph's declarative semantics. Control and physics
// semantics join it with the Phases that author them (DESIGN_POLICY.md §14).
//
// No OpenUSD type appears here: a tool with no USD in the process can consume
// canonical MMD (docs/architecture/WORKSPACE.md §2).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mmd {

using Float2 = std::array<float, 2>;
using Float3 = std::array<float, 3>;
using Float4 = std::array<float, 4>;
using Double3 = std::array<double, 3>;

/// What an optional index into a canonical table holds when it names nothing.
inline constexpr std::int32_t kNone = -1;

/// The three names every canonical element carries
/// (docs/design/TEXT_ENCODING_POLICY.md §5).
struct Name {
    std::string source;   ///< decoded source name, e.g. "左腕"
    std::string english;  ///< decoded English name, e.g. "LeftArm"; often empty
    std::string stableId; ///< USD-safe identifier (TEXT_ENCODING_POLICY.md §6)

    bool operator==(const Name&) const = default;
};

/// Model-level provenance (docs/design/STAGE_CONTRACT.md §5). Display names
/// have their trailing U+0000 padding dropped; comments are verbatim.
struct Metadata {
    std::string sourceVersion; ///< "2.0" or "2.1"
    std::string name;
    std::string englishName;
    std::string comment;
    std::string englishComment;
    /// PMX 2.1 soft bodies the source holds. No stage contract carries them;
    /// the importer reports them (PMX_CONTRACT.md §12).
    std::size_t softBodyCount = 0;

    bool operator==(const Metadata&) const = default;
};

/// One entry of the texture table (TEXT_ENCODING_POLICY.md §7).
struct Texture {
    std::string sourcePath; ///< exactly as decoded; kept even when unsafe
    /// The normalized logical path anchored to the model's layer, e.g.
    /// "./tex/髪.png". Empty when the path is unsafe or names nothing: no
    /// asset path is authored for it.
    std::string assetPath;

    bool operator==(const Texture&) const = default;
};

/// A vertex's PMX deform type. The values are the ones the stage authors in
/// `primvars:mmd:deformType` (STAGE_CONTRACT.md §8.4).
enum class DeformType : std::uint8_t {
    Bdef1 = 0,
    Bdef2 = 1,
    Bdef4 = 2,
    Sdef = 3,
    Qdef = 4,
};

/// MMD sphere-map mode (MATERIAL_POLICY.md §4.1).
enum class SphereMode : std::uint8_t {
    Disabled = 0,
    Multiply = 1,
    Add = 2,
    SubTexture = 3,
};

/// The source form of a material's toon ramp (MATERIAL_POLICY.md §7).
enum class ToonSource : std::uint8_t {
    None = 0,
    Individual = 1,
    Shared = 2,
};

/// The one mesh of contract v1 (STAGE_CONTRACT.md §8.1). Every per-vertex
/// array is sized to the vertex count; every value is in the USD basis.
struct Mesh {
    std::vector<Float3> points;  ///< meters
    std::vector<Float3> normals; ///< unit length, or zero where the source's was
    std::vector<Float2> st;      ///< PMX UV with v flipped (STAGE_CONTRACT.md §8.3)
    /// Additional vec4 channels, raw. Only the first `additionalUvCount` are
    /// filled; the others are empty.
    std::uint8_t additionalUvCount = 0;
    std::array<std::vector<Float4>, 4> additionalUv;
    std::vector<float> edgeScale;
    /// Three vertex indices per triangle, winding already reversed
    /// (STAGE_CONTRACT.md §6.3).
    std::vector<std::int32_t> faceVertexIndices;
    /// True when the material face ranges cover every face exactly once; false
    /// when a repaired source leaves a tail unbound (PMX_CONTRACT.md §8).
    bool materialsCoverFaces = true;
    /// True when any material disables culling (STAGE_CONTRACT.md §8.5).
    bool doubleSided = false;

    // Skinning (STAGE_CONTRACT.md §9.4). Empty when the model has no bones.
    std::vector<DeformType> deformTypes;
    /// N: 1, 2 or 4, from the largest deform type present; 0 when unskinned.
    std::size_t influencesPerVertex = 0;
    /// N per vertex, in canonical joint order, padded with weight 0 on joint 0.
    std::vector<std::int32_t> jointIndices;
    /// N per vertex, each vertex's summing to 1 (STAGE_CONTRACT.md §9.5).
    std::vector<float> jointWeights;
    /// SDEF parameters, converted as points; zero for other vertices. Empty
    /// unless a vertex is SDEF.
    std::vector<Float3> sdefC;
    std::vector<Float3> sdefR0;
    std::vector<Float3> sdefR1;
    std::size_t sdefVertexCount = 0;
    std::size_t qdefVertexCount = 0;

    std::size_t FaceCount() const { return faceVertexIndices.size() / 3; }

    bool operator==(const Mesh&) const = default;
};

/// A material's canonical MMD semantics, identity, face range and texture
/// slots (MATERIAL_POLICY.md §§4 and 7).
struct Material {
    Name name;
    std::size_t sourceIndex = 0; ///< also MMD's draw order
    std::size_t firstFace = 0;   ///< in triangles
    std::size_t faceCount = 0;   ///< in triangles; 0 authors no subset
    Float4 diffuseColor{};
    Float3 specularColor{};
    float specularPower = 0.0f;
    Float3 ambientColor{};
    bool doubleSided = false; ///< PMX drawing flag 0x01
    bool groundShadow = false;
    bool castSelfShadow = false;
    bool receiveSelfShadow = false;
    bool drawEdge = false;
    bool vertexColor = false;
    bool drawPoints = false;
    bool drawLines = false;
    Float4 edgeColor{};
    float edgeSize = 0.0f;
    // Indices into CanonicalDocument::textures, or kNone.
    std::int32_t texture = kNone;
    std::int32_t sphereTexture = kNone;
    SphereMode sphereMode = SphereMode::Disabled;
    ToonSource toonSource = ToonSource::None;
    std::int32_t toonTexture = kNone; ///< an individual toon ramp only
    std::int32_t sharedToonIndex = kNone;
    std::string memo;

    bool operator==(const Material&) const = default;
};

/// A joint of the deformation skeleton (STAGE_CONTRACT.md §9).
struct Bone {
    Name name;
    std::size_t sourceIndex = 0;
    /// Canonical index of the parent, always lower than this bone's, or kNone.
    std::int32_t parent = kNone;
    /// The joint's path of stable identifiers, e.g. "center/LeftArm".
    std::string jointPath;
    /// World-space position, the bind translation, and the translation from
    /// the parent's position, the rest translation (STAGE_CONTRACT.md §9.2).
    /// Meters, USD basis, in double -- a joint transform is a matrix4d -- and
    /// each converted from the source floats directly, so neither carries a
    /// float rounding the source did not have.
    Double3 position{};
    Double3 localTranslation{};

    bool operator==(const Bone&) const = default;
};

/// A morph's kind (PMX_CONTRACT.md §10). The values are the PMX ones, so a
/// canonical morph keeps the source's vocabulary.
enum class MorphType : std::uint8_t {
    Group = 0,
    Vertex = 1,
    Bone = 2,
    Uv = 3,
    AdditionalUv1 = 4,
    AdditionalUv2 = 5,
    AdditionalUv3 = 6,
    AdditionalUv4 = 7,
    Material = 8,
    Flip = 9,
    Impulse = 10,
};

/// The editor panel a morph belongs to (PMX_CONTRACT.md §10). A value the
/// source does not define becomes `Other`, with MMD_MORPH_UNKNOWN_PANEL.
enum class MorphPanel : std::uint8_t {
    Hidden = 0,
    Eyebrow = 1,
    Eye = 2,
    Mouth = 3,
    Other = 4,
};

/// How a material morph combines with the material it names.
enum class MaterialMorphOperation : std::uint8_t {
    Multiply = 0,
    Add = 1,
};

/// A group or flip morph's member: a canonical morph index and its weight.
/// A member that names nothing, or that would close a cycle, is dropped
/// during canonicalization (PMX_CONTRACT.md §10).
struct MorphMember {
    std::int32_t morph = kNone;
    float weight = 0.0f;

    bool operator==(const MorphMember&) const = default;
};

/// A vertex morph's displacement: a vertex index and its offset in meters,
/// in the USD basis (STAGE_CONTRACT.md §6.3).
struct MorphVertexOffset {
    std::int32_t vertex = 0;
    Float3 offset{};

    bool operator==(const MorphVertexOffset&) const = default;
};

/// A bone morph's delta, against a joint in canonical order: a translation in
/// meters and a rotation quaternion (x, y, z, w), both converted.
struct MorphBoneOffset {
    std::int32_t joint = 0;
    Float3 translation{};
    Float4 rotation{0.0f, 0.0f, 0.0f, 1.0f};

    bool operator==(const MorphBoneOffset&) const = default;
};

/// A UV or additional-UV morph's offset: a raw delta in the channel it
/// modifies, never flipped (PMX_CONTRACT.md §10).
struct MorphUvOffset {
    std::int32_t vertex = 0;
    Float4 delta{};

    bool operator==(const MorphUvOffset&) const = default;
};

/// A material morph's modulation of one material, or of every material when
/// `material` is kNone -- which is what PMX's -1 means here, and also what an
/// out-of-range index the parser rejected (with MMD_PMX_INDEX_OUT_OF_RANGE)
/// arrives as. Every value is the source's, unconverted: they are colors,
/// tints and dimensionless factors (STAGE_CONTRACT.md §6.3).
struct MorphMaterialOffset {
    std::int32_t material = kNone;
    MaterialMorphOperation operation = MaterialMorphOperation::Multiply;
    Float4 diffuseColor{};
    Float3 specularColor{};
    float specularPower = 0.0f;
    Float3 ambientColor{};
    Float4 edgeColor{};
    float edgeSize = 0.0f;
    Float4 textureTint{};
    Float4 sphereTint{};
    Float4 toonTint{};

    bool operator==(const MorphMaterialOffset&) const = default;
};

/// An impulse morph's push on a rigid body. Rigid bodies are not authored
/// before Phase 6, so the body is kept by its source index; the velocity is a
/// displacement and the torque an axial vector (STAGE_CONTRACT.md §6.3).
/// Nothing here is ever executed.
struct MorphImpulseOffset {
    std::int32_t rigidBody = kNone;
    bool local = false;
    Float3 velocity{};
    Float3 torque{};

    bool operator==(const MorphImpulseOffset&) const = default;
};

/// One PMX morph, carried declaratively (STAGE_CONTRACT.md §11). Exactly one
/// offset list is used, the one `type` selects; nothing is evaluated, and no
/// group morph is expanded.
struct Morph {
    Name name;
    std::size_t sourceIndex = 0;
    MorphType type = MorphType::Group;
    MorphPanel panel = MorphPanel::Other;
    std::vector<MorphMember> members;                 ///< group and flip
    std::vector<MorphVertexOffset> vertexOffsets;     ///< vertex
    std::vector<MorphBoneOffset> boneOffsets;         ///< bone
    std::vector<MorphUvOffset> uvOffsets;             ///< uv and additionalUv1-4
    std::vector<MorphMaterialOffset> materialOffsets; ///< material
    std::vector<MorphImpulseOffset> impulseOffsets;   ///< impulse

    bool operator==(const Morph&) const = default;
};

struct Skeleton {
    std::vector<Bone> bones; ///< canonical joint order (STAGE_CONTRACT.md §9.1)
    /// Source bone index -> canonical index.
    std::vector<std::int32_t> jointOfSourceBone;

    bool operator==(const Skeleton&) const = default;
};

struct CanonicalDocument {
    Metadata metadata;
    std::vector<Texture> textures; ///< source texture-table order
    Mesh mesh;                     ///< empty when the model has no vertices
    std::vector<Material> materials;
    Skeleton skeleton;         ///< empty when the model has no bones
    std::vector<Morph> morphs; ///< source morph-table order

    bool operator==(const CanonicalDocument&) const = default;
};

} // namespace mmd
