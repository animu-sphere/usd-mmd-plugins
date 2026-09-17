// SPDX-License-Identifier: Apache-2.0
//
// Bind over a model canonicalized from a PMX document stated as data, and a
// motion built from a VMD document stated as data: which names match by
// MMD's byte rule, what a mismatch and an ambiguity report, and what the
// basis conversion does to a key.
#include "mmdMotionBinding/Bind.h"

#include <mmdModel/Basis.h>
#include <mmdModel/Canonicalize.h>
#include <motionVmd/Codes.h>
#include <motionVmd/Cp932.h>
#include <motionVmd/Motion.h>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using namespace mmd;
namespace pmx = mmd::pmx;
namespace vmd = motionVmd;

// 16 CP932 bytes each, sharing their first 15 (先 and 線 share a lead byte): in a 15-byte VMD bone field
// they are one name, in a 20-byte IK field two.
const std::string kLongFront = "右腕捩りボーン先";
const std::string kLongBack = "右腕捩りボーン線";

pmx::Bone
MakeBone(const std::string& name, std::int32_t parent)
{
    pmx::Bone b;
    b.name = name;
    b.position = {0.0f, 1.0f, 0.0f};
    b.parent = parent;
    return b;
}

pmx::Morph
MakeMorph(const std::string& name)
{
    pmx::Morph m;
    m.name = name;
    m.panel = 1;
    m.type = pmx::MorphType::Group;
    return m;
}

CanonicalDocument
Model()
{
    pmx::Document doc;
    doc.bones = {
        MakeBone("左ひじ", 3), // a child before its parent: joint order differs
        MakeBone(kLongBack, pmx::kNoIndex),
        MakeBone("センター", pmx::kNoIndex),
        MakeBone("左腕", 2),
        MakeBone(kLongFront, pmx::kNoIndex),
        MakeBone("é", pmx::kNoIndex), // not CP932
        MakeBone("", pmx::kNoIndex),  // matches nothing, not even an empty VMD name
        MakeBone("右足ＩＫ", 2),
    };
    doc.morphs = {MakeMorph("まばたき"), MakeMorph("あ"), MakeMorph("😀")};
    auto result = Canonicalize(doc);
    assert(result.ok());
    return std::move(result).value();
}

std::int32_t
JointOf(const CanonicalDocument& model, std::size_t sourceBone)
{
    return model.skeleton.jointOfSourceBone[sourceBone];
}

/// A VMD name field holding `utf8` cut to `width` bytes, as MMD writes it.
vmd::Name
VmdName(const std::string& utf8, std::size_t width)
{
    std::string bytes = vmd::EncodeCp932(utf8).value();
    if (bytes.size() > width) {
        bytes.resize(width);
    }
    const auto decoded = vmd::DecodeCp932(
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()));
    return vmd::Name{bytes, decoded.text};
}

vmd::Motion
Motion()
{
    vmd::Document doc;
    const auto bone = [&](const std::string& name, std::uint32_t frame) {
        vmd::BoneKeyframe k;
        k.bone = VmdName(name, 15);
        k.frame = frame;
        k.translation = {1.0f, 2.0f, 3.0f};
        k.rotation = {0.1f, 0.2f, 0.3f, 0.9273618f};
        k.interpolation.fill(20);
        doc.boneKeyframes.push_back(k);
    };
    bone("センター", 10);
    bone("センター", 0);
    bone(kLongFront, 0);
    bone("存在しない", 0);
    bone("左ひじ", 5);
    doc.morphKeyframes.push_back(vmd::MorphKeyframe{VmdName("まばたき", 15), 3, 1.0f});
    doc.morphKeyframes.push_back(vmd::MorphKeyframe{VmdName("い", 15), 3, 1.0f});
    vmd::IkKeyframe ik;
    ik.visible = 1;
    ik.ik.push_back(vmd::IkState{VmdName("右足ＩＫ", 20), 1});
    ik.ik.push_back(vmd::IkState{VmdName(kLongFront, 20), 0});
    ik.ik.push_back(vmd::IkState{VmdName("左足ＩＫ", 20), 1});
    doc.ikKeyframes.push_back(ik);
    doc.sectionsPresent = vmd::kSectionCount;
    auto built = vmd::BuildMotion(doc);
    assert(built.ok() && built.diagnostics().empty());
    return std::move(built).value();
}

std::vector<std::string>
Codes(const Result<binding::BoundMotion>& result)
{
    std::vector<std::string> out;
    for (const Diagnostic& d : result.diagnostics()) {
        out.push_back(d.code);
    }
    return out;
}

void
TestFieldBytes()
{
    assert(binding::FieldBytes("センター", 15) == vmd::EncodeCp932("センター"));
    assert(binding::FieldBytes(kLongFront, 15)->size() == 15);
    assert(binding::FieldBytes(kLongFront, 20)->size() == 16);
    assert(binding::FieldBytes(kLongFront, 15) == binding::FieldBytes(kLongBack, 15));
    assert(binding::FieldBytes(kLongFront, 20) != binding::FieldBytes(kLongBack, 20));
    assert(binding::FieldBytes(std::string("ab\0cd", 5), 15) == std::string("ab"));
    assert(!binding::FieldBytes("é", 15));
}

void
TestBind()
{
    const CanonicalDocument model = Model();
    const vmd::Motion motion = Motion();
    const auto result = binding::Bind(motion, model);
    assert(result.ok());

    const std::vector<std::string> expected{
        "MMD_MOTION_UNENCODABLE_NAME", // bones: é
        "MMD_MOTION_UNENCODABLE_NAME", // morphs: 😀
        "MMD_MOTION_AMBIGUOUS_NAME",   // 右腕捩りボーン先, cut to 15 bytes
        "MMD_MOTION_UNMATCHED_BONE",   // 存在しない
        "MMD_MOTION_UNMATCHED_MORPH",  // い
        "MMD_MOTION_UNMATCHED_BONE",   // 左足ＩＫ
    };
    if (Codes(result) != expected) {
        for (const Diagnostic& d : result.diagnostics()) {
            std::fprintf(stderr, "%s\n", FormatDiagnostic(d).c_str());
        }
    }
    assert(Codes(result) == expected);
    assert(result.diagnostics()[0].location.table == "bones");
    assert(result.diagnostics()[2].severity == Severity::Warning);
    assert(result.diagnostics()[2].location.table == "boneKeyframes");
    assert(result.diagnostics()[5].location.table == "ikKeyframes");

    const binding::BoundMotion& bound = result.value();
    // The ambiguous name binds the lower source index, 右腕捩りボーン線.
    std::vector<std::int32_t> joints;
    for (const binding::BoneTrack& t : bound.bones) {
        joints.push_back(t.joint);
    }
    std::vector<std::int32_t> want{JointOf(model, 0), JointOf(model, 1), JointOf(model, 2)};
    std::sort(want.begin(), want.end());
    assert(joints == want);

    const binding::BoneTrack& center =
        *std::find_if(bound.bones.begin(), bound.bones.end(), [&](const binding::BoneTrack& t) {
            return t.joint == JointOf(model, 2);
        });
    assert(center.keys.size() == 2);
    assert(center.keys[0].frame == 0 && center.keys[1].frame == 10);
    assert(center.keys[0].translation == basis::Displacement({1.0f, 2.0f, 3.0f}));
    assert(center.keys[0].rotation == basis::Quaternion({0.1f, 0.2f, 0.3f, 0.9273618f}));
    assert(center.keys[0].translation[2] < 0.0f);
    assert(center.keys[0].curves == motion.bones[0].keys[0].curves);

    assert(bound.morphs.size() == 1);
    assert(bound.morphs[0].morph == 0);
    assert(bound.morphs[0].keys == motion.morphs[0].keys);

    // In the 20-byte IK field the long name is whole, and names only itself.
    assert(bound.ik.size() == 2);
    std::vector<std::int32_t> ikJoints{bound.ik[0].joint, bound.ik[1].joint};
    std::vector<std::int32_t> wantIk{JointOf(model, 4), JointOf(model, 7)};
    std::sort(wantIk.begin(), wantIk.end());
    assert(ikJoints == wantIk);
    assert(bound.visibility == motion.visibility);
}

void
TestNothingToBind()
{
    const auto result = binding::Bind(vmd::Motion{}, CanonicalDocument{});
    assert(result.ok());
    assert(result.diagnostics().empty());
    assert(result.value() == binding::BoundMotion{});

    // A motion for another model entirely: every name reported, bounded.
    vmd::Document doc;
    for (int i = 0; i < 40; ++i) {
        vmd::BoneKeyframe k;
        k.bone = VmdName("bone" + std::to_string(i), 15);
        doc.boneKeyframes.push_back(k);
    }
    const auto other = binding::Bind(vmd::BuildMotion(doc).value(), Model());
    assert(other.value().bones.empty());
    const auto& diagnostics = other.diagnostics();
    assert(diagnostics.back().message == "24 more in boneKeyframes are not listed");
}

void
TestToDiagnostic()
{
    vmd::Location where;
    where.byteOffset = 1234;
    where.section = "morphKeyframes";
    where.index = 3;
    where.field = "morph";
    const Diagnostic d = binding::ToDiagnostic(
        vmd::MakeDiagnostic(vmd::codes::TextInvalidCp932, "bad bytes", where));
    assert(d.code == "MMD_TEXT_INVALID_CP932");
    assert(d.severity == Severity::Error && d.recoverable);
    assert(d.message == "bad bytes");
    assert(FormatDiagnostic(d) ==
           "MMD_TEXT_INVALID_CP932: bad bytes (morphKeyframes[3].morph at byte 1234)");
    const Diagnostic fatal =
        binding::ToDiagnostic(vmd::MakeDiagnostic(vmd::codes::MotionBadSignature, "no"));
    assert(fatal.severity == Severity::Fatal && !fatal.recoverable);
}

} // namespace

int
main()
{
    TestFieldBytes();
    TestBind();
    TestNothingToBind();
    TestToDiagnostic();
    std::puts("mmdMotionBinding unit tests passed");
    return 0;
}
