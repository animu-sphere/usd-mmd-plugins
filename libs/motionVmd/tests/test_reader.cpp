// SPDX-License-Identifier: Apache-2.0
//
// The VMD reader against bytes built here (VmdEncoder.h): whole motions read
// back field for field, and single bytes changed where the malformed byte is
// the point.
#include "motionVmd/Codes.h"
#include "motionVmd/Reader.h"

#include "VmdEncoder.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

using namespace motionVmd;
using namespace vmdtest;

Result<Document>
ReadBytes(const Bytes& bytes)
{
    return Read(std::span<const std::byte>(bytes.data(), bytes.size()));
}

std::vector<std::string>
Codes(const Result<Document>& result)
{
    std::vector<std::string> out;
    for (const Diagnostic& d : result.diagnostics()) {
        out.push_back(d.code);
    }
    return out;
}

const Diagnostic&
ExpectFatal(const Result<Document>& result, const Code& code)
{
    assert(!result.ok());
    assert(result.fatal() != nullptr);
    if (result.fatal()->code != code.id) {
        std::fprintf(stderr,
                     "expected %.*s, got %s\n",
                     static_cast<int>(code.id.size()),
                     code.id.data(),
                     FormatDiagnostic(*result.fatal()).c_str());
    }
    assert(result.fatal()->code == code.id);
    assert(!result.fatal()->recoverable);
    return *result.fatal();
}

void
TestRoundTrip()
{
    const Document sample = SampleDocument();
    const Bytes bytes = Encode(sample);
    const auto result = ReadBytes(bytes);
    assert(result.ok());
    assert(result.diagnostics().empty());
    assert(result.value() == sample);

    // The signature is the literal MMD writes, not whatever ToString returns.
    assert(std::memcmp(bytes.data(), "Vocaloid Motion Data 0002\0", 26) == 0);
}

void
TestVersion1()
{
    Document doc = SampleDocument();
    doc.header.version = Version::V1;
    doc.header.modelName = MakeName("Old", "Old");
    const Bytes bytes = Encode(doc);
    assert(std::memcmp(bytes.data(), "Vocaloid Motion Data file\0", 26) == 0);
    // 30 + 10: the first count follows a 10-byte model name.
    assert(bytes[40] == std::byte{3});
    const auto result = ReadBytes(bytes);
    assert(result.ok());
    assert(result.value() == doc);
}

void
TestSignatureJunkAfterNul()
{
    Bytes bytes = Encode(SampleDocument());
    bytes[27] = std::byte{0x4A};
    bytes[29] = std::byte{0xFF};
    assert(ReadBytes(bytes).ok());
}

void
TestBadSignature()
{
    Bytes bytes = Encode(SampleDocument());
    bytes[21] = std::byte{'1'};
    const auto bad = ReadBytes(bytes);
    const Diagnostic& fatal = ExpectFatal(bad, codes::MotionBadSignature);
    assert(fatal.location.byteOffset == 0u);

    ExpectFatal(ReadBytes(Bytes{}), codes::MotionTruncatedBuffer);
    const std::string pmx = "PMX ";
    ExpectFatal(ReadBytes(Bytes(reinterpret_cast<const std::byte*>(pmx.data()),
                                reinterpret_cast<const std::byte*>(pmx.data()) + pmx.size())),
                codes::MotionBadSignature);
}

void
TestEndsAfterAnySection()
{
    const Document sample = SampleDocument();
    for (std::size_t sections = 0; sections <= kSectionCount; ++sections) {
        const auto result = ReadBytes(Encode(sample, sections));
        assert(result.ok());
        assert(result.diagnostics().empty());
        const Document& doc = result.value();
        assert(doc.sectionsPresent == sections);
        assert(doc.boneKeyframes.size() == (sections > 0 ? 3u : 0u));
        assert(doc.morphKeyframes.size() == (sections > 1 ? 2u : 0u));
        assert(doc.cameraKeyframes.size() == (sections > 2 ? 1u : 0u));
        assert(doc.lightKeyframes.size() == (sections > 3 ? 1u : 0u));
        assert(doc.selfShadowKeyframes.size() == (sections > 4 ? 1u : 0u));
        assert(doc.ikKeyframes.size() == (sections > 5 ? 2u : 0u));
    }
}

void
TestTruncation()
{
    const Bytes whole = Encode(SampleDocument());
    const std::size_t v2Header = 50;
    // Every prefix that does not end exactly between two sections is fatal.
    std::vector<std::size_t> boundaries{v2Header};
    for (std::size_t s = 1; s <= kSectionCount; ++s) {
        boundaries.push_back(Encode(SampleDocument(), s).size());
    }
    for (std::size_t n = 30; n < whole.size(); ++n) {
        const auto result = ReadBytes(Bytes(whole.begin(), whole.begin() + n));
        const bool boundary =
            std::find(boundaries.begin(), boundaries.end(), n) != boundaries.end();
        if (boundary) {
            assert(result.ok());
            continue;
        }
        assert(!result.ok());
        const std::string& code = result.fatal()->code;
        assert(code == codes::MotionTruncatedBuffer.id ||
               code == codes::MotionCountExceedsBuffer.id);
    }
}

void
TestCountExceedsBuffer()
{
    Bytes bytes = Encode(SampleDocument(), 1);
    // The bone count: three records are there, a fourth is claimed.
    bytes[50] = std::byte{4};
    const auto exceeds = ReadBytes(bytes);
    const Diagnostic& fatal = ExpectFatal(exceeds, codes::MotionCountExceedsBuffer);
    assert(fatal.location.section == "boneKeyframes");
    assert(fatal.location.field == "count");
    assert(fatal.location.byteOffset == 50u);

    // A huge count is refused before anything is allocated for it.
    bytes[50] = std::byte{0xFF};
    bytes[51] = std::byte{0xFF};
    bytes[52] = std::byte{0xFF};
    bytes[53] = std::byte{0xFF};
    ExpectFatal(ReadBytes(bytes), codes::MotionCountExceedsBuffer);

    // An IK record's own count is bounded the same way.
    Document doc;
    IkKeyframe ik;
    ik.ik.push_back(IkState{MakeName("a", "a"), 1});
    doc.ikKeyframes.push_back(ik);
    Bytes ikBytes = Encode(doc);
    const std::size_t statesCount = ikBytes.size() - 21 - 4;
    ikBytes[statesCount] = std::byte{2};
    const auto ikResult = ReadBytes(ikBytes);
    const Diagnostic& inner = ExpectFatal(ikResult, codes::MotionCountExceedsBuffer);
    assert(inner.location.section == "ikKeyframes");
    assert(inner.location.index == 0u);
}

void
TestTrailingBytes()
{
    Bytes bytes = Encode(SampleDocument());
    const std::size_t end = bytes.size();
    bytes.push_back(std::byte{0});
    bytes.push_back(std::byte{1});
    const auto result = ReadBytes(bytes);
    assert(result.ok());
    assert(Codes(result) == std::vector<std::string>{"MMD_MOTION_TRAILING_BYTES"});
    assert(result.diagnostics()[0].location.byteOffset == end);
}

void
TestNames()
{
    Document doc;
    BoneKeyframe k;
    // Cut inside its last character: the half is dropped, the bytes kept.
    k.bone = MakeName(kLongName, "");
    doc.boneKeyframes.push_back(k);
    // Padding after the NUL is not part of the name, whatever it holds.
    k.bone = MakeName(kCenter, kCenterText);
    doc.boneKeyframes.push_back(k);

    Bytes bytes = Encode(doc);
    const std::size_t second = 50 + 4 + 111;
    bytes[second + 9] = std::byte{0xCC};
    bytes[second + 14] = std::byte{0x81}; // a lead byte after the NUL is padding

    const auto result = ReadBytes(bytes);
    assert(result.ok());
    const Document& read = result.value();
    assert(read.boneKeyframes[0].bone.bytes == kLongName.substr(0, 15));
    assert(read.boneKeyframes[0].bone.text == kLongNameCutText);
    assert(read.boneKeyframes[1].bone.bytes == kCenter);
    assert(read.boneKeyframes[1].bone.text == kCenterText);
    assert(Codes(result) == std::vector<std::string>{"MMD_TEXT_TRUNCATED_CP932"});
    const Location& where = result.diagnostics()[0].location;
    assert(where.section == "boneKeyframes");
    assert(where.index == 0u);
    assert(where.field == "bone");
    assert(where.byteOffset == 50u + 4u + 14u);
}

void
TestInvalidName()
{
    Document doc;
    MorphKeyframe k;
    // 0x81 0x20: a lead byte whose second byte is outside every trail range.
    k.morph = MakeName(Cp932({0x41, 0x81, 0x20, 0x42}), "");
    doc.morphKeyframes.push_back(k);
    const auto result = ReadBytes(Encode(doc, 2));
    assert(result.ok());
    assert(result.value().morphKeyframes[0].morph.text.empty());
    assert(result.value().morphKeyframes[0].morph.bytes.size() == 4);
    assert(Codes(result) == std::vector<std::string>{"MMD_TEXT_INVALID_CP932"});
    assert(result.diagnostics()[0].location.byteOffset == 50u + 4u + 4u + 1u);
    assert(result.diagnostics()[0].location.field == "morph");
}

void
TestIkNameField()
{
    Document doc;
    IkKeyframe ik;
    ik.ik.push_back(IkState{MakeName("ok", "ok"), 1});
    // Twenty bytes: an IK name field is wider than a bone keyframe's.
    ik.ik.push_back(IkState{MakeName(kLongName + Cp932({0x81, 0x5B, 0x41, 0x81}), ""), 0});
    doc.ikKeyframes.push_back(ik);
    const auto result = ReadBytes(Encode(doc));
    assert(result.ok());
    const IkState& state = result.value().ikKeyframes[0].ik[1];
    assert(state.bone.bytes.size() == 20);
    assert(state.bone.text == kLongNameText + "ーA");
    assert(Codes(result) == std::vector<std::string>{"MMD_TEXT_TRUNCATED_CP932"});
    assert(result.diagnostics()[0].location.field == "ik[1].bone");
}

void
TestDiagnosticsAreBounded()
{
    Document doc;
    MorphKeyframe k;
    k.morph = MakeName(Cp932({0x81}), "");
    for (int i = 0; i < 40; ++i) {
        doc.morphKeyframes.push_back(k);
    }
    const auto result = ReadBytes(Encode(doc, 2));
    assert(result.ok());
    assert(result.diagnostics().size() == 17);
    assert(result.diagnostics().back().message == "24 more in morphKeyframes are not listed");
}

void
TestReadFile()
{
    const auto dir = std::filesystem::temp_directory_path() / u8"motionVmd-ユニコード-é";
    std::filesystem::create_directories(dir);
    const auto path = dir / u8"モーション.vmd";
    const Bytes bytes = Encode(SampleDocument());
    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
    }
    const auto result = ReadFile(path);
    assert(result.ok());
    assert(result.value() == SampleDocument());

    ExpectFatal(ReadFile(dir / "missing.vmd"), codes::MotionFileUnreadable);
    ExpectFatal(ReadFile(dir), codes::MotionFileUnreadable);
    std::filesystem::remove_all(dir);
}

} // namespace

void
TestReader()
{
    TestRoundTrip();
    TestVersion1();
    TestSignatureJunkAfterNul();
    TestBadSignature();
    TestEndsAfterAnySection();
    TestTruncation();
    TestCountExceedsBuffer();
    TestTrailingBytes();
    TestNames();
    TestInvalidName();
    TestIkNameField();
    TestDiagnosticsAreBounded();
    TestReadFile();
}
