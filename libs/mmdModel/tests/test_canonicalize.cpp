// SPDX-License-Identifier: Apache-2.0
//
// Canonicalize over whole documents, stated as data: what each PMX fact
// becomes (PMX_CONTRACT.md §14) and which diagnostics each repair raises.
#include "mmdModel/Canonicalize.h"

#include "mmdModel/Basis.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using namespace mmd;
namespace pmx = mmd::pmx;

float
M(double v)
{
    return static_cast<float>(v * 0.08);
}

pmx::Vertex
MakeVertex(float x, pmx::Deform deform)
{
    pmx::Vertex v;
    v.position = {x, 10.0f + x, -0.5f};
    v.normal = {0.0f, 0.0f, -1.0f};
    v.uv = {x / 8.0f, 0.25f};
    v.additionalVec4[0] = {x, 1.0f, 2.0f, 3.0f};
    v.deform = deform;
    v.edgeScale = 0.5f + x;
    return v;
}

pmx::Deform
MakeDeform(pmx::DeformType type, std::array<std::int32_t, 4> bones,
           std::array<float, 4> weights = {})
{
    pmx::Deform d;
    d.type = type;
    d.bones = bones;
    d.weights = weights;
    return d;
}

pmx::Bone
MakeBone(const char* name, const char* english, pmx::Vec3 position, std::int32_t parent)
{
    pmx::Bone b;
    b.name = name;
    b.englishName = english;
    b.position = position;
    b.parent = parent;
    return b;
}

pmx::Material
MakeMaterial(const char* name, const char* english, std::int32_t faceIndices)
{
    pmx::Material m;
    m.name = name;
    m.englishName = english;
    m.faceCount = faceIndices;
    return m;
}

pmx::Morph
MakeMorph(const char* name, const char* english, std::uint8_t panel, pmx::MorphType type)
{
    pmx::Morph m;
    m.name = name;
    m.englishName = english;
    m.panel = panel;
    m.type = type;
    return m;
}

/// Four bones, one vertex of each deform type and two more, two materials, a
/// texture table with one path of each kind, and one morph of each kind that
/// a 2.1 file can hold: the sample model of the fixture generator, restated.
pmx::Document
SampleDocument()
{
    using pmx::DeformType;
    pmx::Document doc;
    doc.header.version = pmx::Version::V2_1;
    doc.header.globals.additionalVec4Count = 1;
    doc.model = {"サンプル", "Sample", "コメント\r\n", "comment"};
    doc.vertices = {
        MakeVertex(0.0f, MakeDeform(DeformType::Bdef1, {0, -1, -1, -1})),
        MakeVertex(1.0f, MakeDeform(DeformType::Bdef2, {1, 2, -1, -1}, {0.25f})),
        MakeVertex(2.0f, MakeDeform(DeformType::Bdef4, {0, 1, 2, -1}, {0.5f, 0.3f, 0.2f, 0.0f})),
        MakeVertex(3.0f, MakeDeform(DeformType::Sdef, {1, 2, -1, -1}, {0.5f})),
        MakeVertex(4.0f, MakeDeform(DeformType::Qdef, {0, 1, -1, -1}, {0.5f, 0.5f})),
        MakeVertex(5.0f, MakeDeform(DeformType::Bdef2, {2, -1, -1, -1}, {1.0f})),
    };
    doc.vertices[3].deform.sdefC = {0.0f, 12.0f, 1.0f};
    doc.vertices[3].deform.sdefR0 = {0.0f, 12.5f, 0.0f};
    doc.vertices[3].deform.sdefR1 = {0.0f, 11.5f, 0.0f};
    doc.faces = {0, 1, 2, 2, 3, 4, 3, 4, 5};
    doc.textures = {"tex\\髪.png", "tex/肌.png", "sph\\光沢.sph", "..\\toon\\共有.bmp"};
    doc.materials = {MakeMaterial("髪", "hair", 3), MakeMaterial("肌", "skin", 6)};
    doc.materials[0].flags = pmx::MaterialFlag::NoCulling | pmx::MaterialFlag::DrawsEdge;
    doc.materials[0].diffuse = {0.1f, 0.2f, 0.3f, 0.4f};
    doc.materials[0].specular = {0.5f, 0.6f, 0.7f};
    doc.materials[0].specularPower = 32.0f;
    doc.materials[0].ambient = {0.8f, 0.7f, 0.6f};
    doc.materials[0].edgeColor = {0.9f, 0.8f, 0.7f, 0.6f};
    doc.materials[0].edgeSize = 1.25f;
    doc.materials[0].sphereMode = 3;
    doc.materials[0].memo = "hair memo";
    doc.materials[0].texture = 0;
    doc.materials[0].sphereTexture = 2;
    doc.materials[0].toonReference = pmx::ToonReference::Texture;
    doc.materials[0].toonTexture = 3;
    doc.materials[1].texture = 1;
    doc.materials[1].sphereMode = 1;
    doc.materials[1].toonReference = pmx::ToonReference::Shared;
    doc.materials[1].sharedToon = 1;
    doc.bones = {
        MakeBone("センター", "center", {0.0f, 8.0f, 0.0f}, -1),
        MakeBone("左腕", "LeftArm", {1.5f, 13.0f, 0.5f}, 0),
        MakeBone("左ひじ", "LeftElbow", {3.0f, 12.0f, 0.0f}, 1),
        MakeBone("左手首ＩＫ", "LeftWrist IK", {4.5f, 11.0f, 0.0f}, 0),
    };
    doc.rigidBodies.resize(2);
    doc.morphs = {
        MakeMorph("まばたき", "blink", 2, pmx::MorphType::Vertex),
        MakeMorph("笑い", "smile", 3, pmx::MorphType::Group),
        MakeMorph("肩", "", 4, pmx::MorphType::Bone),
        MakeMorph("UV", "uv", 4, pmx::MorphType::Uv),
        MakeMorph("材質", "material", 4, pmx::MorphType::Material),
        MakeMorph("衝撃", "impulse", 0, pmx::MorphType::Impulse),
    };
    // The second offset names vertex 9, which the table does not hold: the
    // parser would have rejected it, and canonicalization drops it.
    doc.morphs[0].vertexOffsets = {{0, {0.0f, -0.5f, 1.0f}}, {9, {1.0f, 1.0f, 1.0f}}};
    doc.morphs[1].groupOffsets = {{0, 1.0f}, {3, 0.5f}};
    doc.morphs[2].boneOffsets = {{3, {0.0f, 2.0f, 1.0f}, {0.25f, 0.5f, 0.5f, 0.75f}}};
    doc.morphs[3].uvOffsets = {{2, {0.1f, 0.2f, 0.3f, 0.4f}}};
    doc.morphs[4].materialOffsets.resize(1);
    doc.morphs[4].materialOffsets[0].material = -1; // every material
    doc.morphs[4].materialOffsets[0].operation = 1; // add
    doc.morphs[4].materialOffsets[0].diffuse = {0.1f, 0.2f, 0.3f, 0.4f};
    doc.morphs[4].materialOffsets[0].edgeSize = 2.0f;
    doc.morphs[5].impulseOffsets = {{1, 1, {0.0f, 1.0f, 2.0f}, {0.25f, 0.5f, 0.75f}}};
    doc.softBodies.resize(1);
    return doc;
}

CanonicalDocument
ExpectCanonical(const pmx::Document& doc, const std::vector<std::string>& codes = {})
{
    auto result = Canonicalize(doc);
    assert(result.ok());
    std::vector<std::string> got;
    for (const Diagnostic& d : result.diagnostics()) {
        assert(d.recoverable);
        got.push_back(d.code);
    }
    if (got != codes) {
        for (const Diagnostic& d : result.diagnostics()) {
            std::fprintf(stderr, "  %s\n", FormatDiagnostic(d).c_str());
        }
    }
    assert(got == codes);
    return std::move(result).value();
}

void
TestMetadata()
{
    const CanonicalDocument c = ExpectCanonical(SampleDocument(), {"MMD_PATH_UNSAFE_TEXTURE_PATH"});
    assert(c.metadata.sourceVersion == "2.1");
    assert(c.metadata.name == "サンプル");
    assert(c.metadata.englishName == "Sample");
    assert(c.metadata.comment == "コメント\r\n"); // comments are verbatim
    assert(c.metadata.softBodyCount == 1);
}

void
TestMesh()
{
    const CanonicalDocument c = ExpectCanonical(SampleDocument(), {"MMD_PATH_UNSAFE_TEXTURE_PATH"});
    const Mesh& mesh = c.mesh;
    assert(mesh.points.size() == 6);
    // (x, 10 + x, -0.5) in MMD units -> meters, Z mirrored.
    assert((mesh.points[1] == Float3{M(1.0), M(11.0), M(0.5)}));
    // The model faced -Z; it faces +Z now.
    assert((mesh.normals[1] == Float3{0.0f, 0.0f, 1.0f}));
    assert(mesh.st[2][0] == 0.25f && mesh.st[2][1] == 0.75f);
    assert(mesh.additionalUvCount == 1);
    assert((mesh.additionalUv[0][4] == Float4{4.0f, 1.0f, 2.0f, 3.0f})); // raw
    assert(mesh.additionalUv[1].empty());
    assert(mesh.edgeScale[5] == 5.5f);
    // Winding reversed, triangle by triangle.
    assert((mesh.faceVertexIndices == std::vector<std::int32_t>{0, 2, 1, 2, 4, 3, 3, 5, 4}));
    assert(mesh.FaceCount() == 3);
    assert(mesh.materialsCoverFaces);
    assert(mesh.doubleSided); // material 0 disables culling
}

void
TestSkinning()
{
    const CanonicalDocument c = ExpectCanonical(SampleDocument(), {"MMD_PATH_UNSAFE_TEXTURE_PATH"});
    const Mesh& mesh = c.mesh;
    assert(mesh.influencesPerVertex == 4); // BDEF4 and QDEF are present
    const auto joints = [&](std::size_t v) {
        return std::vector<std::int32_t>(mesh.jointIndices.begin() + 4 * v,
                                         mesh.jointIndices.begin() + 4 * v + 4);
    };
    const auto weights = [&](std::size_t v) {
        return std::vector<float>(mesh.jointWeights.begin() + 4 * v,
                                  mesh.jointWeights.begin() + 4 * v + 4);
    };
    // BDEF1: one influence, padded with weight 0 on joint 0.
    assert((joints(0) == std::vector<std::int32_t>{0, 0, 0, 0}));
    assert((weights(0) == std::vector<float>{1.0f, 0.0f, 0.0f, 0.0f}));
    // BDEF2: the second weight is 1 - the first.
    assert((joints(1) == std::vector<std::int32_t>{1, 2, 0, 0}));
    assert((weights(1) == std::vector<float>{0.25f, 0.75f, 0.0f, 0.0f}));
    // BDEF4 with an unused slot: the none is dropped.
    assert((joints(2) == std::vector<std::int32_t>{0, 1, 2, 0}));
    assert((weights(2) == std::vector<float>{0.5f, 0.3f, 0.2f, 0.0f}));
    // BDEF2 whose second bone is none with weight 0.
    assert((joints(5) == std::vector<std::int32_t>{2, 0, 0, 0}));
    assert((weights(5) == std::vector<float>{1.0f, 0.0f, 0.0f, 0.0f}));

    assert((mesh.deformTypes == std::vector<DeformType>{DeformType::Bdef1,
                                                        DeformType::Bdef2,
                                                        DeformType::Bdef4,
                                                        DeformType::Sdef,
                                                        DeformType::Qdef,
                                                        DeformType::Bdef2}));
    assert(mesh.sdefVertexCount == 1 && mesh.qdefVertexCount == 1);
    // SDEF parameters are points: converted, and zero for other vertices.
    assert(mesh.sdefC.size() == 6);
    assert((mesh.sdefC[3] == Float3{0.0f, M(12.0), -M(1.0)}));
    assert((mesh.sdefR0[3] == Float3{0.0f, M(12.5), 0.0f}));
    assert((mesh.sdefC[0] == Float3{}));
}

void
TestSkeleton()
{
    const CanonicalDocument c = ExpectCanonical(SampleDocument(), {"MMD_PATH_UNSAFE_TEXTURE_PATH"});
    const Skeleton& s = c.skeleton;
    assert(s.bones.size() == 4);
    assert(s.bones[1].name.source == "左腕");
    assert(s.bones[1].name.stableId == "LeftArm");
    assert(s.bones[3].name.stableId == "LeftWrist_IK");
    assert(s.bones[2].jointPath == "center/LeftArm/LeftElbow");
    assert(s.bones[3].jointPath == "center/LeftWrist_IK");
    assert(s.bones[2].parent == 1);
    // Bone positions stay in double, each converted from the source float.
    assert((s.bones[1].position == Double3{1.5 * 0.08, 13.0 * 0.08, -(0.5 * 0.08)}));
    // The rest translation is the source offset from the parent, converted.
    assert((s.bones[1].localTranslation == Double3{1.5 * 0.08, 5.0 * 0.08, -(0.5 * 0.08)}));
    assert((s.bones[2].localTranslation == Double3{1.5 * 0.08, -1.0 * 0.08, 0.5 * 0.08}));
    assert(s.bones[0].localTranslation == s.bones[0].position);
    assert((s.jointOfSourceBone == std::vector<std::int32_t>{0, 1, 2, 3}));

    // Reordered: a child listed before its parent. Every bone index -- the
    // parents and the skinning -- follows the canonical order.
    pmx::Document doc = SampleDocument();
    doc.bones[1].parent = 3;
    const CanonicalDocument r =
        ExpectCanonical(doc, {"MMD_SKEL_JOINTS_REORDERED", "MMD_PATH_UNSAFE_TEXTURE_PATH"});
    assert((r.skeleton.jointOfSourceBone == std::vector<std::int32_t>{0, 2, 3, 1}));
    assert(r.skeleton.bones[1].sourceIndex == 3);
    assert(r.skeleton.bones[2].jointPath == "center/LeftWrist_IK/LeftArm");
    assert(r.skeleton.bones[3].jointPath == "center/LeftWrist_IK/LeftArm/LeftElbow");
    // Vertex 1 is BDEF2 over source bones 1 and 2: canonical joints 2 and 3.
    assert(r.mesh.jointIndices[4] == 2 && r.mesh.jointIndices[5] == 3);
}

void
TestWeights()
{
    using pmx::DeformType;
    pmx::Document doc = SampleDocument();
    doc.textures.pop_back();
    doc.materials[0].toonTexture = pmx::kNoIndex;
    // Sums to 2: rescaled. A negative weight is clamped before the sum.
    doc.vertices[2].deform = MakeDeform(DeformType::Bdef4, {0, 1, 2, 3}, {1.0f, 0.5f, 0.5f, -3.0f});
    // Every weight zero: bound fully to the first bone.
    doc.vertices[4].deform = MakeDeform(DeformType::Qdef, {3, 1, -1, -1}, {0.0f, 0.0f});
    // A none with weight (the parser reported it): dropped, the rest rescaled.
    doc.vertices[5].deform = MakeDeform(DeformType::Bdef2, {2, -1, -1, -1}, {0.5f});
    // BDEF1 of no bone at all: bound to the root joint.
    doc.vertices[0].deform = MakeDeform(DeformType::Bdef1, {-1, -1, -1, -1});
    // Within the tolerance: left exactly as stored.
    doc.vertices[1].deform = MakeDeform(DeformType::Bdef4, {0, 1, -1, -1}, {0.5f, 0.500001f});

    const CanonicalDocument c =
        ExpectCanonical(doc, {"MMD_SKEL_WEIGHTS_NORMALIZED", "MMD_SKEL_ZERO_WEIGHTS"});
    const Mesh& mesh = c.mesh;
    assert(mesh.jointWeights[8] == 0.5f && mesh.jointWeights[9] == 0.25f &&
           mesh.jointWeights[10] == 0.25f && mesh.jointWeights[11] == 0.0f);
    assert(mesh.jointIndices[11] == 3); // kept, at weight 0
    assert(mesh.jointIndices[16] == 3 && mesh.jointWeights[16] == 1.0f);
    assert(mesh.jointWeights[17] == 0.0f);
    assert(mesh.jointIndices[20] == 2 && mesh.jointWeights[20] == 1.0f);
    assert(mesh.jointIndices[0] == 0 && mesh.jointWeights[0] == 1.0f);
    assert(mesh.jointWeights[4] == 0.5f && mesh.jointWeights[5] == 0.500001f);

    // Non-finite weights count as zero.
    doc = SampleDocument();
    doc.textures.pop_back();
    doc.materials[0].toonTexture = pmx::kNoIndex;
    doc.vertices[2].deform =
        MakeDeform(DeformType::Bdef4, {0, 1, 2, 3}, {NAN, INFINITY, 0.5f, 0.5f});
    const CanonicalDocument f = ExpectCanonical(doc);
    assert(f.mesh.jointWeights[8] == 0.0f && f.mesh.jointWeights[9] == 0.0f);
    assert(f.mesh.jointWeights[10] == 0.5f && f.mesh.jointWeights[11] == 0.5f);

    // Diagnostics once per import, whatever the count.
    doc = SampleDocument();
    doc.textures.pop_back();
    doc.materials[0].toonTexture = pmx::kNoIndex;
    for (pmx::Vertex& v : doc.vertices) {
        v.deform = MakeDeform(DeformType::Bdef4, {0, 1, 2, 3}, {1.0f, 1.0f, 0.0f, 0.0f});
    }
    const auto result = Canonicalize(doc);
    assert(result.diagnostics().size() == 1);
    assert(result.diagnostics()[0].message.find("6 vertices'") == 0);
}

void
TestMaterials()
{
    const CanonicalDocument c = ExpectCanonical(SampleDocument(), {"MMD_PATH_UNSAFE_TEXTURE_PATH"});
    assert(c.textures.size() == 4);
    assert(c.textures[0].sourcePath == "tex\\髪.png");
    assert(c.textures[0].assetPath == "./tex/髪.png");
    assert(c.textures[3].sourcePath == "..\\toon\\共有.bmp"); // kept as provenance
    assert(c.textures[3].assetPath.empty());                  // and not followed

    const Material& hair = c.materials[0];
    assert(hair.name.stableId == "hair" && hair.name.source == "髪");
    assert(hair.firstFace == 0 && hair.faceCount == 1);
    assert((hair.diffuseColor == Float4{0.1f, 0.2f, 0.3f, 0.4f}));
    assert((hair.specularColor == Float3{0.5f, 0.6f, 0.7f}));
    assert(hair.specularPower == 32.0f);
    assert((hair.ambientColor == Float3{0.8f, 0.7f, 0.6f}));
    assert(hair.doubleSided);
    assert(!hair.groundShadow && !hair.castSelfShadow && !hair.receiveSelfShadow);
    assert(hair.drawEdge && !hair.vertexColor && !hair.drawPoints && !hair.drawLines);
    assert((hair.edgeColor == Float4{0.9f, 0.8f, 0.7f, 0.6f}));
    assert(hair.edgeSize == 1.25f);
    assert(hair.texture == 0 && hair.sphereTexture == 2 && hair.toonTexture == 3);
    assert(hair.sphereMode == SphereMode::SubTexture);
    assert(hair.toonSource == ToonSource::Individual && hair.sharedToonIndex == kNone);
    assert(hair.memo == "hair memo");
    const Material& skin = c.materials[1];
    assert(skin.sourceIndex == 1 && skin.firstFace == 1 && skin.faceCount == 2);
    assert(!skin.doubleSided);
    assert(skin.texture == 1 && skin.sphereTexture == kNone);
    assert(skin.sphereMode == SphereMode::Multiply);
    assert(skin.toonSource == ToonSource::Shared && skin.sharedToonIndex == 1);
    assert(skin.toonTexture == kNone); // a shared toon slot is no texture

    pmx::Document invalid = SampleDocument();
    invalid.textures.pop_back();
    invalid.materials[0].toonTexture = pmx::kNoIndex;
    invalid.materials[0].sphereMode = 4;
    const CanonicalDocument repaired =
        ExpectCanonical(invalid, {"MMD_MATERIAL_UNSUPPORTED_SPHERE_MODE"});
    assert(repaired.materials[0].sphereMode == SphereMode::Disabled);

    pmx::Document invalidToon = SampleDocument();
    invalidToon.textures.pop_back();
    invalidToon.materials[0].toonTexture = pmx::kNoIndex;
    invalidToon.materials[1].sharedToon = 10;
    const CanonicalDocument repairedToon =
        ExpectCanonical(invalidToon, {"MMD_MATERIAL_UNSUPPORTED_TOON_SLOT"});
    assert(repairedToon.materials[1].toonSource == ToonSource::None);
    assert(repairedToon.materials[1].sharedToonIndex == kNone);

    // Short: the tail is unbound and the subsets no longer partition.
    pmx::Document doc = SampleDocument();
    doc.materials[1].faceCount = 3;
    assert(!ExpectCanonical(doc, {"MMD_PATH_UNSAFE_TEXTURE_PATH"}).mesh.materialsCoverFaces);

    // A no-cull material that draws nothing makes nothing double-sided.
    doc = SampleDocument();
    doc.materials[0].faceCount = 0;
    doc.materials[1].faceCount = 9;
    const CanonicalDocument empty = ExpectCanonical(doc, {"MMD_PATH_UNSAFE_TEXTURE_PATH"});
    assert(!empty.mesh.doubleSided);
    assert(empty.materials[0].faceCount == 0 && empty.materials[1].faceCount == 3);
}

void
TestNames()
{
    pmx::Document doc = SampleDocument();
    doc.textures.pop_back();
    doc.materials[0].toonTexture = pmx::kNoIndex;
    doc.model.name = std::string("サンプル\0\0", 14);
    doc.bones[1].englishName = "";       // falls back to its index
    doc.bones[2].englishName = "Center"; // collides with bone 0, ignoring case
    doc.materials[1].name = std::string("肌\0", 4);
    const CanonicalDocument c = ExpectCanonical(
        doc, {"MMD_TEXT_TRAILING_NUL", "MMD_TEXT_TRAILING_NUL", "MMD_USD_IDENTIFIER_COLLISION"});
    assert(c.metadata.name == "サンプル");
    assert(c.materials[1].name.source == "肌");
    assert(c.skeleton.bones[1].name.stableId == "bone_0001");
    assert(c.skeleton.bones[2].name.stableId == "Center_2");
    assert(c.skeleton.bones[2].jointPath == "center/bone_0001/Center_2");
}

void
TestWithoutBones()
{
    pmx::Document doc = SampleDocument();
    doc.textures.pop_back();
    doc.materials[0].toonTexture = pmx::kNoIndex;
    doc.bones.clear();
    for (pmx::Vertex& v : doc.vertices) {
        v.deform = MakeDeform(pmx::DeformType::Bdef1, {-1, -1, -1, -1});
    }
    const CanonicalDocument c = ExpectCanonical(doc);
    assert(c.skeleton.bones.empty());
    assert(c.mesh.points.size() == 6);
    assert(c.mesh.influencesPerVertex == 0);
    assert(c.mesh.jointIndices.empty() && c.mesh.deformTypes.empty());
    assert(c.mesh.sdefC.empty());

    const CanonicalDocument empty = ExpectCanonical(pmx::Document{});
    assert(empty.mesh.points.empty() && empty.materials.empty() && empty.textures.empty());
    assert(empty.metadata.sourceVersion == "2.0");
}

void
TestMorphs()
{
    const CanonicalDocument c = ExpectCanonical(SampleDocument(), {"MMD_PATH_UNSAFE_TEXTURE_PATH"});
    assert(c.morphs.size() == 6);
    for (std::size_t i = 0; i < c.morphs.size(); ++i) {
        assert(c.morphs[i].sourceIndex == i); // morph-table order
    }

    // Identity and provenance, as every element carries it.
    const Morph& blink = c.morphs[0];
    assert(blink.name.source == "まばたき" && blink.name.stableId == "blink");
    assert(c.morphs[2].name.source == "肩" && c.morphs[2].name.stableId == "morph_0002");
    assert(blink.panel == MorphPanel::Eye);
    assert(c.morphs[1].panel == MorphPanel::Mouth);
    assert(c.morphs[5].panel == MorphPanel::Hidden);

    // A vertex morph: a displacement, and the offset of a vertex the table
    // does not hold dropped rather than repaired.
    assert(blink.type == MorphType::Vertex);
    assert(blink.vertexOffsets.size() == 1);
    assert(blink.vertexOffsets[0].vertex == 0);
    assert((blink.vertexOffsets[0].offset == Float3{0.0f, M(-0.5), M(-1.0)}));

    // A group morph: members by canonical index, weights unchanged.
    const Morph& smile = c.morphs[1];
    assert(smile.type == MorphType::Group);
    assert(smile.members.size() == 2);
    assert(smile.members[0].morph == 0 && smile.members[0].weight == 1.0f);
    assert(smile.members[1].morph == 3 && smile.members[1].weight == 0.5f);

    // A bone morph: the joint in canonical order, a displacement, and a
    // quaternion with X and Y negated.
    const Morph& shoulder = c.morphs[2];
    assert(shoulder.type == MorphType::Bone);
    assert(shoulder.boneOffsets.size() == 1);
    assert(shoulder.boneOffsets[0].joint == c.skeleton.jointOfSourceBone[3]);
    assert((shoulder.boneOffsets[0].translation == Float3{0.0f, M(2.0), M(-1.0)}));
    assert((shoulder.boneOffsets[0].rotation == Float4{-0.25f, -0.5f, 0.5f, 0.75f}));

    // A UV morph: raw, in the channel it modifies -- never flipped.
    const Morph& uv = c.morphs[3];
    assert(uv.type == MorphType::Uv);
    assert(uv.uvOffsets.size() == 1 && uv.uvOffsets[0].vertex == 2);
    assert((uv.uvOffsets[0].delta == Float4{0.1f, 0.2f, 0.3f, 0.4f}));

    // A material morph: -1 keeps PMX's "every material".
    const Morph& material = c.morphs[4];
    assert(material.type == MorphType::Material);
    assert(material.materialOffsets.size() == 1);
    assert(material.materialOffsets[0].material == kNone);
    assert(material.materialOffsets[0].operation == MaterialMorphOperation::Add);
    assert((material.materialOffsets[0].diffuseColor == Float4{0.1f, 0.2f, 0.3f, 0.4f}));
    assert(material.materialOffsets[0].edgeSize == 2.0f);

    // An impulse morph: the rigid body by its source index, a velocity as a
    // displacement, a torque as an axial vector.
    const Morph& impulse = c.morphs[5];
    assert(impulse.type == MorphType::Impulse);
    assert(impulse.impulseOffsets.size() == 1);
    assert(impulse.impulseOffsets[0].rigidBody == 1);
    assert(impulse.impulseOffsets[0].local);
    assert((impulse.impulseOffsets[0].velocity == Float3{0.0f, M(1.0), M(-2.0)}));
    assert((impulse.impulseOffsets[0].torque == Float3{-0.25f, -0.5f, 0.75f}));
}

void
TestMorphPanel()
{
    pmx::Document doc = SampleDocument();
    doc.morphs[3].panel = 7;
    const CanonicalDocument c =
        ExpectCanonical(doc, {"MMD_PATH_UNSAFE_TEXTURE_PATH", "MMD_MORPH_UNKNOWN_PANEL"});
    assert(c.morphs[3].panel == MorphPanel::Other);
}

void
TestMorphCycles()
{
    // 1 names itself and a vertex morph; 6 and 7 name each other. Each member
    // that closes a cycle goes, and nothing else does.
    pmx::Document doc = SampleDocument();
    doc.morphs[1].groupOffsets = {{1, 1.0f}, {0, 0.5f}};
    doc.morphs.push_back(MakeMorph("輪1", "", 4, pmx::MorphType::Group));
    doc.morphs.push_back(MakeMorph("輪2", "", 4, pmx::MorphType::Group));
    doc.morphs[6].groupOffsets = {{7, 1.0f}};
    doc.morphs[7].groupOffsets = {{6, 1.0f}};
    const CanonicalDocument c = ExpectCanonical(
        doc, {"MMD_PATH_UNSAFE_TEXTURE_PATH", "MMD_MORPH_GROUP_CYCLE", "MMD_MORPH_GROUP_CYCLE"});
    assert(c.morphs[1].members.size() == 1);
    assert(c.morphs[1].members[0].morph == 0 && c.morphs[1].members[0].weight == 0.5f);
    assert(c.morphs[6].members.size() == 1 && c.morphs[6].members[0].morph == 7);
    assert(c.morphs[7].members.empty());

    // A member that names no morph is dropped without a cycle diagnostic.
    pmx::Document none = SampleDocument();
    none.morphs[1].groupOffsets = {{-1, 1.0f}, {0, 0.5f}};
    const CanonicalDocument dropped = ExpectCanonical(none, {"MMD_PATH_UNSAFE_TEXTURE_PATH"});
    assert(dropped.morphs[1].members.size() == 1);
    assert(dropped.morphs[1].members[0].morph == 0);
}

void
TestDeterminism()
{
    const pmx::Document doc = SampleDocument();
    assert(Canonicalize(doc).value() == Canonicalize(doc).value());
}

} // namespace

void
TestCanonicalize()
{
    TestMetadata();
    TestMesh();
    TestSkinning();
    TestSkeleton();
    TestWeights();
    TestMaterials();
    TestNames();
    TestWithoutBones();
    TestMorphs();
    TestMorphPanel();
    TestMorphCycles();
    TestDeterminism();
}
