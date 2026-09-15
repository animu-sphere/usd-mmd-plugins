// SPDX-License-Identifier: Apache-2.0
#include "mmdPmx/Reader.h"

#include "mmdPmx/Codes.h"

#include "ByteReader.h"
#include "DiagnosticList.h"
#include "Text.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace mmd::pmx {

namespace {

constexpr std::array<std::byte, 4> kSignature{
    std::byte{'P'}, std::byte{'M'}, std::byte{'X'}, std::byte{' '}};

// The eight globals PMX defines; a file may declare more (PMX_CONTRACT.md §3).
constexpr std::size_t kDefinedGlobals = 8;

// globals[2]..[7], in order, as they are named in diagnostics.
constexpr std::array<const char*, 6> kIndexSizeNames{
    "vertex", "texture", "material", "bone", "morph", "rigidBody"};

constexpr std::array<std::string_view, 4> kDeformBoneFields{
    "deform.bones[0]", "deform.bones[1]", "deform.bones[2]", "deform.bones[3]"};
constexpr std::array<std::string_view, 4> kDeformWeightFields{
    "deform.weights[0]", "deform.weights[1]", "deform.weights[2]", "deform.weights[3]"};
constexpr std::array<std::string_view, 4> kAdditionalVec4Fields{
    "additionalVec4[0]", "additionalVec4[1]", "additionalVec4[2]", "additionalVec4[3]"};

enum class IndexKind { Vertex, Texture, Material, Bone, Morph, RigidBody };

std::string
FloatText(float value)
{
    char buffer[32];
    const auto [end, ec] = std::to_chars(std::begin(buffer), std::end(buffer), value);
    return ec == std::errc() ? std::string(buffer, end) : std::string("?");
}

std::string
Plural(std::size_t n, std::string_view noun)
{
    return std::to_string(n) + " " + std::string(noun);
}

/// An index into a table of `size` elements that names none of them, where
/// kNoIndex is a legal "none" (PMX_CONTRACT.md §4).
bool
OutOfRange(std::int32_t index, std::size_t size)
{
    return index < kNoIndex || (index >= 0 && static_cast<std::size_t>(index) >= size);
}

/// The same for a vertex index, which is never optional.
bool
VertexOutOfRange(std::int32_t index, std::size_t size)
{
    return index < 0 || static_cast<std::size_t>(index) >= size;
}

/// One pass over the bytes, in file order. Every Read* returns false after
/// recording the fatal diagnostic that stopped it; nothing is read after that.
class Parser {
public:
    explicit Parser(std::span<const std::byte> bytes) : _size(bytes.size()), _in(bytes) {}

    Result<Document> Run()
    {
        const bool read = _ReadHeader() && _ReadModelInfo() && _ReadVertices()
            && _ReadFaces() && _ReadTextures() && _ReadMaterials() && _ReadBones()
            && _ReadMorphs() && _ReadDisplayFrames() && _ReadRigidBodies()
            && _ReadJoints() && _ReadSoftBodies() && _ReadEnd();
        if (!read) {
            return Result<Document>::Failure(std::move(*_fatal), _diagnostics.Take());
        }
        _Validate();
        return Result<Document>::Success(std::move(_doc), _diagnostics.Take());
    }

private:
    // --- where we are -------------------------------------------------------

    void _Enter(std::string_view table)
    {
        _table = table;
        _index.reset();
        _sub = {};
        _subIndex.reset();
    }

    void _At(std::size_t index)
    {
        _index = index;
        _sub = {};
        _subIndex.reset();
    }

    void _AtSub(std::string_view sub, std::size_t index)
    {
        _sub = sub;
        _subIndex = index;
    }

    /// Built only when a diagnostic needs it: the field names below are
    /// literals, and a record's position is kept as numbers until then.
    Location _Here(std::string_view field, std::optional<std::size_t> offset) const
    {
        Location where;
        where.byteOffset = offset;
        where.table = std::string(_table);
        where.index = _index;
        if (!_sub.empty()) {
            where.field = std::string(_sub) + "[" + std::to_string(*_subIndex) + "]";
            if (!field.empty()) {
                where.field += ".";
            }
        }
        where.field += field;
        return where;
    }

    // --- failure ------------------------------------------------------------

    bool _Fatal(const Code& code, std::string message, std::string_view field,
        std::size_t offset)
    {
        _fatal = MakeDiagnostic(code, std::move(message), _Here(field, offset));
        return false;
    }

    /// A failed read does not advance, so the offset is where the field starts.
    bool _Truncated(std::string_view field)
    {
        return _Fatal(codes::PmxTruncatedBuffer,
            "the file ends before this field does (the file is " + Plural(_size, "bytes") + ")",
            field, _in.offset());
    }

    // --- primitives ---------------------------------------------------------

    template <class T>
    bool _Take(std::optional<T> value, T& out, std::string_view field)
    {
        if (!value) {
            return _Truncated(field);
        }
        out = *value;
        return true;
    }

    bool _U8(std::uint8_t& out, std::string_view field) { return _Take(_in.U8(), out, field); }
    bool _U16(std::uint16_t& out, std::string_view field) { return _Take(_in.U16(), out, field); }
    bool _I32(std::int32_t& out, std::string_view field) { return _Take(_in.I32(), out, field); }
    bool _F32(float& out, std::string_view field) { return _Take(_in.F32(), out, field); }

    template <std::size_t N>
    bool _Floats(std::array<float, N>& out, std::string_view field)
    {
        if (_in.remaining() < 4 * N) {
            return _Truncated(field);
        }
        for (float& value : out) {
            value = *_in.F32();
        }
        return true;
    }

    template <std::size_t N>
    bool _Ints(std::array<std::int32_t, N>& out, std::string_view field)
    {
        if (_in.remaining() < 4 * N) {
            return _Truncated(field);
        }
        for (std::int32_t& value : out) {
            value = *_in.I32();
        }
        return true;
    }

    std::uint8_t _Width(IndexKind kind) const
    {
        const Globals& g = _doc.header.globals;
        switch (kind) {
        case IndexKind::Vertex:
            return g.vertexIndexSize;
        case IndexKind::Texture:
            return g.textureIndexSize;
        case IndexKind::Material:
            return g.materialIndexSize;
        case IndexKind::Bone:
            return g.boneIndexSize;
        case IndexKind::Morph:
            return g.morphIndexSize;
        case IndexKind::RigidBody:
            return g.rigidBodyIndexSize;
        }
        return 4;
    }

    /// An index at its header-declared width (PMX_CONTRACT.md §4): a vertex
    /// index is unsigned at widths 1 and 2, every other index is signed, and
    /// every index is a signed int32 at width 4. Range is checked later, once
    /// the table it points into has been read.
    bool _Index(IndexKind kind, std::int32_t& out, std::string_view field)
    {
        const bool isSigned = kind != IndexKind::Vertex;
        switch (_Width(kind)) {
        case 1: {
            std::uint8_t v = 0;
            if (!_U8(v, field)) {
                return false;
            }
            out = isSigned ? static_cast<std::int8_t>(v) : static_cast<std::int32_t>(v);
            return true;
        }
        case 2: {
            std::uint16_t v = 0;
            if (!_U16(v, field)) {
                return false;
            }
            out = isSigned ? static_cast<std::int16_t>(v) : static_cast<std::int32_t>(v);
            return true;
        }
        default:
            return _I32(out, field);
        }
    }

    /// An int32 byte length, then that many bytes in the header's encoding
    /// (TEXT_ENCODING_POLICY.md §3). Undecodable text is recoverable: the
    /// string is empty and the diagnostic names where decoding stopped.
    bool _Text(std::string& out, std::string_view field)
    {
        const std::size_t start = _in.offset();
        std::int32_t length = 0;
        if (!_I32(length, field)) {
            return false;
        }
        if (length < 0 || static_cast<std::size_t>(length) > _in.remaining()) {
            return _Fatal(codes::PmxCountExceedsBuffer,
                "the text declares " + std::to_string(length) + " bytes; "
                    + Plural(_in.remaining(), "bytes") + " remain",
                field, start);
        }
        const std::span<const std::byte> bytes = *_in.Bytes(static_cast<std::size_t>(length));
        const bool utf8 = _doc.header.globals.textEncoding == TextEncoding::Utf8;
        const std::optional<detail::DecodeError> error =
            utf8 ? detail::DecodeUtf8(bytes, out) : detail::DecodeUtf16Le(bytes, out);
        if (error) {
            _diagnostics.Add(utf8 ? codes::TextInvalidUtf8 : codes::TextInvalidUtf16,
                std::string("the text is not valid ") + (utf8 ? "UTF-8" : "UTF-16LE") + " ("
                    + error->reason + "); it is read as empty",
                _Here(field, start + 4 + error->offset));
        }
        return true;
    }

    /// A table or list count, accepted only if the bytes left can hold that
    /// many records of the smallest encoding one can have: that bounds every
    /// allocation by the input's size with no arbitrary cap
    /// (PMX_CONTRACT.md §2).
    bool _Count(std::size_t& out, std::size_t minimumRecordSize, std::string_view field)
    {
        const std::size_t start = _in.offset();
        std::int32_t n = 0;
        if (!_I32(n, field)) {
            return false;
        }
        if (n < 0) {
            return _Fatal(codes::PmxCountExceedsBuffer,
                "the count is " + std::to_string(n) + ", which is negative", field, start);
        }
        if (static_cast<std::size_t>(n) > _in.remaining() / minimumRecordSize) {
            return _Fatal(codes::PmxCountExceedsBuffer,
                "the count declares " + std::to_string(n) + " records of at least "
                    + Plural(minimumRecordSize, "bytes") + "; "
                    + Plural(_in.remaining(), "bytes") + " remain",
                field, start);
        }
        out = static_cast<std::size_t>(n);
        return true;
    }

    // --- header (PMX_CONTRACT.md §3) ------------------------------------------

    bool _ReadHeader()
    {
        _Enter("header");

        // A non-empty file too short to hold the signature, whose bytes still
        // match "PMX ", is a truncated PMX; anything else -- an empty file
        // included -- is not a PMX at all.
        const auto head = _in.Bytes(std::min(_size, kSignature.size()));
        if (_size == 0 || !std::equal(head->begin(), head->end(), kSignature.begin())) {
            return _Fatal(codes::PmxBadSignature,
                "the file does not start with the PMX signature \"PMX \"", "signature", 0);
        }
        if (_size < kSignature.size()) {
            return _Fatal(codes::PmxTruncatedBuffer,
                "the file ends inside header.signature (" + Plural(_size, "bytes") + ")",
                "signature", 0);
        }

        // Exactly 2.0 or 2.1, both exactly representable in binary32.
        const std::size_t versionOffset = _in.offset();
        float version = 0.0f;
        if (!_F32(version, "version")) {
            return false;
        }
        if (version == 2.0f) {
            _doc.header.version = Version::V2_0;
        } else if (version == 2.1f) {
            _doc.header.version = Version::V2_1;
        } else {
            return _Fatal(codes::PmxUnsupportedVersion,
                "PMX version " + FloatText(version) + " is not 2.0 or 2.1", "version",
                versionOffset);
        }

        const std::size_t countOffset = _in.offset();
        std::uint8_t count = 0;
        if (!_U8(count, "globalsCount")) {
            return false;
        }
        if (count < kDefinedGlobals) {
            return _Fatal(codes::PmxInvalidGlobals,
                "the header declares " + std::to_string(count) + " globals; PMX defines 8",
                "globalsCount", countOffset);
        }
        const std::size_t globalsOffset = _in.offset();
        const auto globalBytes = _in.Bytes(count);
        if (!globalBytes) {
            return _Truncated("globals");
        }
        const auto global = [&](std::size_t i) {
            return std::to_integer<std::uint8_t>((*globalBytes)[i]);
        };
        const auto globalField = [](std::size_t i) {
            return "globals[" + std::to_string(i) + "]";
        };

        Globals& globals = _doc.header.globals;
        if (global(0) > 1) {
            return _Fatal(codes::TextInvalidEncodingFlag,
                "text encoding flag " + std::to_string(global(0))
                    + " is neither 0 (UTF-16LE) nor 1 (UTF-8)",
                globalField(0), globalsOffset);
        }
        globals.textEncoding = static_cast<TextEncoding>(global(0));

        if (global(1) > 4) {
            return _Fatal(codes::PmxInvalidGlobals,
                "additional vec4 count " + std::to_string(global(1)) + " is outside 0-4",
                globalField(1), globalsOffset + 1);
        }
        globals.additionalVec4Count = global(1);

        std::uint8_t* const indexSizes[] = {
            &globals.vertexIndexSize,   &globals.textureIndexSize,
            &globals.materialIndexSize, &globals.boneIndexSize,
            &globals.morphIndexSize,    &globals.rigidBodyIndexSize,
        };
        for (std::size_t k = 0; k < kIndexSizeNames.size(); ++k) {
            const std::size_t i = 2 + k;
            const std::uint8_t size = global(i);
            if (size != 1 && size != 2 && size != 4) {
                return _Fatal(codes::PmxInvalidIndexSize,
                    std::string(kIndexSizeNames[k]) + " index size " + std::to_string(size)
                        + " is not 1, 2 or 4",
                    globalField(i), globalsOffset + i);
            }
            *indexSizes[k] = size;
        }

        if (count > kDefinedGlobals) {
            for (std::size_t i = kDefinedGlobals; i < count; ++i) {
                globals.unknown.push_back(global(i));
            }
            _diagnostics.Add(codes::PmxUnknownGlobals,
                "the header declares " + std::to_string(count)
                    + " globals; those beyond the 8 PMX defines are preserved and ignored",
                _Here(globalField(kDefinedGlobals), globalsOffset + kDefinedGlobals));
        }
        return true;
    }

    bool _ReadModelInfo()
    {
        _Enter("model");
        ModelInfo& m = _doc.model;
        return _Text(m.name, "name") && _Text(m.englishName, "englishName")
            && _Text(m.comment, "comment") && _Text(m.englishComment, "englishComment");
    }

    // --- vertices (PMX_CONTRACT.md §5) ------------------------------------------

    bool _ReadVertices()
    {
        _Enter("vertices");
        const Globals& g = _doc.header.globals;
        // position, normal, uv, the additional vec4s, the deform type, the
        // smallest deform (BDEF1: one bone index), the edge scale.
        const std::size_t minimum =
            12 + 12 + 8 + 16 * std::size_t{g.additionalVec4Count} + 1 + g.boneIndexSize + 4;
        std::size_t count = 0;
        if (!_Count(count, minimum, "count")) {
            return false;
        }
        _doc.vertices.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            _At(i);
            Vertex& v = _doc.vertices[i];
            if (!_Floats(v.position, "position") || !_Floats(v.normal, "normal")
                || !_Floats(v.uv, "uv")) {
                return false;
            }
            for (std::size_t k = 0; k < g.additionalVec4Count; ++k) {
                if (!_Floats(v.additionalVec4[k], kAdditionalVec4Fields[k])) {
                    return false;
                }
            }
            if (!_ReadDeform(v.deform) || !_F32(v.edgeScale, "edgeScale")) {
                return false;
            }
        }
        return true;
    }

    bool _ReadDeform(Deform& d)
    {
        const std::size_t typeOffset = _in.offset();
        std::uint8_t type = 0;
        if (!_U8(type, "deform.type")) {
            return false;
        }
        const bool v21 = _doc.header.version == Version::V2_1;
        if (type > 4 || (type == 4 && !v21)) {
            return _Fatal(codes::PmxInvalidDeformType,
                "deform type " + std::to_string(type)
                    + (type == 4 ? " (QDEF) is PMX 2.1 only, and this is PMX 2.0"
                                 : " is not one PMX defines")
                    + "; the vertex's length is unknown",
                "deform.type", typeOffset);
        }
        d.type = static_cast<DeformType>(type);

        const auto bones = [&](std::size_t n) {
            for (std::size_t k = 0; k < n; ++k) {
                if (!_Index(IndexKind::Bone, d.bones[k], kDeformBoneFields[k])) {
                    return false;
                }
            }
            return true;
        };
        switch (d.type) {
        case DeformType::Bdef1:
            return bones(1);
        case DeformType::Bdef2:
            return bones(2) && _F32(d.weights[0], kDeformWeightFields[0]);
        case DeformType::Bdef4:
        case DeformType::Qdef:
            if (!bones(4)) {
                return false;
            }
            for (std::size_t k = 0; k < 4; ++k) {
                if (!_F32(d.weights[k], kDeformWeightFields[k])) {
                    return false;
                }
            }
            return true;
        case DeformType::Sdef:
            return bones(2) && _F32(d.weights[0], kDeformWeightFields[0])
                && _Floats(d.sdefC, "deform.sdefC") && _Floats(d.sdefR0, "deform.sdefR0")
                && _Floats(d.sdefR1, "deform.sdefR1");
        }
        return true;
    }

    // --- faces (PMX_CONTRACT.md §6) ---------------------------------------------

    bool _ReadFaces()
    {
        _Enter("faces");
        const std::size_t countOffset = _in.offset();
        std::size_t count = 0;
        if (!_Count(count, _doc.header.globals.vertexIndexSize, "count")) {
            return false;
        }
        if (count % 3 != 0) {
            return _Fatal(codes::PmxFaceCountNotTriangles,
                "the face table holds " + Plural(count, "vertex indices")
                    + ", which is not a whole number of triangles",
                "count", countOffset);
        }
        _doc.faces.resize(count);
        const std::size_t vertexCount = _doc.vertices.size();
        for (std::size_t i = 0; i < count; ++i) {
            _At(i);
            const std::size_t start = _in.offset();
            std::int32_t v = 0;
            if (!_Index(IndexKind::Vertex, v, "")) {
                return false;
            }
            // Fatal rather than dropped: dropping a face would shift every
            // material's face range after it.
            if (VertexOutOfRange(v, vertexCount)) {
                return _Fatal(codes::PmxFaceIndexOutOfRange,
                    "the face index is vertex " + std::to_string(v) + ", but there are "
                        + Plural(vertexCount, "vertices"),
                    "", start);
            }
            _doc.faces[i] = static_cast<std::uint32_t>(v);
        }
        return true;
    }

    // --- textures (PMX_CONTRACT.md §7) --------------------------------------------

    bool _ReadTextures()
    {
        _Enter("textures");
        std::size_t count = 0;
        if (!_Count(count, 4, "count")) {
            return false;
        }
        _doc.textures.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            _At(i);
            if (!_Text(_doc.textures[i], "")) {
                return false;
            }
        }
        return true;
    }

    // --- materials (PMX_CONTRACT.md §8) -------------------------------------------

    bool _ReadMaterials()
    {
        _Enter("materials");
        const std::size_t t = _doc.header.globals.textureIndexSize;
        // names; diffuse, specular, specular power, ambient, flags, edge
        // color, edge size; texture and sphere indices, sphere mode, toon
        // reference, the smaller toon value (a shared slot, one byte); memo,
        // face count. Spelled out rather than summed: a hand-summed constant
        // here once overstated the minimum and refused valid files.
        const std::size_t minimum = 8 + (16 + 12 + 4 + 12 + 1 + 16 + 4) + 2 * t + 1 + 1 + 1
            + 4 + 4;
        std::size_t count = 0;
        if (!_Count(count, minimum, "count")) {
            return false;
        }
        _doc.materials.resize(count);
        const std::size_t faceIndices = _doc.faces.size();
        std::size_t consumed = 0;
        for (std::size_t i = 0; i < count; ++i) {
            _At(i);
            Material& m = _doc.materials[i];
            if (!_Text(m.name, "name") || !_Text(m.englishName, "englishName")
                || !_Floats(m.diffuse, "diffuse") || !_Floats(m.specular, "specular")
                || !_F32(m.specularPower, "specularPower") || !_Floats(m.ambient, "ambient")
                || !_U8(m.flags, "flags") || !_Floats(m.edgeColor, "edgeColor")
                || !_F32(m.edgeSize, "edgeSize")
                || !_Index(IndexKind::Texture, m.texture, "texture")
                || !_Index(IndexKind::Texture, m.sphereTexture, "sphereTexture")
                || !_U8(m.sphereMode, "sphereMode")) {
                return false;
            }

            const std::size_t toonOffset = _in.offset();
            std::uint8_t toon = 0;
            if (!_U8(toon, "toonReference")) {
                return false;
            }
            if (toon == 0) {
                m.toonReference = ToonReference::Texture;
                if (!_Index(IndexKind::Texture, m.toonTexture, "toonTexture")) {
                    return false;
                }
            } else if (toon == 1) {
                m.toonReference = ToonReference::Shared;
                if (!_U8(m.sharedToon, "sharedToon")) {
                    return false;
                }
            } else {
                return _Fatal(codes::PmxInvalidLayoutFlag,
                    "toon reference " + std::to_string(toon)
                        + " is neither 0 (a texture index follows) nor 1 (a shared toon slot "
                          "follows); the material's length is unknown",
                    "toonReference", toonOffset);
            }

            if (!_Text(m.memo, "memo")) {
                return false;
            }
            const std::size_t countAt = _in.offset();
            if (!_I32(m.faceCount, "faceCount")) {
                return false;
            }
            const std::size_t left = faceIndices - consumed;
            if (m.faceCount < 0 || m.faceCount % 3 != 0
                || static_cast<std::size_t>(m.faceCount) > left) {
                return _Fatal(codes::PmxMaterialFacesExceedTable,
                    "the material draws " + std::to_string(m.faceCount) + " face indices"
                        + (m.faceCount < 0 || m.faceCount % 3 != 0
                                  ? ", which is not a whole number of triangles"
                                  : ", but only " + Plural(left, "face indices")
                                        + " are left in the face table"),
                    "faceCount", countAt);
            }
            consumed += static_cast<std::size_t>(m.faceCount);
        }
        if (consumed < faceIndices) {
            Location where;
            where.table = "materials";
            _diagnostics.Add(codes::PmxMaterialFacesShort,
                "the materials draw " + std::to_string(consumed) + " of "
                    + Plural(faceIndices, "face indices") + "; the last "
                    + std::to_string(faceIndices - consumed) + " are drawn by no material",
                std::move(where));
        }
        return true;
    }

    // --- bones (PMX_CONTRACT.md §9) -----------------------------------------------

    bool _ReadBones()
    {
        _Enter("bones");
        const std::size_t b = _doc.header.globals.boneIndexSize;
        // names, position, parent, transform layer, flags, and the smaller
        // tail (a bone index: at most 4 bytes, against a 12-byte offset).
        const std::size_t minimum = 8 + 12 + b + 4 + 2 + b;
        std::size_t count = 0;
        if (!_Count(count, minimum, "count")) {
            return false;
        }
        _doc.bones.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            _At(i);
            if (!_ReadBone(_doc.bones[i])) {
                return false;
            }
        }
        return true;
    }

    bool _ReadBone(Bone& bone)
    {
        if (!_Text(bone.name, "name") || !_Text(bone.englishName, "englishName")
            || !_Floats(bone.position, "position")
            || !_Index(IndexKind::Bone, bone.parent, "parent")
            || !_I32(bone.transformLayer, "transformLayer") || !_U16(bone.flags, "flags")) {
            return false;
        }
        const std::uint16_t f = bone.flags;
        if (f & BoneFlag::TailIsBone) {
            if (!_Index(IndexKind::Bone, bone.tailBone, "tailBone")) {
                return false;
            }
        } else if (!_Floats(bone.tailOffset, "tailOffset")) {
            return false;
        }
        if ((f & (BoneFlag::AppendRotation | BoneFlag::AppendTranslation))
            && (!_Index(IndexKind::Bone, bone.appendParent, "appendParent")
                || !_F32(bone.appendRatio, "appendRatio"))) {
            return false;
        }
        if ((f & BoneFlag::FixedAxis) && !_Floats(bone.fixedAxis, "fixedAxis")) {
            return false;
        }
        if ((f & BoneFlag::LocalAxes)
            && (!_Floats(bone.localAxisX, "localAxisX") || !_Floats(bone.localAxisZ, "localAxisZ"))) {
            return false;
        }
        if ((f & BoneFlag::ExternalParent)
            && !_I32(bone.externalParentKey, "externalParentKey")) {
            return false;
        }
        if (!(f & BoneFlag::Ik)) {
            return true;
        }

        Ik& ik = bone.ik;
        std::size_t links = 0;
        if (!_Index(IndexKind::Bone, ik.target, "ik.target")
            || !_I32(ik.loopCount, "ik.loopCount") || !_F32(ik.limitAngle, "ik.limitAngle")
            || !_Count(links, _doc.header.globals.boneIndexSize + 1, "ik.linkCount")) {
            return false;
        }
        ik.links.resize(links);
        for (std::size_t k = 0; k < links; ++k) {
            _AtSub("ik.links", k);
            IkLink& link = ik.links[k];
            if (!_Index(IndexKind::Bone, link.bone, "bone")) {
                return false;
            }
            const std::size_t limitOffset = _in.offset();
            std::uint8_t hasLimits = 0;
            if (!_U8(hasLimits, "hasLimits")) {
                return false;
            }
            if (hasLimits > 1) {
                return _Fatal(codes::PmxInvalidLayoutFlag,
                    "the IK link's limit flag is " + std::to_string(hasLimits)
                        + ", neither 0 nor 1; the link's length is unknown",
                    "hasLimits", limitOffset);
            }
            link.hasLimits = hasLimits == 1;
            if (link.hasLimits
                && (!_Floats(link.lowerLimit, "lowerLimit")
                    || !_Floats(link.upperLimit, "upperLimit"))) {
                return false;
            }
        }
        return true;
    }

    // --- morphs (PMX_CONTRACT.md §10) ---------------------------------------------

    bool _ReadMorphs()
    {
        _Enter("morphs");
        std::size_t count = 0;
        if (!_Count(count, 8 + 1 + 1 + 4, "count")) {
            return false;
        }
        _doc.morphs.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            _At(i);
            if (!_ReadMorph(_doc.morphs[i])) {
                return false;
            }
        }
        return true;
    }

    template <class T, class ReadOne>
    bool _Offsets(std::vector<T>& out, std::size_t minimum, ReadOne readOne)
    {
        std::size_t count = 0;
        if (!_Count(count, minimum, "offsetCount")) {
            return false;
        }
        out.resize(count);
        for (std::size_t k = 0; k < count; ++k) {
            _AtSub("offsets", k);
            if (!readOne(out[k])) {
                return false;
            }
        }
        return true;
    }

    bool _ReadMorph(Morph& morph)
    {
        if (!_Text(morph.name, "name") || !_Text(morph.englishName, "englishName")
            || !_U8(morph.panel, "panel")) {
            return false;
        }
        const std::size_t typeAt = _in.offset();
        std::uint8_t type = 0;
        if (!_U8(type, "type")) {
            return false;
        }
        const bool v21 = _doc.header.version == Version::V2_1;
        if (type > 10 || (type >= 9 && !v21)) {
            return _Fatal(codes::PmxInvalidMorphType,
                "morph type " + std::to_string(type)
                    + (type <= 10 ? " is PMX 2.1 only, and this is PMX 2.0"
                                  : " is not one PMX defines")
                    + "; the offsets' length is unknown",
                "type", typeAt);
        }
        morph.type = static_cast<MorphType>(type);

        const Globals& g = _doc.header.globals;
        switch (morph.type) {
        case MorphType::Group:
        case MorphType::Flip:
            return _Offsets(morph.groupOffsets, g.morphIndexSize + 4u, [&](GroupOffset& o) {
                return _Index(IndexKind::Morph, o.morph, "morph") && _F32(o.weight, "weight");
            });
        case MorphType::Vertex:
            return _Offsets(morph.vertexOffsets, g.vertexIndexSize + 12u, [&](VertexOffset& o) {
                return _Index(IndexKind::Vertex, o.vertex, "vertex")
                    && _Floats(o.displacement, "displacement");
            });
        case MorphType::Bone:
            return _Offsets(morph.boneOffsets, g.boneIndexSize + 28u, [&](BoneOffset& o) {
                return _Index(IndexKind::Bone, o.bone, "bone")
                    && _Floats(o.translation, "translation") && _Floats(o.rotation, "rotation");
            });
        case MorphType::Uv:
        case MorphType::AdditionalUv1:
        case MorphType::AdditionalUv2:
        case MorphType::AdditionalUv3:
        case MorphType::AdditionalUv4:
            return _Offsets(morph.uvOffsets, g.vertexIndexSize + 16u, [&](UvOffset& o) {
                return _Index(IndexKind::Vertex, o.vertex, "vertex") && _Floats(o.delta, "delta");
            });
        case MorphType::Material:
            return _Offsets(morph.materialOffsets, g.materialIndexSize + 113u,
                [&](MaterialOffset& o) {
                    return _Index(IndexKind::Material, o.material, "material")
                        && _U8(o.operation, "operation") && _Floats(o.diffuse, "diffuse")
                        && _Floats(o.specular, "specular")
                        && _F32(o.specularPower, "specularPower")
                        && _Floats(o.ambient, "ambient") && _Floats(o.edgeColor, "edgeColor")
                        && _F32(o.edgeSize, "edgeSize") && _Floats(o.textureTint, "textureTint")
                        && _Floats(o.sphereTint, "sphereTint") && _Floats(o.toonTint, "toonTint");
                });
        case MorphType::Impulse:
            return _Offsets(morph.impulseOffsets, g.rigidBodyIndexSize + 25u,
                [&](ImpulseOffset& o) {
                    return _Index(IndexKind::RigidBody, o.rigidBody, "rigidBody")
                        && _U8(o.local, "local") && _Floats(o.velocity, "velocity")
                        && _Floats(o.torque, "torque");
                });
        }
        return true;
    }

    // --- display frames (PMX_CONTRACT.md §11) ----------------------------------------

    bool _ReadDisplayFrames()
    {
        _Enter("displayFrames");
        std::size_t count = 0;
        if (!_Count(count, 8 + 1 + 4, "count")) {
            return false;
        }
        const Globals& g = _doc.header.globals;
        const std::size_t minimumElement = 1 + std::min(g.boneIndexSize, g.morphIndexSize);
        _doc.displayFrames.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            _At(i);
            DisplayFrame& frame = _doc.displayFrames[i];
            std::size_t elements = 0;
            if (!_Text(frame.name, "name") || !_Text(frame.englishName, "englishName")
                || !_U8(frame.special, "special")
                || !_Count(elements, minimumElement, "elementCount")) {
                return false;
            }
            frame.elements.resize(elements);
            for (std::size_t k = 0; k < elements; ++k) {
                _AtSub("elements", k);
                FrameElement& element = frame.elements[k];
                const std::size_t kindOffset = _in.offset();
                std::uint8_t kind = 0;
                if (!_U8(kind, "kind")) {
                    return false;
                }
                if (kind > 1) {
                    return _Fatal(codes::PmxInvalidLayoutFlag,
                        "display-frame element kind " + std::to_string(kind)
                            + " is neither 0 (a bone) nor 1 (a morph); the element's "
                              "length is unknown",
                        "kind", kindOffset);
                }
                element.kind = static_cast<FrameElementKind>(kind);
                if (!_Index(kind == 0 ? IndexKind::Bone : IndexKind::Morph, element.index,
                        "index")) {
                    return false;
                }
            }
        }
        return true;
    }

    // --- rigid bodies and joints (PMX_CONTRACT.md §13) ---------------------------------

    bool _ReadRigidBodies()
    {
        _Enter("rigidBodies");
        const std::size_t minimum = 8 + _doc.header.globals.boneIndexSize + 1 + 2 + 1 + 36 + 20 + 1;
        std::size_t count = 0;
        if (!_Count(count, minimum, "count")) {
            return false;
        }
        _doc.rigidBodies.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            _At(i);
            RigidBody& r = _doc.rigidBodies[i];
            if (!_Text(r.name, "name") || !_Text(r.englishName, "englishName")
                || !_Index(IndexKind::Bone, r.bone, "bone") || !_U8(r.group, "group")
                || !_U16(r.nonCollisionMask, "nonCollisionMask") || !_U8(r.shape, "shape")
                || !_Floats(r.size, "size") || !_Floats(r.position, "position")
                || !_Floats(r.rotation, "rotation") || !_F32(r.mass, "mass")
                || !_F32(r.linearDamping, "linearDamping")
                || !_F32(r.angularDamping, "angularDamping")
                || !_F32(r.restitution, "restitution") || !_F32(r.friction, "friction")
                || !_U8(r.physicsMode, "physicsMode")) {
                return false;
            }
        }
        return true;
    }

    bool _ReadJoints()
    {
        _Enter("joints");
        const std::size_t minimum = 8 + 1 + 2 * std::size_t{_doc.header.globals.rigidBodyIndexSize} + 96;
        std::size_t count = 0;
        if (!_Count(count, minimum, "count")) {
            return false;
        }
        _doc.joints.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            _At(i);
            Joint& j = _doc.joints[i];
            if (!_Text(j.name, "name") || !_Text(j.englishName, "englishName")
                || !_U8(j.type, "type")
                || !_Index(IndexKind::RigidBody, j.rigidBodyA, "rigidBodyA")
                || !_Index(IndexKind::RigidBody, j.rigidBodyB, "rigidBodyB")
                || !_Floats(j.position, "position") || !_Floats(j.rotation, "rotation")
                || !_Floats(j.translationMin, "translationMin")
                || !_Floats(j.translationMax, "translationMax")
                || !_Floats(j.rotationMin, "rotationMin")
                || !_Floats(j.rotationMax, "rotationMax")
                || !_Floats(j.translationSpring, "translationSpring")
                || !_Floats(j.rotationSpring, "rotationSpring")) {
                return false;
            }
        }
        return true;
    }

    // --- soft bodies (PMX_CONTRACT.md §12) ------------------------------------------

    bool _ReadSoftBodies()
    {
        if (_doc.header.version != Version::V2_1) {
            return true;
        }
        _Enter("softBodies");
        const Globals& g = _doc.header.globals;
        // names, shape, material, group, mask, flags, the five scalars after
        // them, the 25 coefficients and iteration counts, the two list counts.
        const std::size_t minimum = 8 + 1 + g.materialIndexSize + 1 + 2 + 1 + 20 + 100 + 4 + 4;
        std::size_t count = 0;
        if (!_Count(count, minimum, "count")) {
            return false;
        }
        _doc.softBodies.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            _At(i);
            SoftBody& s = _doc.softBodies[i];
            std::size_t anchors = 0;
            if (!_Text(s.name, "name") || !_Text(s.englishName, "englishName")
                || !_U8(s.shape, "shape") || !_Index(IndexKind::Material, s.material, "material")
                || !_U8(s.group, "group") || !_U16(s.nonCollisionMask, "nonCollisionMask")
                || !_U8(s.flags, "flags") || !_I32(s.bLinkDistance, "bLinkDistance")
                || !_I32(s.clusterCount, "clusterCount") || !_F32(s.totalMass, "totalMass")
                || !_F32(s.collisionMargin, "collisionMargin")
                || !_I32(s.aeroModel, "aeroModel") || !_Floats(s.config, "config")
                || !_Floats(s.cluster, "cluster") || !_Ints(s.iteration, "iteration")
                || !_Floats(s.materialCoefficients, "materialCoefficients")
                || !_Count(anchors, std::size_t{g.rigidBodyIndexSize} + g.vertexIndexSize + 1,
                    "anchorCount")) {
                return false;
            }
            s.anchors.resize(anchors);
            for (std::size_t k = 0; k < anchors; ++k) {
                _AtSub("anchors", k);
                SoftBodyAnchor& a = s.anchors[k];
                if (!_Index(IndexKind::RigidBody, a.rigidBody, "rigidBody")
                    || !_Index(IndexKind::Vertex, a.vertex, "vertex")
                    || !_U8(a.nearMode, "nearMode")) {
                    return false;
                }
            }
            _At(i);
            std::size_t pins = 0;
            if (!_Count(pins, g.vertexIndexSize, "pinCount")) {
                return false;
            }
            s.pinVertices.resize(pins);
            for (std::size_t k = 0; k < pins; ++k) {
                _AtSub("pinVertices", k);
                if (!_Index(IndexKind::Vertex, s.pinVertices[k], "")) {
                    return false;
                }
            }
        }
        return true;
    }

    // --- the end (PMX_CONTRACT.md §2) ------------------------------------------------

    bool _ReadEnd()
    {
        if (_in.remaining() > 0) {
            Location where;
            where.byteOffset = _in.offset();
            _diagnostics.Add(codes::PmxTrailingBytes,
                Plural(_in.remaining(), "bytes") + " follow the "
                    + (_doc.header.version == Version::V2_1 ? "soft-body" : "joint")
                    + " table, the last PMX " + std::string(ToString(_doc.header.version))
                    + " defines; they are ignored",
                std::move(where));
        }
        return true;
    }

    // --- index validation (PMX_CONTRACT.md §4) ------------------------------------------
    //
    // After every table has been read, so a forward reference is legal. An
    // out-of-range index becomes kNoIndex, with a diagnostic; what "none"
    // means for each relation is canonicalization's to apply.

    void _Reject(std::int32_t& index, std::size_t size, std::string_view target,
        std::string_view table, std::size_t element, std::string field)
    {
        Location where;
        where.table = std::string(table);
        where.index = element;
        where.field = std::move(field);
        std::string message = "the index is " + std::to_string(index) + ", but the "
            + std::string(target) + " table holds " + std::to_string(size)
            + "; it is read as none";
        _diagnostics.Add(codes::PmxIndexOutOfRange, std::move(message), std::move(where));
        index = kNoIndex;
    }

    void _Check(std::int32_t& index, std::size_t size, std::string_view target,
        std::string_view table, std::size_t element, std::string_view field)
    {
        if (OutOfRange(index, size)) {
            _Reject(index, size, target, table, element, std::string(field));
        }
    }

    void _RejectVertex(std::int32_t& index, std::string_view table, std::size_t element,
        std::string field)
    {
        _Reject(index, _doc.vertices.size(), "vertices", table, element, std::move(field));
    }

    static std::string _Sub(std::string_view list, std::size_t k, std::string_view field)
    {
        std::string out = std::string(list) + "[" + std::to_string(k) + "]";
        if (!field.empty()) {
            out += ".";
            out += field;
        }
        return out;
    }

    void _Validate()
    {
        const std::size_t textures = _doc.textures.size();
        const std::size_t materials = _doc.materials.size();
        const std::size_t bones = _doc.bones.size();
        const std::size_t morphs = _doc.morphs.size();
        const std::size_t rigidBodies = _doc.rigidBodies.size();

        // A deform influence of none is accepted only with weight 0
        // (PMX_CONTRACT.md §5).
        for (std::size_t i = 0; i < _doc.vertices.size(); ++i) {
            Deform& d = _doc.vertices[i].deform;
            const std::size_t used = d.type == DeformType::Bdef1 ? 1
                : (d.type == DeformType::Bdef2 || d.type == DeformType::Sdef) ? 2
                                                                              : 4;
            for (std::size_t k = 0; k < used; ++k) {
                const float weight = d.type == DeformType::Bdef1 ? 1.0f
                    : used == 2 ? (k == 0 ? d.weights[0] : 1.0f - d.weights[0])
                                : d.weights[k];
                std::int32_t& bone = d.bones[k];
                if (bone == kNoIndex) {
                    if (weight != 0.0f) {
                        Location where;
                        where.table = "vertices";
                        where.index = i;
                        where.field = std::string(kDeformBoneFields[k]);
                        _diagnostics.Add(codes::PmxIndexOutOfRange,
                            "the influence names no bone, but its weight is "
                                + FloatText(weight),
                            std::move(where));
                    }
                } else {
                    _Check(bone, bones, "bones", "vertices", i, kDeformBoneFields[k]);
                }
            }
        }

        for (std::size_t i = 0; i < materials; ++i) {
            Material& m = _doc.materials[i];
            _Check(m.texture, textures, "textures", "materials", i, "texture");
            _Check(m.sphereTexture, textures, "textures", "materials", i, "sphereTexture");
            _Check(m.toonTexture, textures, "textures", "materials", i, "toonTexture");
        }

        for (std::size_t i = 0; i < bones; ++i) {
            Bone& b = _doc.bones[i];
            _Check(b.parent, bones, "bones", "bones", i, "parent");
            _Check(b.tailBone, bones, "bones", "bones", i, "tailBone");
            _Check(b.appendParent, bones, "bones", "bones", i, "appendParent");
            _Check(b.ik.target, bones, "bones", "bones", i, "ik.target");
            for (std::size_t k = 0; k < b.ik.links.size(); ++k) {
                std::int32_t& link = b.ik.links[k].bone;
                if (OutOfRange(link, bones)) {
                    _Reject(link, bones, "bones", "bones", i, _Sub("ik.links", k, "bone"));
                }
            }
        }

        for (std::size_t i = 0; i < morphs; ++i) {
            Morph& m = _doc.morphs[i];
            for (std::size_t k = 0; k < m.groupOffsets.size(); ++k) {
                std::int32_t& target = m.groupOffsets[k].morph;
                if (OutOfRange(target, morphs)) {
                    _Reject(target, morphs, "morphs", "morphs", i, _Sub("offsets", k, "morph"));
                }
            }
            for (std::size_t k = 0; k < m.vertexOffsets.size(); ++k) {
                std::int32_t& target = m.vertexOffsets[k].vertex;
                if (VertexOutOfRange(target, _doc.vertices.size())) {
                    _RejectVertex(target, "morphs", i, _Sub("offsets", k, "vertex"));
                }
            }
            for (std::size_t k = 0; k < m.boneOffsets.size(); ++k) {
                std::int32_t& target = m.boneOffsets[k].bone;
                if (OutOfRange(target, bones)) {
                    _Reject(target, bones, "bones", "morphs", i, _Sub("offsets", k, "bone"));
                }
            }
            for (std::size_t k = 0; k < m.uvOffsets.size(); ++k) {
                std::int32_t& target = m.uvOffsets[k].vertex;
                if (VertexOutOfRange(target, _doc.vertices.size())) {
                    _RejectVertex(target, "morphs", i, _Sub("offsets", k, "vertex"));
                }
            }
            for (std::size_t k = 0; k < m.materialOffsets.size(); ++k) {
                std::int32_t& target = m.materialOffsets[k].material;
                if (OutOfRange(target, materials)) {
                    _Reject(target, materials, "materials", "morphs", i,
                        _Sub("offsets", k, "material"));
                }
            }
            for (std::size_t k = 0; k < m.impulseOffsets.size(); ++k) {
                std::int32_t& target = m.impulseOffsets[k].rigidBody;
                if (OutOfRange(target, rigidBodies)) {
                    _Reject(target, rigidBodies, "rigidBodies", "morphs", i,
                        _Sub("offsets", k, "rigidBody"));
                }
            }
        }

        for (std::size_t i = 0; i < _doc.displayFrames.size(); ++i) {
            DisplayFrame& frame = _doc.displayFrames[i];
            for (std::size_t k = 0; k < frame.elements.size(); ++k) {
                FrameElement& e = frame.elements[k];
                const bool bone = e.kind == FrameElementKind::Bone;
                const std::size_t size = bone ? bones : morphs;
                if (OutOfRange(e.index, size)) {
                    _Reject(e.index, size, bone ? "bones" : "morphs", "displayFrames", i,
                        _Sub("elements", k, "index"));
                }
            }
        }

        for (std::size_t i = 0; i < rigidBodies; ++i) {
            _Check(_doc.rigidBodies[i].bone, bones, "bones", "rigidBodies", i, "bone");
        }

        for (std::size_t i = 0; i < _doc.joints.size(); ++i) {
            Joint& j = _doc.joints[i];
            _Check(j.rigidBodyA, rigidBodies, "rigidBodies", "joints", i, "rigidBodyA");
            _Check(j.rigidBodyB, rigidBodies, "rigidBodies", "joints", i, "rigidBodyB");
        }

        for (std::size_t i = 0; i < _doc.softBodies.size(); ++i) {
            SoftBody& s = _doc.softBodies[i];
            _Check(s.material, materials, "materials", "softBodies", i, "material");
            for (std::size_t k = 0; k < s.anchors.size(); ++k) {
                SoftBodyAnchor& a = s.anchors[k];
                if (OutOfRange(a.rigidBody, rigidBodies)) {
                    _Reject(a.rigidBody, rigidBodies, "rigidBodies", "softBodies", i,
                        _Sub("anchors", k, "rigidBody"));
                }
                if (VertexOutOfRange(a.vertex, _doc.vertices.size())) {
                    _RejectVertex(a.vertex, "softBodies", i, _Sub("anchors", k, "vertex"));
                }
            }
            for (std::size_t k = 0; k < s.pinVertices.size(); ++k) {
                if (VertexOutOfRange(s.pinVertices[k], _doc.vertices.size())) {
                    _RejectVertex(s.pinVertices[k], "softBodies", i, _Sub("pinVertices", k, ""));
                }
            }
        }
    }

    const std::size_t _size;
    detail::ByteReader _in;
    Document _doc;
    detail::DiagnosticList _diagnostics;
    std::optional<Diagnostic> _fatal;

    std::string_view _table;
    std::optional<std::size_t> _index;
    std::string_view _sub;
    std::optional<std::size_t> _subIndex;
};

std::string
PathText(const std::filesystem::path& path)
{
    const std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
}

}  // namespace

Result<Document>
Read(std::span<const std::byte> bytes)
{
    return Parser(bytes).Run();
}

Result<Document>
ReadFile(const std::filesystem::path& path)
{
    const auto unreadable = [&](const std::string& why) {
        return Result<Document>::Failure(MakeDiagnostic(codes::PmxFileUnreadable,
            "'" + PathText(path) + "' " + why));
    };

    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) {
        return unreadable(error ? "could not be opened: " + error.message()
                                : "is not a regular file");
    }
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error) {
        return unreadable("could not be sized: " + error.message());
    }
    if (size > std::numeric_limits<std::streamsize>::max()
        || size > std::numeric_limits<std::size_t>::max()) {
        return unreadable("is too large to read into memory");
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return unreadable("could not be opened");
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    if (size > 0
        && !in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size))) {
        return unreadable("could not be read in full");
    }
    return Read(std::span<const std::byte>(bytes));
}

}  // namespace mmd::pmx
