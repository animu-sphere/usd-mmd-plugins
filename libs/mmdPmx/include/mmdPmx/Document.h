// SPDX-License-Identifier: Apache-2.0
//
// pmx::Document: a PMX file as the file states it -- source facts in the
// source's own basis and units, never USD policy
// (docs/design/DESIGN_POLICY.md §5.1, docs/design/PMX_CONTRACT.md §1).
//
// Every table is here, in file order, one record per record in the file.
// Text is decoded to validated UTF-8; a string that failed to decode is empty
// and a diagnostic says so. Every index is validated: it names an element of
// its table, or is kNoIndex. Nothing is converted, normalized, or dropped --
// what an index of kNoIndex means for each relation, and every other semantic
// decision, is canonicalization's (PMX_CONTRACT.md §14).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mmd::pmx {

using Vec2 = std::array<float, 2>;
using Vec3 = std::array<float, 3>;
using Vec4 = std::array<float, 4>;

/// "None": what an optional texture, material, bone, morph or rigid-body index
/// holds when it names nothing, and what an out-of-range index is replaced by
/// (PMX_CONTRACT.md §4).
inline constexpr std::int32_t kNoIndex = -1;

// --- Header ------------------------------------------------------------------

/// The two versions PMX defines. Compared exactly on read (PMX_CONTRACT.md §3).
enum class Version : std::uint8_t {
    V2_0,
    V2_1,
};

/// "2.0" or "2.1".
std::string_view ToString(Version version);

/// globals[0]: the encoding of every string in the file.
enum class TextEncoding : std::uint8_t {
    Utf16Le = 0,
    Utf8 = 1,
};

/// The header's globals (PMX_CONTRACT.md §3), validated.
struct Globals {
    TextEncoding textEncoding = TextEncoding::Utf16Le;
    std::uint8_t additionalVec4Count = 0; ///< 0-4
    // Index widths in bytes: 1, 2 or 4 each (PMX_CONTRACT.md §4).
    std::uint8_t vertexIndexSize = 1;
    std::uint8_t textureIndexSize = 1;
    std::uint8_t materialIndexSize = 1;
    std::uint8_t boneIndexSize = 1;
    std::uint8_t morphIndexSize = 1;
    std::uint8_t rigidBodyIndexSize = 1;
    /// Bytes beyond the eight PMX defines, preserved as read.
    std::vector<std::uint8_t> unknown;

    bool operator==(const Globals&) const = default;
};

struct Header {
    Version version = Version::V2_0;
    Globals globals;

    bool operator==(const Header&) const = default;
};

/// The four strings after the header, preserved verbatim as provenance.
struct ModelInfo {
    std::string name;
    std::string englishName;
    std::string comment;
    std::string englishComment;

    bool operator==(const ModelInfo&) const = default;
};

// --- Vertices (PMX_CONTRACT.md §5) --------------------------------------------

enum class DeformType : std::uint8_t {
    Bdef1 = 0,
    Bdef2 = 1,
    Bdef4 = 2,
    Sdef = 3,
    Qdef = 4, ///< PMX 2.1 only
};

/// "BDEF1", "BDEF2", "BDEF4", "SDEF" or "QDEF".
std::string_view ToString(DeformType type);

/// A vertex's deform, as stored. The file stores a different number of
/// values per type, and so does this record:
///
/// | type        | bones        | weights                              |
/// | BDEF1       | bones[0]     | none stored (bones[0] has weight 1)  |
/// | BDEF2, SDEF | bones[0..1]  | weights[0], the weight of bones[0]   |
/// | BDEF4, QDEF | bones[0..3]  | weights[0..3]                        |
///
/// Unused slots hold kNoIndex and 0. Expanding to explicit influences and
/// normalizing is canonicalization's (PMX_CONTRACT.md §14 step 4).
struct Deform {
    DeformType type = DeformType::Bdef1;
    std::array<std::int32_t, 4> bones{kNoIndex, kNoIndex, kNoIndex, kNoIndex};
    std::array<float, 4> weights{};
    // SDEF only.
    Vec3 sdefC{};
    Vec3 sdefR0{};
    Vec3 sdefR1{};

    bool operator==(const Deform&) const = default;
};

struct Vertex {
    Vec3 position{};
    Vec3 normal{};
    Vec2 uv{};
    /// The first `Globals::additionalVec4Count` are read; the rest are zero.
    std::array<Vec4, 4> additionalVec4{};
    Deform deform;
    float edgeScale = 0.0f;

    bool operator==(const Vertex&) const = default;
};

// --- Materials (PMX_CONTRACT.md §8) --------------------------------------------

/// Drawing-flag bits. 0x20-0x80 are PMX 2.1's, and carry no meaning in a 2.0
/// file, where they are preserved anyway.
struct MaterialFlag {
    static constexpr std::uint8_t NoCulling = 0x01;
    static constexpr std::uint8_t GroundShadow = 0x02;
    static constexpr std::uint8_t CastsSelfShadow = 0x04;
    static constexpr std::uint8_t ReceivesSelfShadow = 0x08;
    static constexpr std::uint8_t DrawsEdge = 0x10;
    static constexpr std::uint8_t VertexColor = 0x20;
    static constexpr std::uint8_t PointDrawing = 0x40;
    static constexpr std::uint8_t LineDrawing = 0x80;
};

/// What follows a material's toon-reference byte.
enum class ToonReference : std::uint8_t {
    Texture = 0, ///< a texture index: `Material::toonTexture`
    Shared = 1,  ///< a shared toon slot: `Material::sharedToon`
};

struct Material {
    std::string name;
    std::string englishName;
    Vec4 diffuse{};
    Vec3 specular{};
    float specularPower = 0.0f;
    Vec3 ambient{};
    std::uint8_t flags = 0; ///< MaterialFlag bits, as stored
    Vec4 edgeColor{};
    float edgeSize = 0.0f;
    std::int32_t texture = kNoIndex;
    std::int32_t sphereTexture = kNoIndex;
    /// As stored. 0 disabled, 1 multiply, 2 add, 3 sub-texture; any other
    /// value is canonicalization's to judge.
    std::uint8_t sphereMode = 0;
    ToonReference toonReference = ToonReference::Texture;
    std::int32_t toonTexture = kNoIndex; ///< when toonReference is Texture
    std::uint8_t sharedToon = 0;         ///< when Shared: as stored; 0-9 are defined
    std::string memo;
    /// The number of face indices this material draws, a multiple of 3. The
    /// materials consume the face table in order.
    std::int32_t faceCount = 0;

    bool operator==(const Material&) const = default;
};

// --- Bones (PMX_CONTRACT.md §9) -------------------------------------------------

/// Bone-flag bits. Bits not listed are preserved and carry no meaning here.
struct BoneFlag {
    static constexpr std::uint16_t TailIsBone = 0x0001;
    static constexpr std::uint16_t Rotatable = 0x0002;
    static constexpr std::uint16_t Translatable = 0x0004;
    static constexpr std::uint16_t Visible = 0x0008;
    static constexpr std::uint16_t Operable = 0x0010;
    static constexpr std::uint16_t Ik = 0x0020;
    static constexpr std::uint16_t LocalAppend = 0x0080;
    static constexpr std::uint16_t AppendRotation = 0x0100;
    static constexpr std::uint16_t AppendTranslation = 0x0200;
    static constexpr std::uint16_t FixedAxis = 0x0400;
    static constexpr std::uint16_t LocalAxes = 0x0800;
    static constexpr std::uint16_t DeformAfterPhysics = 0x1000;
    static constexpr std::uint16_t ExternalParent = 0x2000;
};

struct IkLink {
    std::int32_t bone = kNoIndex;
    bool hasLimits = false;
    Vec3 lowerLimit{}; ///< radians, when hasLimits
    Vec3 upperLimit{}; ///< radians, when hasLimits

    bool operator==(const IkLink&) const = default;
};

struct Ik {
    std::int32_t target = kNoIndex;
    std::int32_t loopCount = 0;
    float limitAngle = 0.0f; ///< radians
    std::vector<IkLink> links;

    bool operator==(const Ik&) const = default;
};

/// A bone. Each group of fields after `flags` is present in the file only
/// when its flag is set, and holds its default otherwise.
struct Bone {
    std::string name;
    std::string englishName;
    Vec3 position{};
    std::int32_t parent = kNoIndex;
    std::int32_t transformLayer = 0;
    std::uint16_t flags = 0;              ///< BoneFlag bits, as stored
    std::int32_t tailBone = kNoIndex;     ///< TailIsBone
    Vec3 tailOffset{};                    ///< not TailIsBone
    std::int32_t appendParent = kNoIndex; ///< AppendRotation or AppendTranslation
    float appendRatio = 0.0f;             ///< AppendRotation or AppendTranslation
    Vec3 fixedAxis{};                     ///< FixedAxis
    Vec3 localAxisX{};                    ///< LocalAxes
    Vec3 localAxisZ{};                    ///< LocalAxes
    std::int32_t externalParentKey = 0;   ///< ExternalParent
    Ik ik;                                ///< Ik

    bool operator==(const Bone&) const = default;
};

// --- Morphs (PMX_CONTRACT.md §10) ------------------------------------------------

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
    Flip = 9,     ///< PMX 2.1 only
    Impulse = 10, ///< PMX 2.1 only
};

/// "group", "vertex", "bone", "uv", "additionalUv1"-"additionalUv4",
/// "material", "flip" or "impulse".
std::string_view ToString(MorphType type);

/// A group or flip morph's member.
struct GroupOffset {
    std::int32_t morph = kNoIndex;
    float weight = 0.0f;

    bool operator==(const GroupOffset&) const = default;
};

struct VertexOffset {
    std::int32_t vertex = kNoIndex;
    Vec3 displacement{};

    bool operator==(const VertexOffset&) const = default;
};

struct BoneOffset {
    std::int32_t bone = kNoIndex;
    Vec3 translation{};
    Vec4 rotation{}; ///< quaternion (x, y, z, w)

    bool operator==(const BoneOffset&) const = default;
};

/// A UV or additional-UV morph's offset: a raw delta in the channel it
/// modifies, never flipped (PMX_CONTRACT.md §10).
struct UvOffset {
    std::int32_t vertex = kNoIndex;
    Vec4 delta{};

    bool operator==(const UvOffset&) const = default;
};

struct MaterialOffset {
    std::int32_t material = kNoIndex; ///< kNoIndex means every material
    std::uint8_t operation = 0;       ///< as stored: 0 multiply, 1 add
    Vec4 diffuse{};
    Vec3 specular{};
    float specularPower = 0.0f;
    Vec3 ambient{};
    Vec4 edgeColor{};
    float edgeSize = 0.0f;
    Vec4 textureTint{};
    Vec4 sphereTint{};
    Vec4 toonTint{};

    bool operator==(const MaterialOffset&) const = default;
};

struct ImpulseOffset {
    std::int32_t rigidBody = kNoIndex;
    std::uint8_t local = 0; ///< as stored: 0 world, 1 local
    Vec3 velocity{};
    Vec3 torque{};

    bool operator==(const ImpulseOffset&) const = default;
};

/// A morph. Exactly one offset list is used, the one its type selects: group
/// and flip use `groupOffsets`, UV and additional-UV use `uvOffsets`.
struct Morph {
    std::string name;
    std::string englishName;
    std::uint8_t panel = 0; ///< as stored: 0 hidden, 1 eyebrow, 2 eye, 3 mouth, 4 other
    MorphType type = MorphType::Group;
    std::vector<GroupOffset> groupOffsets;
    std::vector<VertexOffset> vertexOffsets;
    std::vector<BoneOffset> boneOffsets;
    std::vector<UvOffset> uvOffsets;
    std::vector<MaterialOffset> materialOffsets;
    std::vector<ImpulseOffset> impulseOffsets;

    /// The number of offsets in the list the type selects.
    std::size_t OffsetCount() const;

    bool operator==(const Morph&) const = default;
};

// --- Display frames (PMX_CONTRACT.md §11) ------------------------------------------

enum class FrameElementKind : std::uint8_t {
    Bone = 0,
    Morph = 1,
};

struct FrameElement {
    FrameElementKind kind = FrameElementKind::Bone;
    std::int32_t index = kNoIndex; ///< a bone or a morph, by `kind`

    bool operator==(const FrameElement&) const = default;
};

struct DisplayFrame {
    std::string name;
    std::string englishName;
    std::uint8_t special = 0; ///< as stored: 1 marks the Root and expression frames
    std::vector<FrameElement> elements;

    bool operator==(const DisplayFrame&) const = default;
};

// --- Physics (PMX_CONTRACT.md §12, §13) ----------------------------------------------

/// Rotations are Euler triples in radians, as stored; their composition order
/// is PMX-O1 and nothing here composes them.
struct RigidBody {
    std::string name;
    std::string englishName;
    std::int32_t bone = kNoIndex;
    std::uint8_t group = 0;
    std::uint16_t nonCollisionMask = 0;
    std::uint8_t shape = 0; ///< as stored: 0 sphere, 1 box, 2 capsule
    Vec3 size{};
    Vec3 position{};
    Vec3 rotation{};
    float mass = 0.0f;
    float linearDamping = 0.0f;
    float angularDamping = 0.0f;
    float restitution = 0.0f;
    float friction = 0.0f;
    /// As stored: 0 follows the bone, 1 simulated, 2 simulated with bone
    /// position alignment.
    std::uint8_t physicsMode = 0;

    bool operator==(const RigidBody&) const = default;
};

struct Joint {
    std::string name;
    std::string englishName;
    /// As stored: 0 spring 6-DOF; PMX 2.1 adds 1 6-DOF, 2 point-to-point,
    /// 3 cone-twist, 4 slider, 5 hinge.
    std::uint8_t type = 0;
    std::int32_t rigidBodyA = kNoIndex;
    std::int32_t rigidBodyB = kNoIndex;
    Vec3 position{};
    Vec3 rotation{};
    Vec3 translationMin{};
    Vec3 translationMax{};
    Vec3 rotationMin{};
    Vec3 rotationMax{};
    Vec3 translationSpring{};
    Vec3 rotationSpring{};

    bool operator==(const Joint&) const = default;
};

struct SoftBodyAnchor {
    std::int32_t rigidBody = kNoIndex;
    std::int32_t vertex = kNoIndex;
    std::uint8_t nearMode = 0;

    bool operator==(const SoftBodyAnchor&) const = default;
};

/// A PMX 2.1 soft body: parsed completely so that a 2.1 file is traversed to
/// its end, and kept in the syntax layer only (PMX_CONTRACT.md §12). The
/// arrays keep the specification's order and names.
struct SoftBody {
    std::string name;
    std::string englishName;
    std::uint8_t shape = 0; ///< as stored: 0 triangle mesh, 1 rope
    std::int32_t material = kNoIndex;
    std::uint8_t group = 0;
    std::uint16_t nonCollisionMask = 0;
    std::uint8_t flags = 0; ///< 0x01 B-link, 0x02 cluster creation, 0x04 link crossing
    std::int32_t bLinkDistance = 0;
    std::int32_t clusterCount = 0;
    float totalMass = 0.0f;
    float collisionMargin = 0.0f;
    std::int32_t aeroModel = 0;
    std::array<float, 12> config{}; ///< VCF DP DG LF PR VC DF MT CHR KHR SHR AHR
    std::array<float, 6> cluster{}; ///< SRHR_CL SKHR_CL SSHR_CL SR_SPLT_CL SK_SPLT_CL SS_SPLT_CL
    std::array<std::int32_t, 4> iteration{};     ///< V_IT P_IT D_IT C_IT
    std::array<float, 3> materialCoefficients{}; ///< LST AST VST
    std::vector<SoftBodyAnchor> anchors;
    std::vector<std::int32_t> pinVertices;

    bool operator==(const SoftBody&) const = default;
};

// --- The document ---------------------------------------------------------------------

struct Document {
    Header header;
    ModelInfo model;
    std::vector<Vertex> vertices;
    /// Vertex indices, three per triangle; every one names a vertex.
    std::vector<std::uint32_t> faces;
    /// Texture paths exactly as decoded (PMX_CONTRACT.md §7).
    std::vector<std::string> textures;
    std::vector<Material> materials;
    std::vector<Bone> bones;
    std::vector<Morph> morphs;
    std::vector<DisplayFrame> displayFrames;
    std::vector<RigidBody> rigidBodies;
    std::vector<Joint> joints;
    std::vector<SoftBody> softBodies; ///< PMX 2.1 only; empty in a 2.0 file

    bool operator==(const Document&) const = default;
};

} // namespace mmd::pmx
