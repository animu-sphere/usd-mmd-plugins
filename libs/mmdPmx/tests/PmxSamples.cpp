// SPDX-License-Identifier: Apache-2.0
#include "PmxEncoder.h"

#include <string>

namespace pmxtest {

using namespace mmd::pmx;

Document
SampleDocument(Version version, TextEncoding encoding, std::uint8_t indexWidth,
               std::uint8_t additionalVec4)
{
    const bool v21 = version == Version::V2_1;
    Document doc = EmptyDocument(version, encoding);
    SetIndexWidths(doc, indexWidth);
    doc.header.globals.additionalVec4Count = additionalVec4;
    doc.model = {"サンプル", "Sample", "テスト用のモデル\n二行目", "A model for tests\r\nline 2"};

    // Vertices: one per deform type the version has.
    const auto vertex = [&](float x, Deform deform) {
        Vertex v;
        v.position = {x, 1.0f, -2.0f};
        v.normal = {0.0f, 0.0f, -1.0f};
        v.uv = {x / 4.0f, 0.5f};
        for (std::size_t k = 0; k < additionalVec4; ++k) {
            v.additionalVec4[k] = {x, float(k), 0.25f, 1.0f};
        }
        v.deform = deform;
        v.edgeScale = 1.0f;
        doc.vertices.push_back(v);
    };
    Deform bdef1;
    bdef1.bones[0] = 0;
    vertex(0.0f, bdef1);
    Deform bdef2;
    bdef2.type = DeformType::Bdef2;
    bdef2.bones = {1, 2, kNoIndex, kNoIndex};
    bdef2.weights[0] = 0.75f;
    vertex(1.0f, bdef2);
    Deform bdef4;
    bdef4.type = DeformType::Bdef4;
    bdef4.bones = {0, 1, 2, kNoIndex}; // a none with weight 0 is legal
    bdef4.weights = {0.5f, 0.25f, 0.25f, 0.0f};
    vertex(2.0f, bdef4);
    Deform sdef;
    sdef.type = DeformType::Sdef;
    sdef.bones = {1, 2, kNoIndex, kNoIndex};
    sdef.weights[0] = 0.5f;
    sdef.sdefC = {0.0f, 1.0f, 0.0f};
    sdef.sdefR0 = {0.0f, 1.5f, 0.0f};
    sdef.sdefR1 = {0.0f, 0.5f, 0.0f};
    vertex(3.0f, sdef);
    if (v21) {
        Deform qdef = bdef4;
        qdef.type = DeformType::Qdef;
        vertex(4.0f, qdef);
    } else {
        vertex(4.0f, bdef1);
    }

    // Two triangles, one per material.
    doc.faces = {0, 1, 2, 2, 3, 4};

    doc.textures = {"tex\\髪.png", "sph/肌.spa", "toon\\toon_顔.bmp"};

    Material hair;
    hair.name = "髪";
    hair.englishName = "hair";
    hair.diffuse = {1.0f, 0.9f, 0.8f, 1.0f};
    hair.specular = {0.1f, 0.1f, 0.1f};
    hair.specularPower = 5.0f;
    hair.ambient = {0.5f, 0.45f, 0.4f};
    hair.flags =
        MaterialFlag::NoCulling | MaterialFlag::DrawsEdge | (v21 ? MaterialFlag::VertexColor : 0);
    hair.edgeColor = {0.0f, 0.0f, 0.0f, 1.0f};
    hair.edgeSize = 1.0f;
    hair.texture = 0;
    hair.sphereTexture = 1;
    hair.sphereMode = 1;
    hair.toonReference = ToonReference::Texture;
    hair.toonTexture = 2;
    hair.memo = "メモ";
    hair.faceCount = 3;
    doc.materials.push_back(hair);

    Material skin;
    skin.name = "肌";
    skin.diffuse = {1.0f, 0.8f, 0.7f, 0.5f};
    skin.flags = MaterialFlag::GroundShadow | MaterialFlag::CastsSelfShadow |
                 MaterialFlag::ReceivesSelfShadow;
    skin.texture = kNoIndex;
    skin.sphereTexture = kNoIndex;
    skin.toonReference = ToonReference::Shared;
    skin.sharedToon = 3;
    skin.faceCount = 3;
    doc.materials.push_back(skin);

    // Bones: a root, an arm chain with an IK bone, an append bone, and one
    // that uses every remaining conditional field.
    Bone center;
    center.name = "センター";
    center.position = {0.0f, 8.0f, 0.0f};
    center.flags =
        BoneFlag::Rotatable | BoneFlag::Translatable | BoneFlag::Visible | BoneFlag::Operable;
    center.tailOffset = {0.0f, -1.0f, 0.0f};
    doc.bones.push_back(center);

    Bone arm;
    arm.name = "左腕";
    arm.englishName = "LeftArm";
    arm.position = {1.0f, 12.0f, 0.0f};
    arm.parent = 0;
    arm.flags = BoneFlag::TailIsBone | BoneFlag::Rotatable | BoneFlag::Visible;
    arm.tailBone = 2; // a forward reference is legal
    doc.bones.push_back(arm);

    Bone elbow;
    elbow.name = "左ひじ";
    elbow.englishName = "LeftElbow";
    elbow.position = {3.0f, 12.0f, 0.0f};
    elbow.parent = 1;
    elbow.transformLayer = 1;
    elbow.flags = BoneFlag::Rotatable | BoneFlag::AppendRotation | BoneFlag::FixedAxis |
                  BoneFlag::LocalAxes | BoneFlag::DeformAfterPhysics | BoneFlag::ExternalParent;
    elbow.appendParent = 1;
    elbow.appendRatio = 0.5f;
    elbow.fixedAxis = {1.0f, 0.0f, 0.0f};
    elbow.localAxisX = {1.0f, 0.0f, 0.0f};
    elbow.localAxisZ = {0.0f, 0.0f, 1.0f};
    elbow.externalParentKey = 7;
    doc.bones.push_back(elbow);

    Bone ik;
    ik.name = "左腕ＩＫ";
    ik.englishName = "LeftArm IK";
    ik.position = {5.0f, 12.0f, 0.0f};
    ik.flags = BoneFlag::TailIsBone | BoneFlag::Rotatable | BoneFlag::Translatable | BoneFlag::Ik |
               BoneFlag::AppendTranslation | BoneFlag::LocalAppend;
    ik.tailBone = kNoIndex;
    ik.appendParent = kNoIndex;
    ik.appendRatio = 1.0f;
    ik.ik.target = 2;
    ik.ik.loopCount = 40;
    ik.ik.limitAngle = 0.5f;
    IkLink limited;
    limited.bone = 2;
    limited.hasLimits = true;
    limited.lowerLimit = {-3.14f, 0.0f, 0.0f};
    limited.upperLimit = {-0.01f, 0.0f, 0.0f};
    IkLink free;
    free.bone = 1;
    ik.ik.links = {limited, free};
    doc.bones.push_back(ik);

    // Morphs: every type the version has.
    const auto morph =
        [&](std::string name, std::string english, std::uint8_t panel, MorphType type) -> Morph& {
        Morph m;
        m.name = std::move(name);
        m.englishName = std::move(english);
        m.panel = panel;
        m.type = type;
        doc.morphs.push_back(std::move(m));
        return doc.morphs.back();
    };
    morph("まばたき", "Blink", 2, MorphType::Vertex).vertexOffsets = {{0, {0.0f, -0.1f, 0.0f}},
                                                                      {3, {0.0f, -0.2f, 0.01f}}};
    morph("笑い", "smile 2", 3, MorphType::Group).groupOffsets = {{0, 0.5f}, {3, 1.0f}};
    morph("腕上げ", "", 4, MorphType::Bone).boneOffsets = {
        {1, {0.0f, 0.5f, 0.0f}, {0.0f, 0.0f, 0.38268343f, 0.9238795f}}};
    morph("UV", "uv", 4, MorphType::Uv).uvOffsets = {{1, {0.1f, -0.1f, 0.0f, 0.0f}}};
    morph("追加UV1", "", 0, MorphType::AdditionalUv1).uvOffsets = {{2, {0.0f, 0.0f, 1.0f, 1.0f}}};
    morph("追加UV4", "", 0, MorphType::AdditionalUv4).uvOffsets = {};
    {
        MaterialOffset all;
        all.material = kNoIndex; // every material
        all.operation = 0;
        all.diffuse = {1.0f, 1.0f, 1.0f, 0.0f};
        all.textureTint = {1.0f, 1.0f, 1.0f, 1.0f};
        MaterialOffset one;
        one.material = 1;
        one.operation = 1;
        one.edgeSize = 0.5f;
        one.toonTint = {0.1f, 0.0f, 0.0f, 0.0f};
        morph("材質", "material", 4, MorphType::Material).materialOffsets = {all, one};
    }
    if (v21) {
        morph("反転", "flip", 4, MorphType::Flip).groupOffsets = {{0, 1.0f}};
        morph("衝撃", "impulse", 4, MorphType::Impulse).impulseOffsets = {
            {0, 1, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.5f}}};
    }

    DisplayFrame root;
    root.name = "Root";
    root.englishName = "Root";
    root.special = 1;
    root.elements = {{FrameElementKind::Bone, 0}};
    DisplayFrame face;
    face.name = "表情";
    face.englishName = "Exp";
    face.special = 1;
    face.elements = {{FrameElementKind::Morph, 0}, {FrameElementKind::Morph, 1}};
    DisplayFrame arms;
    arms.name = "腕";
    arms.elements = {
        {FrameElementKind::Bone, 1}, {FrameElementKind::Bone, 2}, {FrameElementKind::Bone, 3}};
    doc.displayFrames = {root, face, arms};

    RigidBody head;
    head.name = "頭";
    head.englishName = "head";
    head.bone = 0;
    head.group = 1;
    head.nonCollisionMask = 0xFFFE;
    head.shape = 0;
    head.size = {1.0f, 0.0f, 0.0f};
    head.position = {0.0f, 15.0f, 0.0f};
    head.mass = 1.0f;
    head.linearDamping = 0.5f;
    head.angularDamping = 0.5f;
    head.friction = 0.5f;
    head.physicsMode = 0;
    RigidBody hair1 = head;
    hair1.name = "髪1";
    hair1.englishName = "hair1";
    hair1.bone = kNoIndex; // unattached
    hair1.shape = 2;
    hair1.size = {0.2f, 1.0f, 0.0f};
    hair1.rotation = {0.1f, 0.2f, 0.3f};
    hair1.physicsMode = 2;
    doc.rigidBodies = {head, hair1};

    Joint joint;
    joint.name = "頭-髪1";
    joint.type = 0;
    joint.rigidBodyA = 0;
    joint.rigidBodyB = 1;
    joint.position = {0.0f, 15.0f, -0.5f};
    joint.rotationMin = {-0.1f, -0.1f, -0.1f};
    joint.rotationMax = {0.1f, 0.1f, 0.1f};
    joint.rotationSpring = {10.0f, 10.0f, 10.0f};
    doc.joints = {joint};
    if (v21) {
        Joint hinge = joint;
        hinge.name = "ヒンジ";
        hinge.type = 5;
        doc.joints.push_back(hinge);

        SoftBody cloth;
        cloth.name = "布";
        cloth.englishName = "cloth";
        cloth.shape = 0;
        cloth.material = 0;
        cloth.group = 2;
        cloth.nonCollisionMask = 0xFFFF;
        cloth.flags = 0x01 | 0x02;
        cloth.bLinkDistance = 2;
        cloth.clusterCount = 4;
        cloth.totalMass = 1.0f;
        cloth.collisionMargin = 0.05f;
        cloth.aeroModel = 1;
        for (std::size_t k = 0; k < cloth.config.size(); ++k) {
            cloth.config[k] = 0.1f * float(k);
        }
        cloth.cluster = {0.1f, 1.0f, 0.5f, 0.5f, 0.5f, 0.5f};
        cloth.iteration = {0, 1, 0, 4};
        cloth.materialCoefficients = {1.0f, 1.0f, 1.0f};
        cloth.anchors = {{0, 0, 1}, {1, 4, 0}};
        cloth.pinVertices = {1, 2};
        doc.softBodies = {cloth};
    }
    return doc;
}

std::string
CheckInvariants(const Document& doc)
{
    const auto inRange = [](std::int32_t index, std::size_t size) {
        return index == kNoIndex || (index >= 0 && static_cast<std::size_t>(index) < size);
    };
    const auto vertex = [&](std::int32_t index) {
        return index >= 0 && static_cast<std::size_t>(index) < doc.vertices.size();
    };
    const std::size_t textures = doc.textures.size();
    const std::size_t materials = doc.materials.size();
    const std::size_t bones = doc.bones.size();
    const std::size_t morphs = doc.morphs.size();
    const std::size_t rigidBodies = doc.rigidBodies.size();

    if (doc.faces.size() % 3 != 0) {
        return "the face count is not a multiple of 3";
    }
    for (std::uint32_t v : doc.faces) {
        if (v >= doc.vertices.size()) {
            return "a face names a missing vertex";
        }
    }
    std::size_t drawn = 0;
    for (const Material& m : doc.materials) {
        if (m.faceCount < 0 || m.faceCount % 3 != 0) {
            return "a material face count is not a whole number of triangles";
        }
        drawn += static_cast<std::size_t>(m.faceCount);
        if (!inRange(m.texture, textures) || !inRange(m.sphereTexture, textures) ||
            !inRange(m.toonTexture, textures)) {
            return "a material texture index is out of range";
        }
    }
    if (drawn > doc.faces.size()) {
        return "the materials draw more face indices than there are";
    }
    for (const Vertex& v : doc.vertices) {
        for (std::int32_t b : v.deform.bones) {
            if (!inRange(b, bones)) {
                return "a deform bone is out of range";
            }
        }
    }
    for (const Bone& b : doc.bones) {
        if (!inRange(b.parent, bones) || !inRange(b.tailBone, bones) ||
            !inRange(b.appendParent, bones) || !inRange(b.ik.target, bones)) {
            return "a bone relation is out of range";
        }
        for (const IkLink& link : b.ik.links) {
            if (!inRange(link.bone, bones)) {
                return "an IK link is out of range";
            }
        }
    }
    for (const Morph& m : doc.morphs) {
        for (const auto& o : m.groupOffsets) {
            if (!inRange(o.morph, morphs)) {
                return "a group or flip member is out of range";
            }
        }
        for (const auto& o : m.vertexOffsets) {
            if (!vertex(o.vertex) && o.vertex != kNoIndex) {
                return "a vertex offset is out of range";
            }
        }
        for (const auto& o : m.boneOffsets) {
            if (!inRange(o.bone, bones)) {
                return "a bone offset is out of range";
            }
        }
        for (const auto& o : m.uvOffsets) {
            if (!vertex(o.vertex) && o.vertex != kNoIndex) {
                return "a UV offset is out of range";
            }
        }
        for (const auto& o : m.materialOffsets) {
            if (!inRange(o.material, materials)) {
                return "a material offset is out of range";
            }
        }
        for (const auto& o : m.impulseOffsets) {
            if (!inRange(o.rigidBody, rigidBodies)) {
                return "an impulse offset is out of range";
            }
        }
    }
    for (const DisplayFrame& f : doc.displayFrames) {
        for (const FrameElement& e : f.elements) {
            if (!inRange(e.index, e.kind == FrameElementKind::Bone ? bones : morphs)) {
                return "a display-frame element is out of range";
            }
        }
    }
    for (const RigidBody& r : doc.rigidBodies) {
        if (!inRange(r.bone, bones)) {
            return "a rigid body's bone is out of range";
        }
    }
    for (const Joint& j : doc.joints) {
        if (!inRange(j.rigidBodyA, rigidBodies) || !inRange(j.rigidBodyB, rigidBodies)) {
            return "a joint's rigid body is out of range";
        }
    }
    for (const SoftBody& s : doc.softBodies) {
        if (!inRange(s.material, materials)) {
            return "a soft body's material is out of range";
        }
        for (const SoftBodyAnchor& a : s.anchors) {
            if (!inRange(a.rigidBody, rigidBodies) || (!vertex(a.vertex) && a.vertex != kNoIndex)) {
                return "a soft-body anchor is out of range";
            }
        }
        for (std::int32_t v : s.pinVertices) {
            if (!vertex(v) && v != kNoIndex) {
                return "a soft-body pin is out of range";
            }
        }
    }
    if (doc.header.version != Version::V2_1 && !doc.softBodies.empty()) {
        return "a PMX 2.0 document has soft bodies";
    }
    return {};
}

} // namespace pmxtest
