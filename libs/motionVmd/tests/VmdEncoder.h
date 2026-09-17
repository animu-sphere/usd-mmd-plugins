// SPDX-License-Identifier: Apache-2.0
//
// Writes VMD bytes from a motionVmd::Document, so each test states the
// motion it means as data and reads back what it wrote. Names are written
// from Name::bytes, NUL-padded to their field -- or cut to it, exactly as MMD
// cuts a name that is too long.
#pragma once

#include <motionVmd/Document.h>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace vmdtest {

using Bytes = std::vector<std::byte>;

class Writer {
public:
    void U8(std::uint8_t v) { out.push_back(static_cast<std::byte>(v)); }
    void U32(std::uint32_t v)
    {
        for (int i = 0; i < 4; ++i) {
            U8(static_cast<std::uint8_t>(v >> (8 * i)));
        }
    }
    void F32(float v) { U32(std::bit_cast<std::uint32_t>(v)); }
    template <class Array> void Floats(const Array& values)
    {
        for (float v : values) {
            F32(v);
        }
    }
    template <class Array> void Raw(const Array& values)
    {
        for (std::uint8_t v : values) {
            U8(v);
        }
    }
    /// `text` in a `width`-byte field: cut, or padded with `pad`.
    void Field(std::string_view text, std::size_t width, std::uint8_t pad = 0)
    {
        for (std::size_t i = 0; i < width; ++i) {
            U8(i < text.size() ? static_cast<std::uint8_t>(text[i]) : pad);
        }
    }

    Bytes out;
};

/// The whole document; `sections` of the six sections are written, so a
/// shorter value writes an older file that ends early.
inline Bytes
Encode(const motionVmd::Document& doc, std::size_t sections = motionVmd::kSectionCount)
{
    using namespace motionVmd;
    Writer w;
    const bool v1 = doc.header.version == Version::V1;
    w.Field(ToString(doc.header.version), 30);
    w.Field(doc.header.modelName.bytes, v1 ? 10 : 20);

    if (sections > 0) {
        w.U32(static_cast<std::uint32_t>(doc.boneKeyframes.size()));
        for (const BoneKeyframe& k : doc.boneKeyframes) {
            w.Field(k.bone.bytes, 15);
            w.U32(k.frame);
            w.Floats(k.translation);
            w.Floats(k.rotation);
            w.Raw(k.interpolation);
        }
    }
    if (sections > 1) {
        w.U32(static_cast<std::uint32_t>(doc.morphKeyframes.size()));
        for (const MorphKeyframe& k : doc.morphKeyframes) {
            w.Field(k.morph.bytes, 15);
            w.U32(k.frame);
            w.F32(k.weight);
        }
    }
    if (sections > 2) {
        w.U32(static_cast<std::uint32_t>(doc.cameraKeyframes.size()));
        for (const CameraKeyframe& k : doc.cameraKeyframes) {
            w.U32(k.frame);
            w.F32(k.distance);
            w.Floats(k.position);
            w.Floats(k.rotation);
            w.Raw(k.interpolation);
            w.U32(k.viewAngle);
            w.U8(k.orthographic);
        }
    }
    if (sections > 3) {
        w.U32(static_cast<std::uint32_t>(doc.lightKeyframes.size()));
        for (const LightKeyframe& k : doc.lightKeyframes) {
            w.U32(k.frame);
            w.Floats(k.color);
            w.Floats(k.direction);
        }
    }
    if (sections > 4) {
        w.U32(static_cast<std::uint32_t>(doc.selfShadowKeyframes.size()));
        for (const SelfShadowKeyframe& k : doc.selfShadowKeyframes) {
            w.U32(k.frame);
            w.U8(k.mode);
            w.F32(k.distance);
        }
    }
    if (sections > 5) {
        w.U32(static_cast<std::uint32_t>(doc.ikKeyframes.size()));
        for (const IkKeyframe& k : doc.ikKeyframes) {
            w.U32(k.frame);
            w.U8(k.visible);
            w.U32(static_cast<std::uint32_t>(k.ik.size()));
            for (const IkState& s : k.ik) {
                w.Field(s.bone.bytes, 20);
                w.U8(s.enabled);
            }
        }
    }
    return w.out;
}

/// CP932 bytes, from their values.
inline std::string
Cp932(std::initializer_list<unsigned> bytes)
{
    std::string out;
    for (unsigned b : bytes) {
        out += static_cast<char>(b);
    }
    return out;
}

// Names as MMD writes them, with the UTF-8 they decode to.
inline const std::string kCenter = Cp932({0x83, 0x5A, 0x83, 0x93, 0x83, 0x5E, 0x81, 0x5B});
inline const std::string kCenterText = "センター";
inline const std::string kRightLegIk = Cp932({0x89, 0x45, 0x91, 0xAB, 0x82, 0x68, 0x82, 0x6A});
inline const std::string kRightLegIkText = "右足ＩＫ";
inline const std::string kBlink = Cp932({0x82, 0xDC, 0x82, 0xCE, 0x82, 0xBD, 0x82, 0xAB});
inline const std::string kBlinkText = "まばたき";
/// 16 bytes: a 15-byte field cuts its last character in half.
inline const std::string kLongName = Cp932({0x89,
                                            0x45,
                                            0x98,
                                            0x72,
                                            0x9D,
                                            0x80,
                                            0x82,
                                            0xE8,
                                            0x83,
                                            0x7B,
                                            0x81,
                                            0x5B,
                                            0x83,
                                            0x93,
                                            0x90,
                                            0xE6});
inline const std::string kLongNameText = "右腕捩りボーン先";
inline const std::string kLongNameCutText = "右腕捩りボーン";

inline motionVmd::Name
MakeName(const std::string& bytes, const std::string& text)
{
    return motionVmd::Name{bytes, text};
}

/// A motion with a record in every section, Japanese names, and values that
/// differ per record, so a field read from the wrong offset shows.
inline motionVmd::Document
SampleDocument()
{
    using namespace motionVmd;
    Document doc;
    doc.header.version = Version::V2;
    doc.header.modelName = MakeName("Model", "Model");

    for (std::uint32_t i = 0; i < 3; ++i) {
        BoneKeyframe k;
        k.bone = i == 1 ? MakeName(kRightLegIk, kRightLegIkText) : MakeName(kCenter, kCenterText);
        k.frame = 30 * (2 - i);
        k.translation = {1.0f + i, 2.0f, -3.5f};
        k.rotation = {0.0f, 0.5f * i, 0.0f, 1.0f};
        for (std::size_t b = 0; b < k.interpolation.size(); ++b) {
            k.interpolation[b] = static_cast<std::uint8_t>((b * 7 + i) % 128);
        }
        doc.boneKeyframes.push_back(k);
    }
    doc.morphKeyframes.push_back(MorphKeyframe{MakeName(kBlink, kBlinkText), 10, 0.75f});
    doc.morphKeyframes.push_back(MorphKeyframe{MakeName(kBlink, kBlinkText), 0, 0.0f});

    CameraKeyframe camera;
    camera.frame = 5;
    camera.distance = -45.0f;
    camera.position = {0.0f, 10.0f, 0.0f};
    camera.rotation = {0.1f, 0.2f, 0.3f};
    for (std::size_t b = 0; b < camera.interpolation.size(); ++b) {
        camera.interpolation[b] = static_cast<std::uint8_t>(b * 5);
    }
    camera.viewAngle = 30;
    camera.orthographic = 1;
    doc.cameraKeyframes.push_back(camera);

    doc.lightKeyframes.push_back(LightKeyframe{0, {0.6f, 0.6f, 0.6f}, {-0.5f, -1.0f, 0.5f}});
    doc.selfShadowKeyframes.push_back(SelfShadowKeyframe{0, 1, 0.01f});

    IkKeyframe ik;
    ik.frame = 0;
    ik.visible = 1;
    ik.ik.push_back(IkState{MakeName(kRightLegIk, kRightLegIkText), 1});
    doc.ikKeyframes.push_back(ik);
    ik.frame = 60;
    ik.visible = 0;
    ik.ik[0].enabled = 0;
    doc.ikKeyframes.push_back(ik);

    doc.sectionsPresent = kSectionCount;
    return doc;
}

} // namespace vmdtest
