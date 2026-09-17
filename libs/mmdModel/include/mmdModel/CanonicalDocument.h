// SPDX-License-Identifier: Apache-2.0
//
// mmd::CanonicalDocument: a model as the stage contract means it -- every
// spatial value already in the USD basis and in meters, every element named
// by a stable identifier, the skeleton in canonical joint order, skinning
// normalized -- and nothing a PMX reader would have to explain
// (docs/design/DESIGN_POLICY.md §5.2, docs/design/PMX_CONTRACT.md §14).
//
// It holds what the stage authors: metadata, textures, the mesh, the
// materials' identity, face ranges and texture slots, the deformation
// skeleton and its control semantics, every morph's declarative semantics,
// and the rigid bodies and joints.
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

/// An impulse morph's push on a rigid body, by its index into
/// CanonicalDocument::physics.rigidBodies -- which keeps the source's order,
/// so it is also the source index. The velocity is a displacement and the
/// torque an axial vector (STAGE_CONTRACT.md §6.3). Nothing here is ever
/// executed.
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

/// A bone's control semantics -- everything PMX says about a bone beyond its
/// position and parent (PMX_CONTRACT.md §9, STAGE_CONTRACT.md §12). Every
/// joint index is canonical and names a joint, or is kNone; a relation whose
/// bone the parser rejected is dropped rather than repaired. Nothing here is
/// evaluated: no append is applied and no axis constrains a rotation.
struct BoneControl {
    /// MMD's evaluation order, with `deformAfterPhysics`: preserved exactly,
    /// and not the canonical joint order.
    std::int32_t transformLayer = 0;
    bool deformAfterPhysics = false;
    bool rotatable = false;
    bool translatable = false;
    bool visible = false;
    bool operable = false;
    /// The tail is a joint, or an offset from the bone in meters (a
    /// displacement); the other is kNone or zero.
    std::int32_t tailJoint = kNone;
    Float3 tailOffset{};
    /// Append (inherit): the joint whose rotation, translation or both this
    /// bone takes on, scaled by the ratio. kNone, with every other append
    /// field cleared, when the bone appends nothing.
    std::int32_t appendSource = kNone;
    float appendRatio = 0.0f;
    bool appendRotation = false;
    bool appendTranslation = false;
    bool appendLocal = false; ///< PMX flag 0x0080: the source's local transform
    /// The one axis the bone rotates about, as an axial vector
    /// (STAGE_CONTRACT.md §6.3); zero when `hasFixedAxis` is false.
    bool hasFixedAxis = false;
    Float3 fixedAxis{};
    /// The X and Z columns of the bone's local frame, converted as a rotation
    /// (STAGE_CONTRACT.md §6.3); zero when `hasLocalAxes` is false.
    bool hasLocalAxes = false;
    Float3 localAxisX{};
    Float3 localAxisZ{};
    bool hasExternalParent = false;
    std::int32_t externalParentKey = 0;

    bool operator==(const BoneControl&) const = default;
};

/// One joint of an IK chain the solver may rotate, and its limits.
struct IkLink {
    std::int32_t joint = kNone;
    bool hasLimits = false;
    /// Radians, about each axis of the USD basis: about X and Y the source's
    /// [min, max] is [-max, -min], about Z it is unchanged
    /// (STAGE_CONTRACT.md §6.3). Zero when `hasLimits` is false.
    Float3 lowerLimit{};
    Float3 upperLimit{};

    bool operator==(const IkLink&) const = default;
};

/// A declarative IK chain (STAGE_CONTRACT.md §12): the effector is brought
/// to the IK bone by rotating the links. Nothing is solved.
struct IkChain {
    std::int32_t joint = kNone;    ///< the IK bone: the goal
    std::int32_t effector = kNone; ///< PMX's IK target: the joint that reaches it
    std::int32_t loopCount = 0;
    float limitAngle = 0.0f;   ///< radians per iteration, unchanged
    std::vector<IkLink> links; ///< source order; a link that names no bone is dropped

    bool operator==(const IkChain&) const = default;
};

/// The control rig (STAGE_CONTRACT.md §12).
struct Rig {
    /// One per joint, in canonical joint order, parallel to Skeleton::bones.
    std::vector<BoneControl> bones;
    /// One per IK bone whose effector names a bone, in bone-table order.
    std::vector<IkChain> ikChains;

    bool operator==(const Rig&) const = default;
};

/// A rigid body's collision shape (PMX_CONTRACT.md §13). The values are the
/// PMX ones; one the source does not define becomes `Sphere`, with
/// MMD_PHYSICS_UNKNOWN_SHAPE.
enum class RigidBodyShape : std::uint8_t {
    Sphere = 0,
    Box = 1,
    Capsule = 2,
};

/// How a rigid body relates to its bone. A value the source does not define
/// becomes `FollowBone`, with MMD_PHYSICS_UNKNOWN_MODE.
enum class PhysicsMode : std::uint8_t {
    FollowBone = 0,      ///< kinematic: the bone moves the body
    Dynamic = 1,         ///< simulated: the body moves the bone
    DynamicWithBone = 2, ///< simulated, the bone's position kept
};

/// A joint's constraint kind. PMX 2.0 defines only `Spring6Dof`; 2.1 adds the
/// rest. A value the source's version does not define becomes `Spring6Dof`,
/// with MMD_PHYSICS_UNKNOWN_JOINT_TYPE.
enum class PhysicsJointType : std::uint8_t {
    Spring6Dof = 0,
    SixDof = 1,
    PointToPoint = 2,
    ConeTwist = 3,
    Slider = 4,
    Hinge = 5,
};

/// A PMX rigid body (PMX_CONTRACT.md §13, STAGE_CONTRACT.md §13), in the USD
/// basis. Nothing here is simulated.
struct RigidBody {
    Name name;
    std::size_t sourceIndex = 0;
    /// The canonical joint of the bone the body is attached to, or kNone.
    std::int32_t bone = kNone;
    std::int32_t collisionGroup = 0; ///< as stored; 0-15 in a well-formed model
    /// As stored: bit g set means the body collides with group g.
    std::int32_t collisionMask = 0;
    RigidBodyShape shape = RigidBodyShape::Sphere;
    /// Meters, never mirrored -- extents are lengths: a sphere's radius in
    /// x, a box's half extents, a capsule's radius in x and the length of its
    /// cylinder, along its Y axis, in y. The components a shape does not use
    /// are kept as they are.
    Double3 size{};
    Double3 position{}; ///< meters, a point
    /// The body's frame (x, y, z, w), composed from the source's Euler angles
    /// (PMX-O1) and converted as a rotation; w is never negative.
    Float4 orientation{0.0f, 0.0f, 0.0f, 1.0f};
    float mass = 0.0f;
    float linearDamping = 0.0f;
    float angularDamping = 0.0f;
    float restitution = 0.0f;
    float friction = 0.0f;
    PhysicsMode mode = PhysicsMode::FollowBone;

    bool operator==(const RigidBody&) const = default;
};

/// A PMX joint between two rigid bodies, in the USD basis. Its limits are
/// about and along the axes of its own frame, and a lower limit above its
/// upper one leaves that axis free, as the source's does. Nothing here is
/// simulated.
struct PhysicsJoint {
    Name name;
    std::size_t sourceIndex = 0;
    PhysicsJointType type = PhysicsJointType::Spring6Dof;
    /// Indices into Physics::rigidBodies; both name a body.
    std::int32_t rigidBodyA = kNone;
    std::int32_t rigidBodyB = kNone;
    Double3 position{};                         ///< meters, a point
    Float4 orientation{0.0f, 0.0f, 0.0f, 1.0f}; ///< as a rigid body's
    /// Meters: along X and Y scaled, along Z [min, max] -> s * [-max, -min]
    /// (STAGE_CONTRACT.md §6.3).
    Float3 translationLowerLimit{};
    Float3 translationUpperLimit{};
    /// Radians: about X and Y [min, max] -> [-max, -min], about Z unchanged.
    Float3 rotationLowerLimit{};
    Float3 rotationUpperLimit{};
    /// Spring constants per axis, in source units: their unit depends on the
    /// length scale, and converting them is a physics runtime's decision.
    Float3 translationSpring{};
    Float3 rotationSpring{};
    /// The joint's frame in each body's frame, from their rest transforms:
    /// what a two-body constraint is built from.
    Float3 localPositionA{};
    Float4 localOrientationA{0.0f, 0.0f, 0.0f, 1.0f};
    Float3 localPositionB{};
    Float4 localOrientationB{0.0f, 0.0f, 0.0f, 1.0f};

    bool operator==(const PhysicsJoint&) const = default;
};

/// Rigid bodies and joints (STAGE_CONTRACT.md §13).
struct Physics {
    /// Every rigid body, in source order: none is dropped, and a body whose
    /// bone the parser rejected is unattached.
    std::vector<RigidBody> rigidBodies;
    /// Every joint whose two bodies both name one, in source order.
    std::vector<PhysicsJoint> joints;

    bool operator==(const Physics&) const = default;
};

struct CanonicalDocument {
    Metadata metadata;
    std::vector<Texture> textures; ///< source texture-table order
    Mesh mesh;                     ///< empty when the model has no vertices
    std::vector<Material> materials;
    Skeleton skeleton;         ///< empty when the model has no bones
    Rig rig;                   ///< empty when the model has no bones
    std::vector<Morph> morphs; ///< source morph-table order
    Physics physics;           ///< empty when the model has no rigid bodies

    bool operator==(const CanonicalDocument&) const = default;
};

} // namespace mmd
