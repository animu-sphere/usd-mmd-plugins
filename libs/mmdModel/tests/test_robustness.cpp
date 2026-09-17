// SPDX-License-Identifier: Apache-2.0
//
// Canonicalization is total over every document the parser can return. The
// parser's own robustness suite and fuzzer establish which documents those
// are -- every index names an element or is none, faces name vertices,
// material face counts fit -- and within that, anything goes: parents that
// cycle or name themselves, weights that are negative, NaN or huge, names
// that are all punctuation or padded with U+0000, texture paths built from
// the pieces that make a path unsafe. Each generated document is
// canonicalized twice and checked against the promises CanonicalDocument.h
// makes -- morphs included, whose members are generated to cycle freely
// through each other, and the rig, whose bones carry every flag and name any
// bone or none. The generator is a fixed-seed PRNG with its own arithmetic,
// so the run is the same on every platform.
#include "mmdModel/Canonicalize.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <set>
#include <utility>
#include <string>
#include <vector>

namespace {

namespace pmx = mmd::pmx;

/// SplitMix64: small, and specified to the bit, unlike the standard
/// library's distributions.
class Random {
public:
    explicit Random(std::uint64_t seed) : _state(seed) {}

    std::uint64_t Next()
    {
        std::uint64_t z = (_state += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    /// [0, n)
    std::size_t Below(std::size_t n) { return n == 0 ? 0 : static_cast<std::size_t>(Next() % n); }
    bool OneIn(std::size_t n) { return Below(n) == 0; }

    template <class T> const T& Pick(const std::vector<T>& values)
    {
        return values[Below(values.size())];
    }

private:
    std::uint64_t _state;
};

float
WildFloat(Random& r)
{
    static const std::vector<float> special{0.0f,
                                            -0.0f,
                                            1.0f,
                                            -1.0f,
                                            0.5f,
                                            1e-30f,
                                            1e30f,
                                            -1e30f,
                                            std::numeric_limits<float>::max(),
                                            std::numeric_limits<float>::infinity(),
                                            -std::numeric_limits<float>::infinity(),
                                            std::numeric_limits<float>::quiet_NaN()};
    if (r.OneIn(4)) {
        return r.Pick(special);
    }
    return static_cast<float>(static_cast<double>(r.Below(2001)) / 100.0 - 10.0);
}

pmx::Vec3
WildVec3(Random& r)
{
    return {WildFloat(r), WildFloat(r), WildFloat(r)};
}

std::string
WildName(Random& r)
{
    static const std::vector<std::string> pieces{"",
                                                 "a",
                                                 "Z",
                                                 "9",
                                                 "_",
                                                 " ",
                                                 "-",
                                                 "ab",
                                                 "左腕",
                                                 "ＩＫ",
                                                 "・",
                                                 std::string("\0", 1),
                                                 "Arm",
                                                 "arm",
                                                 "ARM",
                                                 "bone_0001",
                                                 "material_0000",
                                                 "x_1",
                                                 std::string(70, 'q')};
    std::string name;
    const std::size_t n = r.Below(4);
    for (std::size_t i = 0; i < n; ++i) {
        name += r.Pick(pieces);
    }
    return name;
}

std::string
WildPath(Random& r)
{
    static const std::vector<std::string> pieces{"tex",
                                                 "髪.png",
                                                 "..",
                                                 ".",
                                                 "/",
                                                 "\\",
                                                 "C:",
                                                 "http:",
                                                 "a",
                                                 "b.bmp",
                                                 "//",
                                                 std::string("\0", 1),
                                                 " ",
                                                 "~",
                                                 "\t",
                                                 "\x7F",
                                                 "\xC2\x85",
                                                 "\xC2\xA0"};
    std::string path;
    const std::size_t n = r.Below(6);
    for (std::size_t i = 0; i < n; ++i) {
        path += r.Pick(pieces);
    }
    return path;
}

/// An index into a table of `size`, or none.
std::int32_t
Index(Random& r, std::size_t size)
{
    if (size == 0 || r.OneIn(5)) {
        return pmx::kNoIndex;
    }
    return static_cast<std::int32_t>(r.Below(size));
}

pmx::Document
Generate(Random& r)
{
    pmx::Document doc;
    doc.header.version = r.OneIn(2) ? pmx::Version::V2_0 : pmx::Version::V2_1;
    doc.header.globals.additionalVec4Count = static_cast<std::uint8_t>(r.Below(5));
    doc.model = {WildName(r), WildName(r), WildName(r), WildName(r)};

    const std::size_t bones = r.OneIn(6) ? 0 : r.Below(12);
    for (std::size_t i = 0; i < bones; ++i) {
        pmx::Bone b;
        b.name = WildName(r);
        b.englishName = WildName(r);
        b.position = WildVec3(r);
        // Anything the parser can leave, cycles and self-parents included.
        b.parent = Index(r, bones);
        // Every flag, undefined bits too, and relations that name any bone,
        // this one included, or none.
        b.flags = static_cast<std::uint16_t>(r.Below(0x10000));
        b.transformLayer = static_cast<std::int32_t>(r.Below(5)) - 2;
        b.tailBone = Index(r, bones);
        b.tailOffset = WildVec3(r);
        b.appendParent = Index(r, bones);
        b.appendRatio = WildFloat(r);
        b.fixedAxis = WildVec3(r);
        b.localAxisX = WildVec3(r);
        b.localAxisZ = WildVec3(r);
        b.externalParentKey = static_cast<std::int32_t>(r.Below(8));
        b.ik.target = Index(r, bones);
        b.ik.loopCount = static_cast<std::int32_t>(r.Below(100));
        b.ik.limitAngle = WildFloat(r);
        const std::size_t links = r.Below(4);
        for (std::size_t k = 0; k < links; ++k) {
            b.ik.links.push_back({Index(r, bones), r.OneIn(2), WildVec3(r), WildVec3(r)});
        }
        doc.bones.push_back(b);
    }

    const std::size_t vertices = r.Below(10);
    for (std::size_t i = 0; i < vertices; ++i) {
        pmx::Vertex v;
        v.position = WildVec3(r);
        v.normal = WildVec3(r);
        v.uv = {WildFloat(r), WildFloat(r)};
        for (auto& a : v.additionalVec4) {
            a = {WildFloat(r), WildFloat(r), WildFloat(r), WildFloat(r)};
        }
        v.deform.type = static_cast<pmx::DeformType>(r.Below(5));
        for (auto& b : v.deform.bones) {
            b = Index(r, bones);
        }
        for (auto& w : v.deform.weights) {
            w = WildFloat(r);
        }
        v.deform.sdefC = WildVec3(r);
        v.deform.sdefR0 = WildVec3(r);
        v.deform.sdefR1 = WildVec3(r);
        v.edgeScale = WildFloat(r);
        doc.vertices.push_back(v);
    }

    const std::size_t triangles = vertices == 0 ? 0 : r.Below(8);
    for (std::size_t i = 0; i < 3 * triangles; ++i) {
        doc.faces.push_back(static_cast<std::uint32_t>(r.Below(vertices)));
    }

    const std::size_t textures = r.Below(5);
    for (std::size_t i = 0; i < textures; ++i) {
        doc.textures.push_back(WildPath(r));
    }

    // Face counts that fit the table, as the parser guarantees, falling short
    // of it at random.
    std::size_t left = triangles;
    const std::size_t materials = r.Below(5);
    for (std::size_t i = 0; i < materials; ++i) {
        pmx::Material m;
        m.name = WildName(r);
        m.englishName = WildName(r);
        m.flags = static_cast<std::uint8_t>(r.Below(256));
        m.texture = Index(r, textures);
        m.sphereTexture = Index(r, textures);
        m.toonReference = r.OneIn(2) ? pmx::ToonReference::Texture : pmx::ToonReference::Shared;
        m.toonTexture = Index(r, textures);
        const std::size_t faces = i + 1 == materials && r.OneIn(2) ? left : r.Below(left + 1);
        m.faceCount = static_cast<std::int32_t>(3 * faces);
        left -= faces;
        doc.materials.push_back(m);
    }
    // Rigid bodies with any bone, shape, mode and transform, undefined values
    // included, and joints between any bodies or none.
    const std::size_t bodies = r.Below(4);
    doc.rigidBodies.resize(bodies);
    for (pmx::RigidBody& b : doc.rigidBodies) {
        b.name = WildName(r);
        b.englishName = WildName(r);
        b.bone = Index(r, bones);
        b.group = static_cast<std::uint8_t>(r.Below(20));
        b.nonCollisionMask = static_cast<std::uint16_t>(r.Below(0x10000));
        b.shape = static_cast<std::uint8_t>(r.Below(5));
        b.size = WildVec3(r);
        b.position = WildVec3(r);
        b.rotation = WildVec3(r);
        b.mass = WildFloat(r);
        b.physicsMode = static_cast<std::uint8_t>(r.Below(5));
    }
    doc.joints.resize(r.Below(4));
    for (pmx::Joint& j : doc.joints) {
        j.name = WildName(r);
        j.englishName = WildName(r);
        j.type = static_cast<std::uint8_t>(r.Below(8));
        j.rigidBodyA = Index(r, bodies);
        j.rigidBodyB = Index(r, bodies);
        j.position = WildVec3(r);
        j.rotation = WildVec3(r);
        j.translationMin = WildVec3(r);
        j.translationMax = WildVec3(r);
        j.rotationMin = WildVec3(r);
        j.rotationMax = WildVec3(r);
        j.translationSpring = WildVec3(r);
        j.rotationSpring = WildVec3(r);
    }
    // Morphs of every type, with members that name later morphs, earlier
    // ones, or themselves: a group graph with any cycle in it.
    const std::size_t morphs = r.OneIn(5) ? 0 : r.Below(10);
    doc.morphs.resize(morphs);
    for (std::size_t i = 0; i < morphs; ++i) {
        pmx::Morph& m = doc.morphs[i];
        m.name = WildName(r);
        m.englishName = WildName(r);
        m.panel = static_cast<std::uint8_t>(r.Below(8)); // 5-7 are undefined
        m.type = static_cast<pmx::MorphType>(r.Below(11));
        const std::size_t offsets = r.Below(4);
        for (std::size_t k = 0; k < offsets; ++k) {
            switch (m.type) {
            case pmx::MorphType::Group:
            case pmx::MorphType::Flip:
                m.groupOffsets.push_back({Index(r, morphs), WildFloat(r)});
                break;
            case pmx::MorphType::Vertex:
                m.vertexOffsets.push_back({Index(r, vertices), WildVec3(r)});
                break;
            case pmx::MorphType::Bone:
                m.boneOffsets.push_back({Index(r, bones),
                                         WildVec3(r),
                                         {WildFloat(r), WildFloat(r), WildFloat(r), WildFloat(r)}});
                break;
            case pmx::MorphType::Uv:
            case pmx::MorphType::AdditionalUv1:
            case pmx::MorphType::AdditionalUv2:
            case pmx::MorphType::AdditionalUv3:
            case pmx::MorphType::AdditionalUv4: {
                pmx::UvOffset o;
                o.vertex = Index(r, vertices);
                o.delta = {WildFloat(r), WildFloat(r), WildFloat(r), WildFloat(r)};
                m.uvOffsets.push_back(o);
                break;
            }
            case pmx::MorphType::Material: {
                pmx::MaterialOffset o;
                o.material = Index(r, materials);
                o.operation = static_cast<std::uint8_t>(r.Below(3)); // 2 is undefined
                o.diffuse = {WildFloat(r), WildFloat(r), WildFloat(r), WildFloat(r)};
                o.specularPower = WildFloat(r);
                o.edgeSize = WildFloat(r);
                m.materialOffsets.push_back(o);
                break;
            }
            case pmx::MorphType::Impulse:
                m.impulseOffsets.push_back({Index(r, doc.rigidBodies.size()),
                                            static_cast<std::uint8_t>(r.Below(2)),
                                            WildVec3(r),
                                            WildVec3(r)});
                break;
            }
        }
    }
    doc.softBodies.resize(r.Below(2));
    return doc;
}

bool
IsIdentifier(const std::string& id)
{
    if (id.empty() || (id[0] >= '0' && id[0] <= '9')) {
        return false;
    }
    for (char c : id) {
        const bool ok =
            (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
        if (!ok) {
            return false;
        }
    }
    return true;
}

std::string
Lower(std::string s)
{
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return s;
}

/// Whether expanding every group and flip morph terminates: no member
/// reaches its own morph (PMX_CONTRACT.md §10). Written as its own walk, so
/// it is not the canonicalizer's algorithm checking itself.
bool
MorphMembersTerminate(const std::vector<mmd::Morph>& morphs)
{
    std::vector<std::uint8_t> state(morphs.size(), 0); // 0 unvisited, 1 on the walk, 2 settled
    std::vector<std::pair<std::size_t, std::size_t>> walk;
    for (std::size_t start = 0; start < morphs.size(); ++start) {
        if (state[start] != 0) {
            continue;
        }
        state[start] = 1;
        walk.assign(1, {start, 0});
        while (!walk.empty()) {
            const std::size_t morph = walk.back().first;
            if (walk.back().second >= morphs[morph].members.size()) {
                state[morph] = 2;
                walk.pop_back();
                continue;
            }
            const std::size_t k = walk.back().second++;
            const std::size_t to = static_cast<std::size_t>(morphs[morph].members[k].morph);
            if (state[to] == 1) {
                return false;
            }
            if (state[to] == 0) {
                state[to] = 1;
                walk.push_back({to, 0});
            }
        }
    }
    return true;
}

/// The first promise of CanonicalDocument.h the result breaks, or "".
std::string
Violation(const pmx::Document& doc, const mmd::CanonicalDocument& c)
{
    const std::size_t nv = doc.vertices.size();
    const std::size_t nb = doc.bones.size();
    const mmd::Mesh& mesh = c.mesh;

    // Skeleton: a permutation, parents first, identifiers unique and valid.
    if (c.skeleton.bones.size() != nb || c.skeleton.jointOfSourceBone.size() != nb) {
        return "the skeleton is not one joint per bone";
    }
    std::set<std::string> ids;
    std::set<std::string> paths;
    std::vector<bool> seen(nb, false);
    for (std::size_t j = 0; j < nb; ++j) {
        const mmd::Bone& bone = c.skeleton.bones[j];
        if (bone.sourceIndex >= nb || seen[bone.sourceIndex]) {
            return "the joint order is not a permutation";
        }
        seen[bone.sourceIndex] = true;
        if (c.skeleton.jointOfSourceBone[bone.sourceIndex] != static_cast<std::int32_t>(j)) {
            return "jointOfSourceBone disagrees with the order";
        }
        if (bone.parent != mmd::kNone &&
            (bone.parent < 0 || bone.parent >= static_cast<std::int32_t>(j))) {
            return "a joint's parent does not precede it";
        }
        if (!IsIdentifier(bone.name.stableId) || !ids.insert(Lower(bone.name.stableId)).second) {
            return "a bone identifier is invalid or not unique: " + bone.name.stableId;
        }
        if (!paths.insert(bone.jointPath).second) {
            return "two joints share a path";
        }
        const std::string expected =
            bone.parent == mmd::kNone
                ? bone.name.stableId
                : c.skeleton.bones[static_cast<std::size_t>(bone.parent)].jointPath + "/" +
                      bone.name.stableId;
        if (bone.jointPath != expected) {
            return "a joint path is not its parent's path and its identifier";
        }
    }

    // Rig: one control per joint, every kept joint index naming a joint, one
    // chain per IK bone with an effector, in bone-table order.
    const auto joint = [nb](std::int32_t j) { return j >= 0 && static_cast<std::size_t>(j) < nb; };
    if (c.rig.bones.size() != nb) {
        return "the rig is not one control per joint";
    }
    for (const mmd::BoneControl& b : c.rig.bones) {
        if ((b.tailJoint != mmd::kNone && !joint(b.tailJoint)) ||
            (b.appendSource != mmd::kNone && !joint(b.appendSource))) {
            return "a bone's tail or append source names no joint";
        }
        if (b.appendSource == mmd::kNone &&
            (b.appendRatio != 0.0f || b.appendRotation || b.appendTranslation || b.appendLocal)) {
            return "a bone that appends nothing keeps append fields";
        }
    }
    std::size_t chains = 0;
    std::int32_t previous = -1;
    for (std::size_t i = 0; i < nb; ++i) {
        const pmx::Bone& b = doc.bones[i];
        if ((b.flags & pmx::BoneFlag::Ik) && b.ik.target >= 0 &&
            static_cast<std::size_t>(b.ik.target) < nb) {
            ++chains;
        }
    }
    if (c.rig.ikChains.size() != chains) {
        return "the IK chains are not one per IK bone with an effector";
    }
    for (const mmd::IkChain& chain : c.rig.ikChains) {
        if (!joint(chain.joint) || !joint(chain.effector)) {
            return "an IK chain's joint or effector names no joint";
        }
        const auto source = static_cast<std::int32_t>(
            c.skeleton.bones[static_cast<std::size_t>(chain.joint)].sourceIndex);
        if (source <= previous) {
            return "the IK chains are not in bone-table order";
        }
        previous = source;
        for (const mmd::IkLink& link : chain.links) {
            if (!joint(link.joint)) {
                return "an IK link names no joint";
            }
        }
    }

    // Mesh: every array sized to the vertices, faces in range.
    if (mesh.points.size() != nv || mesh.normals.size() != nv || mesh.st.size() != nv ||
        mesh.edgeScale.size() != nv) {
        return "a per-vertex array is not sized to the vertices";
    }
    for (std::size_t k = 0; k < 4; ++k) {
        if (mesh.additionalUv[k].size() != (k < mesh.additionalUvCount ? nv : 0)) {
            return "an additional UV channel has the wrong size";
        }
    }
    if (mesh.faceVertexIndices.size() != doc.faces.size()) {
        return "the face table changed size";
    }
    for (std::int32_t v : mesh.faceVertexIndices) {
        if (v < 0 || static_cast<std::size_t>(v) >= nv) {
            return "a face names no vertex";
        }
    }

    // Skinning: present exactly with bones, joints in range, weights summing to 1.
    const std::size_t n = mesh.influencesPerVertex;
    if (nb == 0) {
        if (n != 0 || !mesh.jointIndices.empty() || !mesh.deformTypes.empty()) {
            return "a model without bones is skinned";
        }
    } else if (nv > 0) {
        if ((n != 1 && n != 2 && n != 4) || mesh.jointIndices.size() != nv * n ||
            mesh.jointWeights.size() != nv * n || mesh.deformTypes.size() != nv) {
            return "the skinning arrays are not N per vertex";
        }
        for (std::size_t v = 0; v < nv; ++v) {
            double sum = 0.0;
            for (std::size_t k = 0; k < n; ++k) {
                const std::int32_t joint = mesh.jointIndices[v * n + k];
                const float w = mesh.jointWeights[v * n + k];
                if (joint < 0 || static_cast<std::size_t>(joint) >= nb) {
                    return "a joint index is out of range";
                }
                if (!std::isfinite(w) || w < 0.0f) {
                    return "a weight is negative or not finite";
                }
                sum += w;
            }
            if (std::abs(sum - 1.0) > 1e-4) {
                return "a vertex's weights do not sum to 1";
            }
        }
        const bool sdef = mesh.sdefVertexCount > 0;
        if (mesh.sdefC.size() != (sdef ? nv : 0)) {
            return "the SDEF arrays are the wrong size";
        }
    }

    // Materials: contiguous ranges within the faces, unique identifiers,
    // texture slots that name a texture.
    std::set<std::string> materialIds;
    std::size_t next = 0;
    for (const mmd::Material& m : c.materials) {
        if (m.firstFace != next || m.firstFace + m.faceCount > mesh.FaceCount()) {
            return "a material's face range is not the next one, or leaves the faces";
        }
        next += m.faceCount;
        if (!IsIdentifier(m.name.stableId) || !materialIds.insert(Lower(m.name.stableId)).second) {
            return "a material identifier is invalid or not unique";
        }
        for (std::int32_t t : {m.texture, m.sphereTexture, m.toonTexture}) {
            if (t != mmd::kNone && (t < 0 || static_cast<std::size_t>(t) >= c.textures.size())) {
                return "a texture slot names no texture";
            }
        }
    }
    if (mesh.materialsCoverFaces != (next == mesh.FaceCount())) {
        return "materialsCoverFaces disagrees with the ranges";
    }

    // Morphs: one per source morph, in source order, with every kept index
    // naming an element and no group member reaching its own morph.
    if (c.morphs.size() != doc.morphs.size()) {
        return "the morph table changed size";
    }
    std::set<std::string> morphIds;
    for (std::size_t i = 0; i < c.morphs.size(); ++i) {
        const mmd::Morph& m = c.morphs[i];
        if (m.sourceIndex != i) {
            return "a morph is not in morph-table order";
        }
        if (!IsIdentifier(m.name.stableId) || !morphIds.insert(Lower(m.name.stableId)).second) {
            return "a morph identifier is invalid or not unique";
        }
        if (static_cast<std::uint8_t>(m.panel) > 4) {
            return "a morph panel is not one of the five";
        }
        for (const mmd::MorphMember& member : m.members) {
            if (member.morph < 0 || static_cast<std::size_t>(member.morph) >= c.morphs.size()) {
                return "a group member names no morph";
            }
        }
        for (const mmd::MorphVertexOffset& o : m.vertexOffsets) {
            if (o.vertex < 0 || static_cast<std::size_t>(o.vertex) >= nv) {
                return "a vertex morph offset names no vertex";
            }
        }
        for (const mmd::MorphBoneOffset& o : m.boneOffsets) {
            if (o.joint < 0 || static_cast<std::size_t>(o.joint) >= nb) {
                return "a bone morph offset names no joint";
            }
        }
        for (const mmd::MorphUvOffset& o : m.uvOffsets) {
            if (o.vertex < 0 || static_cast<std::size_t>(o.vertex) >= nv) {
                return "a UV morph offset names no vertex";
            }
        }
        for (const mmd::MorphMaterialOffset& o : m.materialOffsets) {
            if (o.material != mmd::kNone &&
                (o.material < 0 || static_cast<std::size_t>(o.material) >= c.materials.size())) {
                return "a material morph offset names no material";
            }
        }
        for (const mmd::MorphImpulseOffset& o : m.impulseOffsets) {
            if (o.rigidBody < 0 ||
                static_cast<std::size_t>(o.rigidBody) >= doc.rigidBodies.size()) {
                return "an impulse morph offset names no rigid body";
            }
        }
    }
    // Physics: every body kept in order, every kept index naming what it
    // should, every enumeration defined, every joint joining two bodies.
    const std::size_t nr = doc.rigidBodies.size();
    const auto body = [nr](std::int32_t k) { return k >= 0 && static_cast<std::size_t>(k) < nr; };
    if (c.physics.rigidBodies.size() != nr) {
        return "a rigid body was dropped";
    }
    for (std::size_t i = 0; i < nr; ++i) {
        const mmd::RigidBody& b = c.physics.rigidBodies[i];
        if (b.sourceIndex != i) {
            return "the rigid bodies are not in source order";
        }
        if (b.bone != mmd::kNone && (b.bone < 0 || static_cast<std::size_t>(b.bone) >= nb)) {
            return "a rigid body's bone names no joint";
        }
        if (static_cast<int>(b.shape) > 2 || static_cast<int>(b.mode) > 2) {
            return "a rigid body's shape or mode is undefined";
        }
        if (b.orientation[3] < 0.0f) {
            return "a rigid body's orientation has a negative w";
        }
    }
    std::size_t joined = 0;
    for (const pmx::Joint& j : doc.joints) {
        joined += body(j.rigidBodyA) && body(j.rigidBodyB) ? 1 : 0;
    }
    if (c.physics.joints.size() != joined) {
        return "the joints are not every joint between two bodies";
    }
    const int lastType = doc.header.version == pmx::Version::V2_1 ? 5 : 0;
    for (const mmd::PhysicsJoint& j : c.physics.joints) {
        if (!body(j.rigidBodyA) || !body(j.rigidBodyB)) {
            return "a joint names no rigid body";
        }
        if (static_cast<int>(j.type) > lastType) {
            return "a joint's type is undefined in its version";
        }
    }

    if (!MorphMembersTerminate(c.morphs)) {
        return "a group morph reaches its own morph";
    }

    // Textures: an authored path is anchored, relative, and stays inside.
    if (c.textures.size() != doc.textures.size()) {
        return "the texture table changed size";
    }
    for (std::size_t i = 0; i < c.textures.size(); ++i) {
        const std::string& p = c.textures[i].assetPath;
        if (c.textures[i].sourcePath != doc.textures[i]) {
            return "a texture's source path is not verbatim";
        }
        if (p.empty()) {
            continue;
        }
        if (p.rfind("./", 0) != 0 || p.find('\\') != std::string::npos ||
            p.find("//") != std::string::npos || p == "./.." || p.rfind("./../", 0) == 0 ||
            p.find("/./") != std::string::npos) {
            return "an authored texture path is not normalized: " + p;
        }
        // No control character: SdfAssetPath would refuse the path.
        for (std::size_t k = 0; k < p.size(); ++k) {
            const auto c = static_cast<unsigned char>(p[k]);
            const bool c1 = c == 0xC2 && k + 1 < p.size() &&
                            static_cast<unsigned char>(p[k + 1]) >= 0x80 &&
                            static_cast<unsigned char>(p[k + 1]) <= 0x9F;
            if (c < 0x20 || c == 0x7F || c1) {
                return "an authored texture path holds a control character";
            }
        }
    }
    return {};
}

/// Every value of a canonical document, as bytes: the same document must
/// produce the same bits, which operator== cannot say once a NaN is present.
class Fingerprint {
public:
    explicit Fingerprint(const mmd::CanonicalDocument& c)
    {
        const mmd::Mesh& m = c.mesh;
        Add(c.metadata.name, c.metadata.englishName, c.metadata.comment, c.metadata.englishComment);
        for (const mmd::Texture& t : c.textures) {
            Add(t.sourcePath, t.assetPath);
        }
        Raw(m.points);
        Raw(m.normals);
        Raw(m.st);
        for (const auto& channel : m.additionalUv) {
            Raw(channel);
        }
        Raw(m.edgeScale);
        Raw(m.faceVertexIndices);
        Raw(m.deformTypes);
        Raw(m.jointIndices);
        Raw(m.jointWeights);
        Raw(m.sdefC);
        Raw(m.sdefR0);
        Raw(m.sdefR1);
        Add(std::to_string(m.influencesPerVertex),
            m.doubleSided ? "d" : "s",
            m.materialsCoverFaces ? "p" : "n");
        for (const mmd::Material& mat : c.materials) {
            Add(mat.name.source,
                mat.name.english,
                mat.name.stableId,
                std::to_string(mat.firstFace) + "," + std::to_string(mat.faceCount) + "," +
                    std::to_string(mat.texture) + "," + std::to_string(mat.sphereTexture) + "," +
                    std::to_string(mat.toonTexture));
        }
        for (const mmd::Bone& b : c.skeleton.bones) {
            Add(b.name.source,
                b.jointPath,
                std::to_string(b.sourceIndex),
                std::to_string(b.parent));
            Raw(std::vector<mmd::Double3>{b.position, b.localTranslation});
        }
        for (const mmd::BoneControl& b : c.rig.bones) {
            Add(std::to_string(b.transformLayer),
                std::to_string(b.tailJoint),
                std::to_string(b.appendSource),
                std::to_string(b.externalParentKey));
            Raw(std::vector<mmd::Float3>{b.tailOffset, b.fixedAxis, b.localAxisX, b.localAxisZ});
            Raw(std::vector<float>{b.appendRatio});
        }
        for (const mmd::IkChain& chain : c.rig.ikChains) {
            Add(std::to_string(chain.joint),
                std::to_string(chain.effector),
                std::to_string(chain.loopCount));
            Raw(std::vector<float>{chain.limitAngle});
            for (const mmd::IkLink& link : chain.links) {
                Add(std::to_string(link.joint));
                Raw(std::vector<mmd::Float3>{link.lowerLimit, link.upperLimit});
            }
        }
        for (const mmd::RigidBody& b : c.physics.rigidBodies) {
            Add(b.name.stableId,
                std::to_string(b.bone),
                std::to_string(b.collisionGroup) + "," + std::to_string(b.collisionMask) + "," +
                    std::to_string(static_cast<int>(b.shape)) + "," +
                    std::to_string(static_cast<int>(b.mode)));
            Raw(std::vector<mmd::Float3>{b.size});
            Raw(std::vector<mmd::Double3>{b.position});
            Raw(std::vector<mmd::Float4>{b.orientation});
            Raw(std::vector<float>{
                b.mass, b.linearDamping, b.angularDamping, b.restitution, b.friction});
        }
        for (const mmd::PhysicsJoint& j : c.physics.joints) {
            Add(j.name.stableId,
                std::to_string(static_cast<int>(j.type)),
                std::to_string(j.rigidBodyA) + "," + std::to_string(j.rigidBodyB));
            Raw(std::vector<mmd::Double3>{j.position});
            Raw(std::vector<mmd::Float4>{j.orientation, j.localOrientationA, j.localOrientationB});
            Raw(std::vector<mmd::Float3>{j.translationLowerLimit,
                                         j.translationUpperLimit,
                                         j.rotationLowerLimit,
                                         j.rotationUpperLimit,
                                         j.translationSpring,
                                         j.rotationSpring,
                                         j.localPositionA,
                                         j.localPositionB});
        }
    }

    bool operator==(const Fingerprint&) const = default;

private:
    template <class... Strings> void Add(const Strings&... strings)
    {
        ((_bytes += std::string(strings) + '\x1F'), ...);
    }

    template <class T> void Raw(const std::vector<T>& values)
    {
        const char* data = reinterpret_cast<const char*>(values.data());
        _bytes.append(data, values.size() * sizeof(T));
        _bytes += '\x1E';
    }

    std::string _bytes;
};

} // namespace

int
main()
{
    Random random(0x6D6D644D6F64656Cull); // "mmdModel"
    constexpr std::size_t kDocuments = 20000;
    std::size_t diagnostics = 0;
    for (std::size_t i = 0; i < kDocuments; ++i) {
        const pmx::Document doc = Generate(random);
        const auto first = mmd::Canonicalize(doc);
        assert(first.ok());
        for (const mmd::Diagnostic& d : first.diagnostics()) {
            assert(d.recoverable);
        }
        diagnostics += first.diagnostics().size();
        const std::string violation = Violation(doc, first.value());
        if (!violation.empty()) {
            std::fprintf(stderr, "document %zu: %s\n", i, violation.c_str());
        }
        assert(violation.empty());
        // The same document, the same canonical model, bit for bit.
        const auto second = mmd::Canonicalize(doc);
        assert(Fingerprint(second.value()) == Fingerprint(first.value()));
        assert(second.diagnostics().size() == first.diagnostics().size());
    }
    // The run is only meaningful if the generator reaches the repairs.
    assert(diagnostics > kDocuments);
    std::printf("robustness: %zu documents, %zu diagnostics\n", kDocuments, diagnostics);
    return 0;
}
