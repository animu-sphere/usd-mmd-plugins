// SPDX-License-Identifier: Apache-2.0
//
// A PMX writer for the tests, and only for them: it encodes a pmx::Document
// back to bytes, so a test can state a model as data, read it back, and
// compare -- or write a table by hand, field by field, where the bytes
// themselves are the point. It writes whatever the document holds, invalid
// values included; that is how the malformed-input tests make their input.
//
// Nothing here is a PMX writer the project ships (DESIGN_POLICY.md §2.4).
#pragma once

#include "mmdPmx/Document.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pmxtest {

using Bytes = std::vector<std::byte>;
namespace pmx = mmd::pmx;

class Writer {
public:
    explicit Writer(const pmx::Header& header) : header(header) {}

    const pmx::Header& header;
    Bytes bytes;
    /// Write every string's bytes as they are, whatever the encoding: how a
    /// test puts invalid UTF-16LE into a file.
    bool rawText = false;

    void U8(std::uint32_t v) { bytes.push_back(static_cast<std::byte>(v & 0xFF)); }
    void U16(std::uint32_t v)
    {
        U8(v);
        U8(v >> 8);
    }
    void I32(std::int32_t v)
    {
        const auto u = static_cast<std::uint32_t>(v);
        U16(u);
        U16(u >> 16);
    }
    void F32(float v) { I32(static_cast<std::int32_t>(std::bit_cast<std::uint32_t>(v))); }
    template <std::size_t N> void Floats(const std::array<float, N>& values)
    {
        for (float v : values) {
            F32(v);
        }
    }
    void Raw(const std::string& s)
    {
        for (char c : s) {
            U8(static_cast<unsigned char>(c));
        }
    }

    /// An index at `width` bytes, two's complement: -1 is 0xFF at width 1.
    void Index(std::uint8_t width, std::int32_t v)
    {
        if (width == 1) {
            U8(static_cast<std::uint32_t>(v));
        } else if (width == 2) {
            U16(static_cast<std::uint32_t>(v));
        } else {
            I32(v);
        }
    }
    void Vertex(std::int32_t v) { Index(header.globals.vertexIndexSize, v); }
    void Texture(std::int32_t v) { Index(header.globals.textureIndexSize, v); }
    void Material(std::int32_t v) { Index(header.globals.materialIndexSize, v); }
    void Bone(std::int32_t v) { Index(header.globals.boneIndexSize, v); }
    void Morph(std::int32_t v) { Index(header.globals.morphIndexSize, v); }
    void RigidBody(std::int32_t v) { Index(header.globals.rigidBodyIndexSize, v); }

    /// A text field: the byte length, then the UTF-8 held in `s` encoded as
    /// the header says.
    void Text(const std::string& s)
    {
        if (rawText || header.globals.textEncoding == pmx::TextEncoding::Utf8) {
            I32(static_cast<std::int32_t>(s.size()));
            Raw(s);
            return;
        }
        const std::vector<std::uint16_t> units = Utf16(s);
        I32(static_cast<std::int32_t>(units.size() * 2));
        for (std::uint16_t unit : units) {
            U16(unit);
        }
    }

    static std::vector<std::uint16_t> Utf16(const std::string& utf8)
    {
        std::vector<std::uint16_t> out;
        for (std::size_t i = 0; i < utf8.size();) {
            const auto b = [&](std::size_t k) {
                return static_cast<std::uint32_t>(static_cast<unsigned char>(utf8[i + k]));
            };
            std::uint32_t cp = 0;
            if (b(0) < 0x80) {
                cp = b(0);
                i += 1;
            } else if (b(0) < 0xE0) {
                cp = ((b(0) & 0x1F) << 6) | (b(1) & 0x3F);
                i += 2;
            } else if (b(0) < 0xF0) {
                cp = ((b(0) & 0x0F) << 12) | ((b(1) & 0x3F) << 6) | (b(2) & 0x3F);
                i += 3;
            } else {
                cp = ((b(0) & 0x07) << 18) | ((b(1) & 0x3F) << 12) | ((b(2) & 0x3F) << 6) |
                     (b(3) & 0x3F);
                i += 4;
            }
            if (cp >= 0x10000) {
                cp -= 0x10000;
                out.push_back(static_cast<std::uint16_t>(0xD800 + (cp >> 10)));
                out.push_back(static_cast<std::uint16_t>(0xDC00 + (cp & 0x3FF)));
            } else {
                out.push_back(static_cast<std::uint16_t>(cp));
            }
        }
        return out;
    }
};

// --- one function per section, in file order -----------------------------------

inline void
EncodeHeader(Writer& w, const pmx::Document& doc)
{
    const pmx::Globals& g = doc.header.globals;
    w.Raw("PMX ");
    w.F32(doc.header.version == pmx::Version::V2_1 ? 2.1f : 2.0f);
    w.U8(static_cast<std::uint32_t>(8 + g.unknown.size()));
    for (std::uint32_t v : {static_cast<std::uint32_t>(g.textEncoding),
                            std::uint32_t{g.additionalVec4Count},
                            std::uint32_t{g.vertexIndexSize},
                            std::uint32_t{g.textureIndexSize},
                            std::uint32_t{g.materialIndexSize},
                            std::uint32_t{g.boneIndexSize},
                            std::uint32_t{g.morphIndexSize},
                            std::uint32_t{g.rigidBodyIndexSize}}) {
        w.U8(v);
    }
    for (std::uint8_t v : g.unknown) {
        w.U8(v);
    }
    w.Text(doc.model.name);
    w.Text(doc.model.englishName);
    w.Text(doc.model.comment);
    w.Text(doc.model.englishComment);
}

inline void
EncodeVertices(Writer& w, const pmx::Document& doc)
{
    w.I32(static_cast<std::int32_t>(doc.vertices.size()));
    for (const pmx::Vertex& v : doc.vertices) {
        w.Floats(v.position);
        w.Floats(v.normal);
        w.Floats(v.uv);
        for (std::size_t k = 0; k < doc.header.globals.additionalVec4Count; ++k) {
            w.Floats(v.additionalVec4[k]);
        }
        const pmx::Deform& d = v.deform;
        w.U8(static_cast<std::uint32_t>(d.type));
        switch (d.type) {
        case pmx::DeformType::Bdef1:
            w.Bone(d.bones[0]);
            break;
        case pmx::DeformType::Bdef2:
            w.Bone(d.bones[0]);
            w.Bone(d.bones[1]);
            w.F32(d.weights[0]);
            break;
        case pmx::DeformType::Bdef4:
        case pmx::DeformType::Qdef:
            for (std::int32_t b : d.bones) {
                w.Bone(b);
            }
            w.Floats(d.weights);
            break;
        case pmx::DeformType::Sdef:
            w.Bone(d.bones[0]);
            w.Bone(d.bones[1]);
            w.F32(d.weights[0]);
            w.Floats(d.sdefC);
            w.Floats(d.sdefR0);
            w.Floats(d.sdefR1);
            break;
        default:
            return; // an invalid type: the reader stops here
        }
        w.F32(v.edgeScale);
    }
}

inline void
EncodeFaces(Writer& w, const pmx::Document& doc)
{
    w.I32(static_cast<std::int32_t>(doc.faces.size()));
    for (std::uint32_t v : doc.faces) {
        w.Vertex(static_cast<std::int32_t>(v));
    }
}

inline void
EncodeTextures(Writer& w, const pmx::Document& doc)
{
    w.I32(static_cast<std::int32_t>(doc.textures.size()));
    for (const std::string& path : doc.textures) {
        w.Text(path);
    }
}

inline void
EncodeMaterials(Writer& w, const pmx::Document& doc)
{
    w.I32(static_cast<std::int32_t>(doc.materials.size()));
    for (const pmx::Material& m : doc.materials) {
        w.Text(m.name);
        w.Text(m.englishName);
        w.Floats(m.diffuse);
        w.Floats(m.specular);
        w.F32(m.specularPower);
        w.Floats(m.ambient);
        w.U8(m.flags);
        w.Floats(m.edgeColor);
        w.F32(m.edgeSize);
        w.Texture(m.texture);
        w.Texture(m.sphereTexture);
        w.U8(m.sphereMode);
        w.U8(static_cast<std::uint32_t>(m.toonReference));
        if (m.toonReference == pmx::ToonReference::Texture) {
            w.Texture(m.toonTexture);
        } else if (m.toonReference == pmx::ToonReference::Shared) {
            w.U8(m.sharedToon);
        } else {
            return;
        }
        w.Text(m.memo);
        w.I32(m.faceCount);
    }
}

inline void
EncodeBones(Writer& w, const pmx::Document& doc)
{
    using pmx::BoneFlag;
    w.I32(static_cast<std::int32_t>(doc.bones.size()));
    for (const pmx::Bone& b : doc.bones) {
        w.Text(b.name);
        w.Text(b.englishName);
        w.Floats(b.position);
        w.Bone(b.parent);
        w.I32(b.transformLayer);
        w.U16(b.flags);
        if (b.flags & BoneFlag::TailIsBone) {
            w.Bone(b.tailBone);
        } else {
            w.Floats(b.tailOffset);
        }
        if (b.flags & (BoneFlag::AppendRotation | BoneFlag::AppendTranslation)) {
            w.Bone(b.appendParent);
            w.F32(b.appendRatio);
        }
        if (b.flags & BoneFlag::FixedAxis) {
            w.Floats(b.fixedAxis);
        }
        if (b.flags & BoneFlag::LocalAxes) {
            w.Floats(b.localAxisX);
            w.Floats(b.localAxisZ);
        }
        if (b.flags & BoneFlag::ExternalParent) {
            w.I32(b.externalParentKey);
        }
        if (b.flags & BoneFlag::Ik) {
            w.Bone(b.ik.target);
            w.I32(b.ik.loopCount);
            w.F32(b.ik.limitAngle);
            w.I32(static_cast<std::int32_t>(b.ik.links.size()));
            for (const pmx::IkLink& link : b.ik.links) {
                w.Bone(link.bone);
                w.U8(link.hasLimits ? 1 : 0);
                if (link.hasLimits) {
                    w.Floats(link.lowerLimit);
                    w.Floats(link.upperLimit);
                }
            }
        }
    }
}

inline void
EncodeMorphs(Writer& w, const pmx::Document& doc)
{
    using pmx::MorphType;
    w.I32(static_cast<std::int32_t>(doc.morphs.size()));
    for (const pmx::Morph& m : doc.morphs) {
        w.Text(m.name);
        w.Text(m.englishName);
        w.U8(m.panel);
        w.U8(static_cast<std::uint32_t>(m.type));
        switch (m.type) {
        case MorphType::Group:
        case MorphType::Flip:
            w.I32(static_cast<std::int32_t>(m.groupOffsets.size()));
            for (const auto& o : m.groupOffsets) {
                w.Morph(o.morph);
                w.F32(o.weight);
            }
            break;
        case MorphType::Vertex:
            w.I32(static_cast<std::int32_t>(m.vertexOffsets.size()));
            for (const auto& o : m.vertexOffsets) {
                w.Vertex(o.vertex);
                w.Floats(o.displacement);
            }
            break;
        case MorphType::Bone:
            w.I32(static_cast<std::int32_t>(m.boneOffsets.size()));
            for (const auto& o : m.boneOffsets) {
                w.Bone(o.bone);
                w.Floats(o.translation);
                w.Floats(o.rotation);
            }
            break;
        case MorphType::Uv:
        case MorphType::AdditionalUv1:
        case MorphType::AdditionalUv2:
        case MorphType::AdditionalUv3:
        case MorphType::AdditionalUv4:
            w.I32(static_cast<std::int32_t>(m.uvOffsets.size()));
            for (const auto& o : m.uvOffsets) {
                w.Vertex(o.vertex);
                w.Floats(o.delta);
            }
            break;
        case MorphType::Material:
            w.I32(static_cast<std::int32_t>(m.materialOffsets.size()));
            for (const auto& o : m.materialOffsets) {
                w.Material(o.material);
                w.U8(o.operation);
                w.Floats(o.diffuse);
                w.Floats(o.specular);
                w.F32(o.specularPower);
                w.Floats(o.ambient);
                w.Floats(o.edgeColor);
                w.F32(o.edgeSize);
                w.Floats(o.textureTint);
                w.Floats(o.sphereTint);
                w.Floats(o.toonTint);
            }
            break;
        case MorphType::Impulse:
            w.I32(static_cast<std::int32_t>(m.impulseOffsets.size()));
            for (const auto& o : m.impulseOffsets) {
                w.RigidBody(o.rigidBody);
                w.U8(o.local);
                w.Floats(o.velocity);
                w.Floats(o.torque);
            }
            break;
        default:
            return;
        }
    }
}

inline void
EncodeDisplayFrames(Writer& w, const pmx::Document& doc)
{
    w.I32(static_cast<std::int32_t>(doc.displayFrames.size()));
    for (const pmx::DisplayFrame& f : doc.displayFrames) {
        w.Text(f.name);
        w.Text(f.englishName);
        w.U8(f.special);
        w.I32(static_cast<std::int32_t>(f.elements.size()));
        for (const pmx::FrameElement& e : f.elements) {
            w.U8(static_cast<std::uint32_t>(e.kind));
            if (e.kind == pmx::FrameElementKind::Bone) {
                w.Bone(e.index);
            } else if (e.kind == pmx::FrameElementKind::Morph) {
                w.Morph(e.index);
            } else {
                return;
            }
        }
    }
}

inline void
EncodeRigidBodies(Writer& w, const pmx::Document& doc)
{
    w.I32(static_cast<std::int32_t>(doc.rigidBodies.size()));
    for (const pmx::RigidBody& r : doc.rigidBodies) {
        w.Text(r.name);
        w.Text(r.englishName);
        w.Bone(r.bone);
        w.U8(r.group);
        w.U16(r.nonCollisionMask);
        w.U8(r.shape);
        w.Floats(r.size);
        w.Floats(r.position);
        w.Floats(r.rotation);
        for (float v : {r.mass, r.linearDamping, r.angularDamping, r.restitution, r.friction}) {
            w.F32(v);
        }
        w.U8(r.physicsMode);
    }
}

inline void
EncodeJoints(Writer& w, const pmx::Document& doc)
{
    w.I32(static_cast<std::int32_t>(doc.joints.size()));
    for (const pmx::Joint& j : doc.joints) {
        w.Text(j.name);
        w.Text(j.englishName);
        w.U8(j.type);
        w.RigidBody(j.rigidBodyA);
        w.RigidBody(j.rigidBodyB);
        for (const pmx::Vec3& v : {j.position,
                                   j.rotation,
                                   j.translationMin,
                                   j.translationMax,
                                   j.rotationMin,
                                   j.rotationMax,
                                   j.translationSpring,
                                   j.rotationSpring}) {
            w.Floats(v);
        }
    }
}

inline void
EncodeSoftBodies(Writer& w, const pmx::Document& doc)
{
    w.I32(static_cast<std::int32_t>(doc.softBodies.size()));
    for (const pmx::SoftBody& s : doc.softBodies) {
        w.Text(s.name);
        w.Text(s.englishName);
        w.U8(s.shape);
        w.Material(s.material);
        w.U8(s.group);
        w.U16(s.nonCollisionMask);
        w.U8(s.flags);
        w.I32(s.bLinkDistance);
        w.I32(s.clusterCount);
        w.F32(s.totalMass);
        w.F32(s.collisionMargin);
        w.I32(s.aeroModel);
        w.Floats(s.config);
        w.Floats(s.cluster);
        for (std::int32_t v : s.iteration) {
            w.I32(v);
        }
        w.Floats(s.materialCoefficients);
        w.I32(static_cast<std::int32_t>(s.anchors.size()));
        for (const pmx::SoftBodyAnchor& a : s.anchors) {
            w.RigidBody(a.rigidBody);
            w.Vertex(a.vertex);
            w.U8(a.nearMode);
        }
        w.I32(static_cast<std::int32_t>(s.pinVertices.size()));
        for (std::int32_t v : s.pinVertices) {
            w.Vertex(v);
        }
    }
}

/// The whole file. Soft bodies are written for PMX 2.1 only, as the reader
/// expects them.
inline Bytes
Encode(const pmx::Document& doc, bool rawText = false)
{
    Writer w(doc.header);
    w.rawText = rawText;
    EncodeHeader(w, doc);
    EncodeVertices(w, doc);
    EncodeFaces(w, doc);
    EncodeTextures(w, doc);
    EncodeMaterials(w, doc);
    EncodeBones(w, doc);
    EncodeMorphs(w, doc);
    EncodeDisplayFrames(w, doc);
    EncodeRigidBodies(w, doc);
    EncodeJoints(w, doc);
    if (doc.header.version == pmx::Version::V2_1) {
        EncodeSoftBodies(w, doc);
    }
    return std::move(w.bytes);
}

/// An empty model: every table empty, index width 1, UTF-16LE.
inline pmx::Document
EmptyDocument(pmx::Version version = pmx::Version::V2_0,
              pmx::TextEncoding encoding = pmx::TextEncoding::Utf16Le)
{
    pmx::Document doc;
    doc.header.version = version;
    doc.header.globals.textEncoding = encoding;
    doc.model.name = "empty";
    return doc;
}

/// Every index width set to `width`.
inline void
SetIndexWidths(pmx::Document& doc, std::uint8_t width)
{
    pmx::Globals& g = doc.header.globals;
    g.vertexIndexSize = g.textureIndexSize = g.materialIndexSize = g.boneIndexSize =
        g.morphIndexSize = g.rigidBodyIndexSize = width;
}

/// A small model that uses every table and every record variant its version
/// allows: each deform type, both toon references, every conditional bone
/// field, every morph type, both display-frame element kinds, and (2.1) a
/// soft body. Japanese names throughout, as in the ordinary case.
pmx::Document SampleDocument(pmx::Version version, pmx::TextEncoding encoding,
                             std::uint8_t indexWidth, std::uint8_t additionalVec4 = 2);

/// Every invariant a successfully read document promises (Document.h):
/// faces name vertices, materials' face counts fit the face table, and every
/// other index names an element of its table or is kNoIndex. Returns the
/// first violation, or an empty string.
std::string CheckInvariants(const pmx::Document& doc);

} // namespace pmxtest
