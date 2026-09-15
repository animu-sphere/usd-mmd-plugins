// SPDX-License-Identifier: Apache-2.0
//
// The PMX reader against bytes built here, so every case states exactly which
// byte it is about: whole models written by PmxEncoder.h and read back, and
// single tables written field by field where the malformed byte is the point.
#include "mmdPmx/Reader.h"

#include "PmxEncoder.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace pmxtest;
using namespace mmd::pmx;

mmd::Result<Document>
ReadBytes(const Bytes& bytes)
{
    return Read(std::span<const std::byte>(bytes.data(), bytes.size()));
}

std::vector<std::string>
Codes(const mmd::Result<Document>& result)
{
    std::vector<std::string> out;
    for (const mmd::Diagnostic& d : result.diagnostics()) {
        out.push_back(d.code);
    }
    return out;
}

const mmd::Diagnostic&
ExpectFatal(const mmd::Result<Document>& result, const std::string& code)
{
    assert(!result.ok());
    assert(result.fatal() != nullptr);
    if (result.fatal()->code != code) {
        std::fprintf(stderr, "expected %s, got %s\n", code.c_str(),
            mmd::FormatDiagnostic(*result.fatal()).c_str());
    }
    assert(result.fatal()->code == code);
    assert(!result.fatal()->recoverable);
    return *result.fatal();
}

void
ExpectFatalAt(const Bytes& bytes, const std::string& code, std::uint64_t offset)
{
    const auto result = ReadBytes(bytes);
    const mmd::Diagnostic& fatal = ExpectFatal(result, code);
    if (fatal.location.byteOffset != offset) {
        std::fprintf(stderr, "%s: expected byte %llu\n", mmd::FormatDiagnostic(fatal).c_str(),
            static_cast<unsigned long long>(offset));
    }
    assert(fatal.location.byteOffset == offset);
}

/// A successful read whose recoverable diagnostics have exactly these codes.
Document
ExpectRead(const Bytes& bytes, const std::vector<std::string>& codes = {})
{
    auto result = ReadBytes(bytes);
    if (!result.ok()) {
        std::fprintf(stderr, "unexpected: %s\n", mmd::FormatDiagnostic(*result.fatal()).c_str());
    }
    assert(result.ok());
    if (Codes(result) != codes) {
        for (const mmd::Diagnostic& d : result.diagnostics()) {
            std::fprintf(stderr, "  %s\n", mmd::FormatDiagnostic(d).c_str());
        }
    }
    assert(Codes(result) == codes);
    assert(CheckInvariants(result.value()).empty());
    return std::move(result).value();
}

enum class Table {
    Vertices, Faces, Textures, Materials, Bones, Morphs, DisplayFrames, RigidBodies,
    Joints, SoftBodies,
};

/// The whole document, except that `replaced` is written by `write`.
Bytes
EncodeWith(const Document& doc, Table replaced, const std::function<void(Writer&)>& write)
{
    Writer w(doc.header);
    EncodeHeader(w, doc);
    using Encoder = void (*)(Writer&, const Document&);
    const std::pair<Table, Encoder> order[] = {
        {Table::Vertices, EncodeVertices},
        {Table::Faces, EncodeFaces},
        {Table::Textures, EncodeTextures},
        {Table::Materials, EncodeMaterials},
        {Table::Bones, EncodeBones},
        {Table::Morphs, EncodeMorphs},
        {Table::DisplayFrames, EncodeDisplayFrames},
        {Table::RigidBodies, EncodeRigidBodies},
        {Table::Joints, EncodeJoints},
        {Table::SoftBodies, EncodeSoftBodies},
    };
    for (const auto& [table, encode] : order) {
        if (table == Table::SoftBodies && doc.header.version != Version::V2_1) {
            continue;
        }
        if (table == replaced) {
            write(w);
        } else {
            encode(w, doc);
        }
    }
    return std::move(w.bytes);
}

/// An empty model with one bone, `vertices` BDEF1 vertices bound to it, and
/// the given faces, all drawn by one material.
Document
Mesh(std::size_t vertices, std::vector<std::uint32_t> faces, Version version = Version::V2_0)
{
    Document doc = EmptyDocument(version);
    Bone root;
    root.name = "root";
    doc.bones.push_back(root);
    for (std::size_t i = 0; i < vertices; ++i) {
        Vertex v;
        v.position = {float(i), 0.0f, 0.0f};
        v.deform.bones[0] = 0;
        doc.vertices.push_back(v);
    }
    doc.faces = std::move(faces);
    if (!doc.faces.empty()) {
        Material all;
        all.name = "all";
        all.faceCount = static_cast<std::int32_t>(doc.faces.size());
        doc.materials.push_back(all);
    }
    return doc;
}

// --- header ------------------------------------------------------------------

/// signature, version, globals -- and nothing after them.
Bytes
Header(float version = 2.0f, std::vector<int> globals = {0, 0, 4, 1, 1, 2, 2, 1})
{
    Bytes out;
    for (char c : {'P', 'M', 'X', ' '}) {
        out.push_back(static_cast<std::byte>(c));
    }
    const auto bits = std::bit_cast<std::uint32_t>(version);
    for (int shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<std::byte>((bits >> shift) & 0xFF));
    }
    out.push_back(static_cast<std::byte>(globals.size()));
    for (int g : globals) {
        out.push_back(static_cast<std::byte>(g));
    }
    return out;
}

/// A header completed into an empty model: four empty strings and every
/// table count zero.
Bytes
Complete(Bytes header, bool v21 = false)
{
    header.resize(header.size() + 4 * 4 + 4 * (v21 ? 10 : 9), std::byte{0});
    return header;
}

void
TestValidHeaders()
{
    const Document v20 = ExpectRead(Complete(Header(2.0f, {0, 0, 4, 1, 1, 2, 2, 1})));
    const mmd::pmx::Header& h = v20.header;
    assert(h.version == Version::V2_0);
    assert(ToString(h.version) == "2.0");
    assert(h.globals.textEncoding == TextEncoding::Utf16Le);
    assert(h.globals.additionalVec4Count == 0);
    assert(h.globals.vertexIndexSize == 4);
    assert(h.globals.textureIndexSize == 1);
    assert(h.globals.materialIndexSize == 1);
    assert(h.globals.boneIndexSize == 2);
    assert(h.globals.morphIndexSize == 2);
    assert(h.globals.rigidBodyIndexSize == 1);
    assert(h.globals.unknown.empty());

    const Document v21 = ExpectRead(Complete(Header(2.1f, {1, 4, 1, 1, 1, 1, 1, 1}), true));
    assert(v21.header.version == Version::V2_1);
    assert(ToString(v21.header.version) == "2.1");
    assert(v21.header.globals.textEncoding == TextEncoding::Utf8);
    assert(v21.header.globals.additionalVec4Count == 4);

    // A 2.1 file ends with the soft-body count; a 2.0 file with that many
    // bytes has four trailing ones.
    ExpectFatalAt(Complete(Header(2.1f, {1, 0, 1, 1, 1, 1, 1, 1})), "MMD_PMX_TRUNCATED_BUFFER",
        Complete(Header(2.1f, {1, 0, 1, 1, 1, 1, 1, 1})).size());
    ExpectRead(Complete(Header(2.0f, {1, 0, 1, 1, 1, 1, 1, 1}), true), {"MMD_PMX_TRAILING_BYTES"});
}

void
TestSignature()
{
    ExpectFatalAt({}, "MMD_PMX_BAD_SIGNATURE", 0);
    Bytes pmd;
    for (int c : {int{'P'}, int{'m'}, int{'d'}, 0}) {
        pmd.push_back(static_cast<std::byte>(c));
    }
    ExpectFatalAt(pmd, "MMD_PMX_BAD_SIGNATURE", 0);
    Bytes lower = Complete(Header());
    lower[1] = std::byte{'m'};
    ExpectFatalAt(lower, "MMD_PMX_BAD_SIGNATURE", 0);

    // A prefix of the signature is a truncated PMX, not a foreign file.
    Bytes prefix = Header();
    prefix.resize(2);
    ExpectFatalAt(prefix, "MMD_PMX_TRUNCATED_BUFFER", 0);
}

void
TestVersion()
{
    ExpectFatalAt(Complete(Header(1.0f)), "MMD_PMX_UNSUPPORTED_VERSION", 4);
    ExpectFatalAt(Complete(Header(2.2f)), "MMD_PMX_UNSUPPORTED_VERSION", 4);
    // Compared exactly: the float nearest 2.1 is legal, its neighbour is not.
    ExpectFatalAt(Complete(Header(std::nextafter(2.1f, 3.0f))), "MMD_PMX_UNSUPPORTED_VERSION", 4);

    Bytes shortVersion = Header();
    shortVersion.resize(6);
    ExpectFatalAt(shortVersion, "MMD_PMX_TRUNCATED_BUFFER", 4);
}

void
TestGlobals()
{
    Bytes noCount = Header();
    noCount.resize(8);
    ExpectFatalAt(noCount, "MMD_PMX_TRUNCATED_BUFFER", 8);

    ExpectFatalAt(Complete(Header(2.0f, {0, 0, 1, 1, 1, 1, 1})), "MMD_PMX_INVALID_GLOBALS", 8);

    Bytes shortGlobals = Header();
    shortGlobals.resize(12);
    ExpectFatalAt(shortGlobals, "MMD_PMX_TRUNCATED_BUFFER", 9);

    ExpectFatalAt(Complete(Header(2.0f, {2, 0, 1, 1, 1, 1, 1, 1})),
        "MMD_TEXT_INVALID_ENCODING_FLAG", 9);
    ExpectFatalAt(Complete(Header(2.0f, {0, 5, 1, 1, 1, 1, 1, 1})), "MMD_PMX_INVALID_GLOBALS", 10);

    // Each index width, in its own byte.
    for (std::size_t i = 2; i < 8; ++i) {
        for (int bad : {0, 3, 8}) {
            std::vector<int> globals{0, 0, 1, 1, 1, 1, 1, 1};
            globals[i] = bad;
            ExpectFatalAt(Complete(Header(2.0f, globals)), "MMD_PMX_INVALID_INDEX_SIZE", 9 + i);
        }
    }
    const auto located = ReadBytes(Complete(Header(2.0f, {0, 0, 1, 1, 1, 3, 1, 1})));
    assert(located.fatal()->location.field == "globals[5]");
    assert(located.fatal()->location.table == "header");
}

void
TestUnknownGlobals()
{
    const auto result = ReadBytes(Complete(Header(2.0f, {0, 0, 1, 1, 1, 1, 1, 1, 7, 9})));
    assert(result.ok());
    assert(result.diagnostics().size() == 1);
    const mmd::Diagnostic& d = result.diagnostics()[0];
    assert(d.code == "MMD_PMX_UNKNOWN_GLOBALS");
    assert(d.severity == mmd::Severity::Warning);
    assert(d.recoverable);
    assert(d.location.byteOffset == 17);
    assert((result.value().header.globals.unknown == std::vector<std::uint8_t>{7, 9}));
}

// --- whole models ------------------------------------------------------------

void
TestRoundTrip()
{
    for (Version version : {Version::V2_0, Version::V2_1}) {
        for (TextEncoding encoding : {TextEncoding::Utf16Le, TextEncoding::Utf8}) {
            for (std::uint8_t width : {1, 2, 4}) {
                for (std::uint8_t extra : {0, 1, 4}) {
                    const Document doc = SampleDocument(version, encoding, width, extra);
                    assert(CheckInvariants(doc).empty());
                    const Document read = ExpectRead(Encode(doc));
                    assert(read == doc);
                }
            }
        }
    }

    // Mixed widths: each index kind is read at its own width.
    Document mixed = SampleDocument(Version::V2_1, TextEncoding::Utf8, 1);
    Globals& g = mixed.header.globals;
    g.vertexIndexSize = 2;
    g.textureIndexSize = 4;
    g.materialIndexSize = 1;
    g.boneIndexSize = 4;
    g.morphIndexSize = 2;
    g.rigidBodyIndexSize = 1;
    assert(ExpectRead(Encode(mixed)) == mixed);

    // The sample's source facts survive exactly, Japanese included.
    const Document read = ExpectRead(Encode(SampleDocument(Version::V2_0, TextEncoding::Utf16Le, 1)));
    assert(read.model.name == "サンプル");
    assert(read.model.comment == "テスト用のモデル\n二行目");
    assert(read.textures[0] == "tex\\髪.png");
    assert(read.bones[1].name == "左腕" && read.bones[1].englishName == "LeftArm");
    assert(read.bones[1].tailBone == 2);
    assert(read.bones[3].ik.links.size() == 2 && read.bones[3].ik.links[0].hasLimits);
    assert(read.materials[1].toonReference == ToonReference::Shared);
    assert(read.materials[1].sharedToon == 3);
    assert(read.morphs[6].materialOffsets[0].material == kNoIndex);
    assert(read.morphs[0].OffsetCount() == 2);
    assert(read.softBodies.empty());
}

void
TestIndexSignedness()
{
    // A vertex index is unsigned at widths 1 and 2 ...
    const Document narrow = ExpectRead(Encode(Mesh(200, {0, 100, 199})));
    assert(narrow.faces[2] == 199);

    Document wide = Mesh(32770, {0, 1, 32769});
    wide.header.globals.vertexIndexSize = 2;
    assert(ExpectRead(Encode(wide)).faces[2] == 32769);

    // ... and every other index is signed: 0xFF at width 1 is -1, "none".
    Document parent = Mesh(0, {});
    parent.bones.push_back(parent.bones[0]);
    parent.bones[1].parent = 0;
    for (std::uint8_t width : {1, 2, 4}) {
        parent.header.globals.boneIndexSize = width;
        const Document read = ExpectRead(Encode(parent));
        assert(read.bones[0].parent == kNoIndex);
        assert(read.bones[1].parent == 0);
    }

    // A vertex index read at width 4 that is negative is out of range.
    Document negative = Mesh(3, {});
    negative.header.globals.vertexIndexSize = 4;
    std::size_t at = 0;
    const Bytes bytes = EncodeWith(negative, Table::Faces, [&](Writer& w) {
        w.I32(3);
        w.Vertex(0);
        at = w.bytes.size();
        w.Vertex(-1);
        w.Vertex(2);
    });
    ExpectFatalAt(bytes, "MMD_PMX_FACE_INDEX_OUT_OF_RANGE", at);
}

void
TestTrailingBytes()
{
    Bytes bytes = Encode(SampleDocument(Version::V2_1, TextEncoding::Utf8, 2));
    const std::size_t end = bytes.size();
    bytes.push_back(std::byte{0xDE});
    bytes.push_back(std::byte{0xAD});
    const auto result = ReadBytes(bytes);
    assert(result.ok());
    assert(Codes(result) == std::vector<std::string>{"MMD_PMX_TRAILING_BYTES"});
    assert(result.diagnostics()[0].severity == mmd::Severity::Warning);
    assert(result.diagnostics()[0].location.byteOffset == end);
}

void
TestCounts()
{
    const Document doc = EmptyDocument();
    for (std::int32_t count : {std::numeric_limits<std::int32_t>::max(), -1, 1}) {
        std::size_t at = 0;
        const Bytes bytes = EncodeWith(doc, Table::Vertices, [&](Writer& w) {
            at = w.bytes.size();
            w.I32(count);
        });
        // Even a count of 1 exceeds what the remaining 32 bytes can hold.
        ExpectFatalAt(bytes, "MMD_PMX_COUNT_EXCEEDS_BUFFER", at);
        assert(ReadBytes(bytes).fatal()->location.field == "count");
        assert(ReadBytes(bytes).fatal()->location.table == "vertices");
    }

    // A text length that is negative, or longer than what is left.
    for (std::int32_t length : {-1, 1000}) {
        std::size_t at = 0;
        const Bytes bytes = EncodeWith(doc, Table::Textures, [&](Writer& w) {
            w.I32(1);
            at = w.bytes.size();
            w.I32(length);
        });
        ExpectFatalAt(bytes, "MMD_PMX_COUNT_EXCEEDS_BUFFER", at);
    }

    // A list count inside a record is bounded the same way.
    Document ik = Mesh(0, {});
    std::size_t at = 0;
    const Bytes bytes = EncodeWith(ik, Table::Bones, [&](Writer& w) {
        w.I32(1);
        w.Text("ik");
        w.Text("");
        w.Floats(Vec3{});
        w.Bone(kNoIndex);
        w.I32(0);
        w.U16(BoneFlag::Ik);
        w.Floats(Vec3{});
        w.Bone(0);
        w.I32(1);
        w.F32(0.5f);
        at = w.bytes.size();
        w.I32(1000000);
    });
    ExpectFatalAt(bytes, "MMD_PMX_COUNT_EXCEEDS_BUFFER", at);
    assert(ReadBytes(bytes).fatal()->location.field == "ik.linkCount");
}

void
TestTruncationEverywhere()
{
    // Every proper prefix of a valid file is fatal: the last table's count is
    // its last four bytes, so no prefix is a complete file.
    for (Version version : {Version::V2_0, Version::V2_1}) {
        const Bytes bytes = Encode(SampleDocument(version, TextEncoding::Utf16Le, 2));
        for (std::size_t n = 0; n < bytes.size(); ++n) {
            const Bytes prefix(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(n));
            const auto result = ReadBytes(prefix);
            assert(!result.ok());
            const std::string& code = result.fatal()->code;
            assert(code == "MMD_PMX_TRUNCATED_BUFFER" || code == "MMD_PMX_COUNT_EXCEEDS_BUFFER"
                || (n == 0 && code == "MMD_PMX_BAD_SIGNATURE"));
            assert(result.fatal()->location.byteOffset <= n);
        }
    }
}

// --- vertices ------------------------------------------------------------------

void
TestDeformTypes()
{
    Document doc = Mesh(1, {});
    doc.header.globals.additionalVec4Count = 1;
    const std::size_t header = Encode(EmptyDocument()).size() - 9 * 4;
    const std::size_t typeOffset = header + 4 + 12 + 12 + 8 + 16;

    doc.vertices[0].deform.type = static_cast<DeformType>(5);
    ExpectFatalAt(Encode(doc), "MMD_PMX_INVALID_DEFORM_TYPE", typeOffset);

    doc.vertices[0].deform.type = DeformType::Qdef;
    ExpectFatalAt(Encode(doc), "MMD_PMX_INVALID_DEFORM_TYPE", typeOffset);
    doc.header.version = Version::V2_1;
    ExpectRead(Encode(doc));
}

void
TestDeformBones()
{
    // None with weight 0 is accepted; none with a weight is not, and neither
    // is a bone that does not exist, whatever its weight.
    Document doc = Mesh(4, {});
    Deform& bdef1 = doc.vertices[0].deform;
    bdef1.bones[0] = kNoIndex;
    Deform& bdef2 = doc.vertices[1].deform;
    bdef2.type = DeformType::Bdef2;
    bdef2.bones = {0, kNoIndex, kNoIndex, kNoIndex};
    bdef2.weights[0] = 1.0f;
    Deform& half = doc.vertices[2].deform;
    half = bdef2;
    half.weights[0] = 0.5f;
    Deform& bdef4 = doc.vertices[3].deform;
    bdef4.type = DeformType::Bdef4;
    bdef4.bones = {0, 5, kNoIndex, kNoIndex};
    bdef4.weights = {1.0f, 0.0f, 0.0f, 0.0f};

    auto result = ReadBytes(Encode(doc));
    assert(result.ok());
    assert(Codes(result)
        == (std::vector<std::string>{
            "MMD_PMX_INDEX_OUT_OF_RANGE", "MMD_PMX_INDEX_OUT_OF_RANGE", "MMD_PMX_INDEX_OUT_OF_RANGE"}));
    const auto& d = result.diagnostics();
    assert(d[0].location.table == "vertices" && d[0].location.index == 0u);
    assert(d[0].location.field == "deform.bones[0]");
    assert(d[1].location.index == 2u && d[1].location.field == "deform.bones[1]");
    assert(d[2].location.index == 3u && d[2].location.field == "deform.bones[1]");
    assert(d[0].severity == mmd::Severity::Error && d[0].recoverable);
    assert(!d[0].location.byteOffset.has_value());
    assert(d[1].message == "the influence names no bone, but its weight is 0.5");
    assert(mmd::FormatDiagnostic(d[2])
        == "MMD_PMX_INDEX_OUT_OF_RANGE: the index is 5, but the bones table holds 1; it is "
           "read as none (vertices[3].deform.bones[1])");
    // The reference is "none" now; the document stays well-formed.
    assert(result.value().vertices[3].deform.bones[1] == kNoIndex);
    assert(CheckInvariants(result.value()).empty());
}

// --- faces ---------------------------------------------------------------------

void
TestFaces()
{
    const Document doc = Mesh(5, {});
    std::size_t at = 0;
    Bytes bytes = EncodeWith(doc, Table::Faces, [&](Writer& w) {
        at = w.bytes.size();
        w.I32(4);
        for (int v : {0, 1, 2, 3}) {
            w.Vertex(v);
        }
    });
    ExpectFatalAt(bytes, "MMD_PMX_FACE_COUNT_NOT_TRIANGLES", at);

    bytes = EncodeWith(doc, Table::Faces, [&](Writer& w) {
        w.I32(6);
        for (int v : {0, 1, 2, 3, 4}) {
            w.Vertex(v);
        }
        at = w.bytes.size();
        w.Vertex(5);
    });
    ExpectFatalAt(bytes, "MMD_PMX_FACE_INDEX_OUT_OF_RANGE", at);
    const auto result = ReadBytes(bytes);
    assert(result.fatal()->location.table == "faces");
    assert(result.fatal()->location.index == 5u);
}

// --- materials -------------------------------------------------------------------

Material
Drawing(std::int32_t faceCount)
{
    Material m;
    m.name = "m";
    m.faceCount = faceCount;
    return m;
}

void
TestMaterialFaceRanges()
{
    Document doc = Mesh(3, {0, 1, 2, 2, 1, 0});
    for (std::int32_t bad : {4, 9, -3}) {
        doc.materials = {Drawing(bad)};
        const auto result = ReadBytes(Encode(doc));
        ExpectFatal(result, "MMD_PMX_MATERIAL_FACES_EXCEED_TABLE");
        assert(result.fatal()->location.field == "faceCount");
        assert(result.fatal()->location.index == 0u);
    }
    doc.materials = {Drawing(3), Drawing(6)};
    ExpectFatal(ReadBytes(Encode(doc)), "MMD_PMX_MATERIAL_FACES_EXCEED_TABLE");

    doc.materials = {Drawing(3), Drawing(3)};
    ExpectRead(Encode(doc));

    // Short: the tail is drawn by no material, and the import continues.
    doc.materials = {Drawing(3), Drawing(0)};
    const auto result = ReadBytes(Encode(doc));
    assert(result.ok());
    assert(Codes(result) == std::vector<std::string>{"MMD_PMX_MATERIAL_FACES_SHORT"});
    assert(result.diagnostics()[0].severity == mmd::Severity::Error);
    assert(result.diagnostics()[0].location.table == "materials");
}

void
TestMaterialFields()
{
    Document doc = Mesh(3, {0, 1, 2});
    doc.textures = {"a.png", "b.png", "c.png"};
    Material m = Drawing(3);
    m.toonReference = static_cast<ToonReference>(2);
    doc.materials = {m};
    ExpectFatal(ReadBytes(Encode(doc)), "MMD_PMX_INVALID_LAYOUT_FLAG");

    m.toonReference = ToonReference::Texture;
    m.texture = 7;
    m.sphereTexture = -2;
    m.toonTexture = 2;
    // Stored, never judged here: canonicalization decides what these mean.
    m.sphereMode = 9;
    m.flags = 0xFF;
    doc.materials = {m};
    const auto result = ReadBytes(Encode(doc));
    assert(result.ok());
    assert(Codes(result)
        == (std::vector<std::string>{"MMD_PMX_INDEX_OUT_OF_RANGE", "MMD_PMX_INDEX_OUT_OF_RANGE"}));
    assert(result.diagnostics()[0].location.field == "texture");
    assert(result.diagnostics()[1].location.field == "sphereTexture");
    const Material& read = result.value().materials[0];
    assert(read.texture == kNoIndex && read.sphereTexture == kNoIndex && read.toonTexture == 2);
    assert(read.sphereMode == 9 && read.flags == 0xFF);
}

// --- bones ----------------------------------------------------------------------

void
TestBones()
{
    Document doc = SampleDocument(Version::V2_0, TextEncoding::Utf8, 1);
    doc.bones[0].parent = 9;
    doc.bones[3].ik.links[1].bone = 9;
    doc.bones[2].appendParent = 4;
    const auto result = ReadBytes(Encode(doc));
    assert(result.ok());
    const auto& d = result.diagnostics();
    assert(d.size() == 3);
    assert(d[0].location.index == 0u && d[0].location.field == "parent");
    assert(d[1].location.index == 2u && d[1].location.field == "appendParent");
    assert(d[2].location.index == 3u && d[2].location.field == "ik.links[1].bone");
    const Document& read = result.value();
    assert(read.bones[0].parent == kNoIndex);
    assert(read.bones[3].ik.links.size() == 2 && read.bones[3].ik.links[1].bone == kNoIndex);

    // An IK link's limit flag selects what follows it, so only 0 and 1 are
    // readable.
    Document one = Mesh(0, {});
    std::size_t at = 0;
    const Bytes bytes = EncodeWith(one, Table::Bones, [&](Writer& w) {
        w.I32(1);
        w.Text("ik");
        w.Text("");
        w.Floats(Vec3{});
        w.Bone(kNoIndex);
        w.I32(0);
        w.U16(BoneFlag::Ik);
        w.Floats(Vec3{});
        w.Bone(0);
        w.I32(1);
        w.F32(0.5f);
        w.I32(1);
        w.Bone(0);
        at = w.bytes.size();
        w.U8(2);
    });
    ExpectFatalAt(bytes, "MMD_PMX_INVALID_LAYOUT_FLAG", at);
    assert(ReadBytes(bytes).fatal()->location.field == "ik.links[0].hasLimits");
}

// --- morphs ------------------------------------------------------------------------

void
TestMorphTypes()
{
    Document doc = Mesh(1, {});
    Morph m;
    m.name = "m";
    for (std::uint8_t type : {9, 10, 11, 255}) {
        m.type = static_cast<MorphType>(type);
        doc.morphs = {m};
        doc.header.version = Version::V2_0;
        ExpectFatal(ReadBytes(Encode(doc)), "MMD_PMX_INVALID_MORPH_TYPE");
        doc.header.version = Version::V2_1;
        if (type <= 10) {
            ExpectRead(Encode(doc));
        } else {
            ExpectFatal(ReadBytes(Encode(doc)), "MMD_PMX_INVALID_MORPH_TYPE");
        }
    }
}

void
TestMorphTargets()
{
    Document doc = SampleDocument(Version::V2_1, TextEncoding::Utf16Le, 2);
    doc.morphs[0].vertexOffsets[1].vertex = 99;
    doc.morphs[1].groupOffsets[0].morph = 50;
    doc.morphs[2].boneOffsets[0].bone = -3;
    doc.morphs[6].materialOffsets[1].material = 2;
    doc.morphs[8].impulseOffsets[0].rigidBody = 2;
    const auto result = ReadBytes(Encode(doc));
    assert(result.ok());
    const auto& d = result.diagnostics();
    assert(d.size() == 5);
    assert(d[0].location.table == "morphs" && d[0].location.index == 0u);
    assert(d[0].location.field == "offsets[1].vertex");
    assert(d[1].location.field == "offsets[0].morph");
    assert(d[2].location.field == "offsets[0].bone");
    assert(d[3].location.field == "offsets[1].material");
    assert(d[4].location.field == "offsets[0].rigidBody");
    assert(result.value().morphs[0].vertexOffsets[1].vertex == kNoIndex);
    // kNoIndex means "every material" in a material morph, and stays legal.
    assert(result.value().morphs[6].materialOffsets[0].material == kNoIndex);
}

// --- display frames, physics ----------------------------------------------------------

void
TestDisplayFrames()
{
    Document doc = SampleDocument(Version::V2_0, TextEncoding::Utf8, 1);
    doc.displayFrames[2].elements[1].index = 30;
    doc.displayFrames[1].elements[0].index = 40;
    auto result = ReadBytes(Encode(doc));
    assert(result.ok());
    assert(result.diagnostics().size() == 2);
    assert(result.diagnostics()[0].location.index == 1u);
    assert(result.diagnostics()[0].location.field == "elements[0].index");
    assert(result.diagnostics()[1].location.index == 2u);

    doc.displayFrames[1].elements[0].kind = static_cast<FrameElementKind>(2);
    ExpectFatal(ReadBytes(Encode(doc)), "MMD_PMX_INVALID_LAYOUT_FLAG");
}

void
TestPhysics()
{
    Document doc = SampleDocument(Version::V2_1, TextEncoding::Utf8, 4);
    doc.rigidBodies[0].bone = 12;
    doc.joints[0].rigidBodyB = 2;
    doc.joints[1].rigidBodyA = -7;
    doc.softBodies[0].material = 5;
    doc.softBodies[0].anchors[1].vertex = 99;
    doc.softBodies[0].pinVertices[0] = -1;
    const auto result = ReadBytes(Encode(doc));
    assert(result.ok());
    const auto& d = result.diagnostics();
    assert(d.size() == 6);
    assert(d[0].location.table == "rigidBodies" && d[0].location.field == "bone");
    assert(d[1].location.table == "joints" && d[1].location.field == "rigidBodyB");
    assert(d[2].location.index == 1u && d[2].location.field == "rigidBodyA");
    assert(d[3].location.table == "softBodies" && d[3].location.field == "material");
    assert(d[4].location.field == "anchors[1].vertex");
    // A vertex index is never optional, so -1 is out of range there.
    assert(d[5].location.field == "pinVertices[0]");

    // Soft bodies exist in 2.1 only: a 2.0 reader stops after the joints.
    Document v20 = SampleDocument(Version::V2_0, TextEncoding::Utf8, 1);
    assert(ExpectRead(Encode(v20)).softBodies.empty());
}

// --- text --------------------------------------------------------------------------

void
TestInvalidText()
{
    Document doc = Mesh(3, {0, 1, 2}, Version::V2_1);
    doc.header.globals.textEncoding = TextEncoding::Utf8;
    Material m = Drawing(3);
    m.name = "ok\xFF";
    m.englishName = "fine";
    doc.materials = {m};
    const auto result = ReadBytes(Encode(doc));
    assert(result.ok());
    assert(Codes(result) == std::vector<std::string>{"MMD_TEXT_INVALID_UTF8"});
    const mmd::Diagnostic& d = result.diagnostics()[0];
    assert(d.severity == mmd::Severity::Error && d.recoverable);
    assert(d.location.table == "materials" && d.location.index == 0u);
    assert(d.location.field == "name");
    assert(d.location.byteOffset.has_value());
    // The whole string is dropped, never repaired, and reading goes on.
    assert(result.value().materials[0].name.empty());
    assert(result.value().materials[0].englishName == "fine");

    for (const std::string& raw : {std::string("abc"), std::string("\x3D\xD8", 2)}) {
        Document utf16 = EmptyDocument();
        utf16.model.name = raw;
        const auto bad = ReadBytes(Encode(utf16, /*rawText=*/true));
        assert(bad.ok());
        assert(Codes(bad) == std::vector<std::string>{"MMD_TEXT_INVALID_UTF16"});
        assert(bad.diagnostics()[0].location.table == "model");
        assert(bad.diagnostics()[0].location.field == "name");
        assert(bad.value().model.name.empty());
    }
}

// --- diagnostics ---------------------------------------------------------------------

void
TestDiagnosticLimit()
{
    Document doc = Mesh(40, {});
    for (Vertex& v : doc.vertices) {
        v.deform.bones[0] = 3;
    }
    doc.bones[0].parent = 3;
    const auto result = ReadBytes(Encode(doc));
    assert(result.ok());
    const auto& d = result.diagnostics();
    // 16 of the 40 vertices, the bone (another table), then the summary.
    assert(d.size() == 16 + 1 + 1);
    for (std::size_t i = 0; i < 16; ++i) {
        assert(d[i].location.table == "vertices" && d[i].location.index == i);
    }
    assert(d[16].location.table == "bones");
    assert(d[17].code == "MMD_PMX_INDEX_OUT_OF_RANGE");
    assert(d[17].location.table == "vertices" && !d[17].location.index.has_value());
    assert(d[17].message.find("24 more") != std::string::npos);
    // Every vertex was still repaired, listed or not.
    assert(CheckInvariants(result.value()).empty());
}

// --- ReadFile ------------------------------------------------------------------------

void
TestReadFile()
{
    namespace fs = std::filesystem;
    const fs::path scratch = fs::temp_directory_path() / "mmdPmx-tests";
    fs::create_directories(scratch);

    // Named in UTF-8 and handed over as a path: no code page is involved.
    const fs::path path = scratch / fs::path(u8"ユニコード-é.pmx");
    const Document doc = SampleDocument(Version::V2_1, TextEncoding::Utf16Le, 2);
    const Bytes bytes = Encode(doc);
    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }
    const auto read = ReadFile(path);
    assert(read.ok());
    assert(read.value() == doc);

    ExpectFatal(ReadFile(scratch / "missing.pmx"), "MMD_PMX_FILE_UNREADABLE");
    ExpectFatal(ReadFile(scratch), "MMD_PMX_FILE_UNREADABLE");

    fs::remove(path);
}

}  // namespace

void
TestReader()
{
    TestValidHeaders();
    TestSignature();
    TestVersion();
    TestGlobals();
    TestUnknownGlobals();
    TestRoundTrip();
    TestIndexSignedness();
    TestTrailingBytes();
    TestCounts();
    TestTruncationEverywhere();
    TestDeformTypes();
    TestDeformBones();
    TestFaces();
    TestMaterialFaceRanges();
    TestMaterialFields();
    TestBones();
    TestMorphTypes();
    TestMorphTargets();
    TestDisplayFrames();
    TestPhysics();
    TestInvalidText();
    TestDiagnosticLimit();
    TestReadFile();
}
