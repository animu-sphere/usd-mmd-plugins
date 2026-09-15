// SPDX-License-Identifier: Apache-2.0
//
// mmd::CanonicalDocument: a model as the stage contract means it -- every
// spatial value already in the USD basis and in meters, every element named
// by a stable identifier, the skeleton in canonical joint order, skinning
// normalized -- and nothing a PMX reader would have to explain
// (docs/design/DESIGN_POLICY.md §5.2, docs/design/PMX_CONTRACT.md §14).
//
// It holds what the current stage authors: metadata, textures, the mesh, the
// materials' identity, face ranges and texture slots, and the deformation
// skeleton. Morphs, control and physics semantics join it with the Phases that
// author them (DESIGN_POLICY.md §14).
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
    std::string source;    ///< decoded source name, e.g. "左腕"
    std::string english;   ///< decoded English name, e.g. "LeftArm"; often empty
    std::string stableId;  ///< USD-safe identifier (TEXT_ENCODING_POLICY.md §6)

    bool operator==(const Name&) const = default;
};

/// Model-level provenance (docs/design/STAGE_CONTRACT.md §5). Display names
/// have their trailing U+0000 padding dropped; comments are verbatim.
struct Metadata {
    std::string sourceVersion;  ///< "2.0" or "2.1"
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
    std::string sourcePath;  ///< exactly as decoded; kept even when unsafe
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

/// The one mesh of contract v1 (STAGE_CONTRACT.md §8.1). Every per-vertex
/// array is sized to the vertex count; every value is in the USD basis.
struct Mesh {
    std::vector<Float3> points;   ///< meters
    std::vector<Float3> normals;  ///< unit length, or zero where the source's was
    std::vector<Float2> st;       ///< PMX UV with v flipped (STAGE_CONTRACT.md §8.3)
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

/// A material's identity, face range and texture slots. Its full MMD semantics
/// and realizations are authored from Phase 3 (MATERIAL_POLICY.md).
struct Material {
    Name name;
    std::size_t sourceIndex = 0;  ///< also MMD's draw order
    std::size_t firstFace = 0;    ///< in triangles
    std::size_t faceCount = 0;    ///< in triangles; 0 authors no subset
    bool doubleSided = false;     ///< PMX drawing flag 0x01
    // Indices into CanonicalDocument::textures, or kNone.
    std::int32_t texture = kNone;
    std::int32_t sphereTexture = kNone;
    std::int32_t toonTexture = kNone;  ///< an individual toon ramp only

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

struct Skeleton {
    std::vector<Bone> bones;  ///< canonical joint order (STAGE_CONTRACT.md §9.1)
    /// Source bone index -> canonical index.
    std::vector<std::int32_t> jointOfSourceBone;

    bool operator==(const Skeleton&) const = default;
};

struct CanonicalDocument {
    Metadata metadata;
    std::vector<Texture> textures;  ///< source texture-table order
    Mesh mesh;                      ///< empty when the model has no vertices
    std::vector<Material> materials;
    Skeleton skeleton;              ///< empty when the model has no bones

    bool operator==(const CanonicalDocument&) const = default;
};

}  // namespace mmd
