// SPDX-License-Identifier: Apache-2.0
//
// BuildMotion: tracks per name bytes, keys sorted, the last record of a frame
// kept, and the interpolation bytes read as MOTION_CONTRACT.md §6 says.
#include "motionVmd/Codes.h"
#include "motionVmd/Motion.h"

#include "VmdEncoder.h"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using namespace motionVmd;
using namespace vmdtest;

std::vector<std::string>
Codes(const Result<Motion>& result)
{
    std::vector<std::string> out;
    for (const Diagnostic& d : result.diagnostics()) {
        out.push_back(d.code);
    }
    return out;
}

void
TestSample()
{
    const Document doc = SampleDocument();
    const auto result = BuildMotion(doc);
    assert(result.ok());
    assert(result.diagnostics().empty());
    const Motion& motion = result.value();
    assert(motion.header == doc.header);

    // センター first: it appears first in the file, although 右足ＩＫ's key is
    // not the latest.
    assert(motion.bones.size() == 2);
    assert(motion.bones[0].bone.bytes == kCenter);
    assert(motion.bones[1].bone.bytes == kRightLegIk);
    const std::vector<BoneKey>& center = motion.bones[0].keys;
    assert(center.size() == 2);
    assert(center[0].frame == 0 && center[1].frame == 60);
    assert(center[0].translation == doc.boneKeyframes[2].translation);
    assert(center[0].rotation == doc.boneKeyframes[2].rotation);
    assert(center[0].curves == BoneCurves(doc.boneKeyframes[2].interpolation));

    assert(motion.morphs.size() == 1);
    assert(motion.morphs[0].keys.size() == 2);
    assert(motion.morphs[0].keys[0].frame == 0 && motion.morphs[0].keys[0].weight == 0.0f);
    assert(motion.morphs[0].keys[1].frame == 10 && motion.morphs[0].keys[1].weight == 0.75f);

    assert(motion.camera.size() == 1);
    assert(motion.camera[0].orthographic);
    assert(motion.camera[0].viewAngle == 30);
    assert(motion.light.size() == 1 && motion.selfShadow.size() == 1);

    assert(motion.visibility.size() == 2);
    assert(motion.visibility[0].visible && !motion.visibility[1].visible);
    assert(motion.ik.size() == 1);
    assert(motion.ik[0].bone.text == kRightLegIkText);
    assert(motion.ik[0].keys.size() == 2);
    assert(motion.ik[0].keys[0].enabled && !motion.ik[0].keys[1].enabled);
}

void
TestCurves()
{
    std::array<std::uint8_t, 64> bone{};
    for (std::size_t i = 0; i < bone.size(); ++i) {
        bone[i] = static_cast<std::uint8_t>(i);
    }
    const auto curves = BoneCurves(bone);
    // Byte i holds i, so each x1 names the byte it came from: Z's and
    // rotation's from row 1.
    assert((curves[kX] == Bezier{0, 4, 8, 12}));
    assert((curves[kY] == Bezier{1, 5, 9, 13}));
    assert((curves[kZ] == Bezier{17, 6, 10, 14}));
    assert((curves[kRotation] == Bezier{18, 7, 11, 15}));

    // As MMD writes them: rows shifted by one byte, and row 0's b[2] and b[3]
    // overwritten with the physics toggle (99, 15). The curves are unaffected.
    const std::array<std::uint8_t, 16> row{
        10, 11, 12, 13, 20, 21, 22, 23, 30, 31, 32, 33, 40, 41, 42, 43};
    std::array<std::uint8_t, 64> written{};
    for (std::size_t r = 0; r < 4; ++r) {
        for (std::size_t i = r; i < 16; ++i) {
            written[16 * r + i - r] = row[i];
        }
    }
    const auto expected = BoneCurves(written);
    written[2] = 99;
    written[3] = 15;
    assert(BoneCurves(written) == expected);
    assert((expected[kZ] == Bezier{12, 22, 32, 42}));
    assert((expected[kRotation] == Bezier{13, 23, 33, 43}));

    std::array<std::uint8_t, 24> camera{};
    for (std::size_t i = 0; i < camera.size(); ++i) {
        camera[i] = static_cast<std::uint8_t>(i);
    }
    const auto cameraCurves = CameraCurves(camera);
    // x1, x2, y1, y2 per channel in the bytes.
    assert((cameraCurves[kCameraX] == Bezier{0, 2, 1, 3}));
    assert((cameraCurves[kCameraRotation] == Bezier{12, 14, 13, 15}));
    assert((cameraCurves[kViewAngle] == Bezier{20, 22, 21, 23}));
}

void
TestDuplicates()
{
    Document doc;
    BoneKeyframe k;
    k.bone = MakeName(kCenter, kCenterText);
    for (float x : {1.0f, 2.0f, 3.0f}) {
        k.frame = 10;
        k.translation = {x, 0.0f, 0.0f};
        doc.boneKeyframes.push_back(k);
    }
    k.frame = 5;
    doc.boneKeyframes.push_back(k);
    // Another track at the same frame is not a duplicate.
    k.bone = MakeName("other", "other");
    k.frame = 10;
    doc.boneKeyframes.push_back(k);

    IkKeyframe ik;
    ik.frame = 3;
    ik.ik.push_back(IkState{MakeName("a", "a"), 0});
    ik.ik.push_back(IkState{MakeName("a", "a"), 1});
    doc.ikKeyframes.push_back(ik);

    const auto result = BuildMotion(doc);
    assert(result.ok());
    const Motion& motion = result.value();
    assert(motion.bones.size() == 2);
    assert(motion.bones[0].keys.size() == 2);
    assert(motion.bones[0].keys[1].frame == 10);
    assert(motion.bones[0].keys[1].translation[0] == 3.0f);
    assert(motion.bones[1].keys.size() == 1);
    assert(motion.ik[0].keys.size() == 1 && motion.ik[0].keys[0].enabled);

    assert((Codes(result) == std::vector<std::string>{"MMD_MOTION_DUPLICATE_KEYFRAME",
                                                      "MMD_MOTION_DUPLICATE_KEYFRAME"}));
    assert(result.diagnostics()[0].location.section == "boneKeyframes");
    assert(result.diagnostics()[0].message.find("2 keyframes") != std::string::npos);
    assert(result.diagnostics()[1].location.section == "ikKeyframes");
}

void
TestNamesByBytes()
{
    // Two fields that decode to the same (empty) text are different tracks.
    Document doc;
    MorphKeyframe k;
    k.morph = MakeName(Cp932({0x85, 0x40}), "");
    doc.morphKeyframes.push_back(k);
    k.morph = MakeName(Cp932({0x85, 0x41}), "");
    doc.morphKeyframes.push_back(k);
    const auto result = BuildMotion(doc);
    assert(result.value().morphs.size() == 2);
    assert(result.diagnostics().empty());
}

} // namespace

void
TestMotion()
{
    TestSample();
    TestCurves();
    TestDuplicates();
    TestNamesByBytes();
}
