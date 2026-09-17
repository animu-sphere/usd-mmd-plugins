// SPDX-License-Identifier: Apache-2.0
#include "motionVmd/Reader.h"

#include "motionVmd/Codes.h"
#include "motionVmd/Cp932.h"

#include "DiagnosticList.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace motionVmd {

namespace {

constexpr std::size_t kSignatureSize = 30;
constexpr std::string_view kSignatureV1 = "Vocaloid Motion Data file";
constexpr std::string_view kSignatureV2 = "Vocaloid Motion Data 0002";

constexpr std::size_t kBoneNameSize = 15;
constexpr std::size_t kMorphNameSize = 15;
constexpr std::size_t kIkNameSize = 20;

// Record sizes (MOTION_CONTRACT.md §3). The IK record is variable; 9 bytes is
// its minimum, an empty IK list.
constexpr std::size_t kBoneRecord = kBoneNameSize + 4 + 12 + 16 + 64;
constexpr std::size_t kMorphRecord = kMorphNameSize + 4 + 4;
constexpr std::size_t kCameraRecord = 4 + 4 + 12 + 12 + 24 + 4 + 1;
constexpr std::size_t kLightRecord = 4 + 12 + 12;
constexpr std::size_t kSelfShadowRecord = 4 + 1 + 4;
constexpr std::size_t kIkRecordMinimum = 4 + 1 + 4;
constexpr std::size_t kIkStateRecord = kIkNameSize + 1;

std::string
Bytes(std::uint64_t n)
{
    return std::to_string(n) + (n == 1 ? " byte" : " bytes");
}

/// One pass over the bytes, in file order. Every _Read* returns false after
/// recording the fatal diagnostic that stopped it.
class Parser {
public:
    explicit Parser(std::span<const std::byte> bytes) : _bytes(bytes) {}

    Result<Document> Run()
    {
        if (!_ReadHeader() || !_ReadSections()) {
            return Result<Document>::Failure(std::move(*_fatal), _diagnostics.Take());
        }
        if (_Remaining() != 0) {
            _diagnostics.Add(codes::MotionTrailingBytes,
                             Bytes(_Remaining()) + " follow the last section",
                             Location{_offset, {}, std::nullopt, {}});
        }
        return Result<Document>::Success(std::move(_doc), _diagnostics.Take());
    }

private:
    std::size_t _Remaining() const { return _bytes.size() - _offset; }

    // --- where we are -------------------------------------------------------

    Location _Here(std::string_view field, std::size_t offset) const
    {
        Location where;
        where.byteOffset = offset;
        where.section = std::string(_section);
        where.index = _index;
        where.field = std::string(field);
        return where;
    }

    bool _Fatal(const Code& code, std::string message, std::string_view field, std::size_t offset)
    {
        _fatal = MakeDiagnostic(code, std::move(message), _Here(field, offset));
        return false;
    }

    bool _Truncated(std::string_view field)
    {
        return _Fatal(codes::MotionTruncatedBuffer,
                      "the file ends before this field does (the file is " + Bytes(_bytes.size()) +
                          ")",
                      field,
                      _offset);
    }

    // --- primitives: the caller has checked that the bytes are there --------

    std::uint8_t _U8() { return std::to_integer<std::uint8_t>(_bytes[_offset++]); }

    std::uint32_t _U32()
    {
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
            v |= std::to_integer<std::uint32_t>(_bytes[_offset + i]) << (8 * i);
        }
        _offset += 4;
        return v;
    }

    float _F32()
    {
        static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
        return std::bit_cast<float>(_U32());
    }

    template <std::size_t N> void _Floats(std::array<float, N>& out)
    {
        for (float& value : out) {
            value = _F32();
        }
    }

    template <std::size_t N> void _Raw(std::array<std::uint8_t, N>& out)
    {
        for (std::uint8_t& value : out) {
            value = _U8();
        }
    }

    /// A fixed-length CP932 name field of `size` bytes (MOTION_CONTRACT.md §4).
    Name _Name(std::size_t size, std::string_view field)
    {
        const std::size_t start = _offset;
        const std::span<const std::byte> whole = _bytes.subspan(start, size);
        _offset += size;

        const auto nul = std::find(whole.begin(), whole.end(), std::byte{0});
        const std::span<const std::byte> content =
            whole.first(static_cast<std::size_t>(nul - whole.begin()));

        Name name;
        name.bytes.assign(reinterpret_cast<const char*>(content.data()), content.size());
        Cp932Decoded decoded = DecodeCp932(content);
        if (decoded.invalidAt) {
            _diagnostics.Add(codes::TextInvalidCp932,
                             "the bytes at offset " + std::to_string(*decoded.invalidAt) +
                                 " of the name are not a CP932 character; the name is empty",
                             _Here(field, start + *decoded.invalidAt));
        } else if (decoded.truncated) {
            _diagnostics.Add(codes::TextTruncatedCp932,
                             "the name ends in half a two-byte character, which is dropped",
                             _Here(field, start + content.size() - 1));
        }
        name.text = std::move(decoded.text);
        return name;
    }

    /// A section's count, bounded by the records the rest of the file can hold.
    bool _Count(std::size_t recordSize, std::uint32_t& count)
    {
        if (_Remaining() < 4) {
            return _Truncated("count");
        }
        const std::size_t at = _offset;
        count = _U32();
        if (static_cast<std::uint64_t>(count) * recordSize > _Remaining()) {
            return _Fatal(codes::MotionCountExceedsBuffer,
                          std::to_string(count) + " records of at least " + Bytes(recordSize) +
                              " do not fit in the " + Bytes(_Remaining()) + " that remain",
                          "count",
                          at);
        }
        return true;
    }

    // --- the file -----------------------------------------------------------

    bool _ReadHeader()
    {
        _section = "header";
        const std::size_t available = std::min(_Remaining(), kSignatureSize);
        const std::span<const std::byte> field = _bytes.first(available);
        const auto nul = std::find(field.begin(), field.end(), std::byte{0});
        const std::string_view signature(reinterpret_cast<const char*>(field.data()),
                                         static_cast<std::size_t>(nul - field.begin()));
        const auto matches = [&](std::string_view known) {
            return available == kSignatureSize ? signature == known : known.starts_with(signature);
        };
        std::size_t nameSize = 0;
        if (matches(kSignatureV2)) {
            _doc.header.version = Version::V2;
            nameSize = 20;
        } else if (matches(kSignatureV1)) {
            _doc.header.version = Version::V1;
            nameSize = 10;
        } else {
            return _Fatal(codes::MotionBadSignature,
                          "the file does not start with a VMD signature",
                          "signature",
                          0);
        }
        if (available < kSignatureSize) {
            return _Truncated("signature");
        }
        _offset = kSignatureSize;
        if (_Remaining() < nameSize) {
            return _Truncated("modelName");
        }
        _doc.header.modelName = _Name(nameSize, "modelName");
        return true;
    }

    bool _ReadSections()
    {
        for (std::size_t s = 0; s < kSectionCount; ++s) {
            // A file may end between two sections: the rest are empty.
            if (_Remaining() == 0) {
                return true;
            }
            const auto section = static_cast<Section>(s);
            _section = ToString(section);
            _index.reset();
            bool read = false;
            switch (section) {
            case Section::Bone:
                read = _ReadBones();
                break;
            case Section::Morph:
                read = _ReadMorphs();
                break;
            case Section::Camera:
                read = _ReadCameras();
                break;
            case Section::Light:
                read = _ReadLights();
                break;
            case Section::SelfShadow:
                read = _ReadSelfShadows();
                break;
            case Section::Ik:
                read = _ReadIk();
                break;
            }
            if (!read) {
                return false;
            }
            _doc.sectionsPresent = s + 1;
        }
        return true;
    }

    bool _ReadBones()
    {
        std::uint32_t count = 0;
        if (!_Count(kBoneRecord, count)) {
            return false;
        }
        _doc.boneKeyframes.resize(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            _index = i;
            BoneKeyframe& k = _doc.boneKeyframes[i];
            k.bone = _Name(kBoneNameSize, "bone");
            k.frame = _U32();
            _Floats(k.translation);
            _Floats(k.rotation);
            _Raw(k.interpolation);
        }
        return true;
    }

    bool _ReadMorphs()
    {
        std::uint32_t count = 0;
        if (!_Count(kMorphRecord, count)) {
            return false;
        }
        _doc.morphKeyframes.resize(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            _index = i;
            MorphKeyframe& k = _doc.morphKeyframes[i];
            k.morph = _Name(kMorphNameSize, "morph");
            k.frame = _U32();
            k.weight = _F32();
        }
        return true;
    }

    bool _ReadCameras()
    {
        std::uint32_t count = 0;
        if (!_Count(kCameraRecord, count)) {
            return false;
        }
        _doc.cameraKeyframes.resize(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            CameraKeyframe& k = _doc.cameraKeyframes[i];
            k.frame = _U32();
            k.distance = _F32();
            _Floats(k.position);
            _Floats(k.rotation);
            _Raw(k.interpolation);
            k.viewAngle = _U32();
            k.orthographic = _U8();
        }
        return true;
    }

    bool _ReadLights()
    {
        std::uint32_t count = 0;
        if (!_Count(kLightRecord, count)) {
            return false;
        }
        _doc.lightKeyframes.resize(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            LightKeyframe& k = _doc.lightKeyframes[i];
            k.frame = _U32();
            _Floats(k.color);
            _Floats(k.direction);
        }
        return true;
    }

    bool _ReadSelfShadows()
    {
        std::uint32_t count = 0;
        if (!_Count(kSelfShadowRecord, count)) {
            return false;
        }
        _doc.selfShadowKeyframes.resize(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            SelfShadowKeyframe& k = _doc.selfShadowKeyframes[i];
            k.frame = _U32();
            k.mode = _U8();
            k.distance = _F32();
        }
        return true;
    }

    bool _ReadIk()
    {
        std::uint32_t count = 0;
        if (!_Count(kIkRecordMinimum, count)) {
            return false;
        }
        _doc.ikKeyframes.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            _index = i;
            if (_Remaining() < kIkRecordMinimum) {
                return _Truncated("frame");
            }
            IkKeyframe& k = _doc.ikKeyframes.emplace_back();
            k.frame = _U32();
            k.visible = _U8();
            std::uint32_t states = 0;
            if (!_Count(kIkStateRecord, states)) {
                return false;
            }
            k.ik.resize(states);
            for (std::uint32_t j = 0; j < states; ++j) {
                const std::string field = "ik[" + std::to_string(j) + "].bone";
                k.ik[j].bone = _Name(kIkNameSize, field);
                k.ik[j].enabled = _U8();
            }
        }
        return true;
    }

    std::span<const std::byte> _bytes;
    std::size_t _offset = 0;
    std::string_view _section;
    std::optional<std::size_t> _index;

    Document _doc;
    detail::DiagnosticList _diagnostics;
    std::optional<Diagnostic> _fatal;
};

} // namespace

std::string_view
ToString(Version version)
{
    return version == Version::V1 ? "Vocaloid Motion Data file" : "Vocaloid Motion Data 0002";
}

std::string_view
ToString(Section section)
{
    switch (section) {
    case Section::Bone:
        return "boneKeyframes";
    case Section::Morph:
        return "morphKeyframes";
    case Section::Camera:
        return "cameraKeyframes";
    case Section::Light:
        return "lightKeyframes";
    case Section::SelfShadow:
        return "selfShadowKeyframes";
    case Section::Ik:
        return "ikKeyframes";
    }
    return "unknown";
}

Result<Document>
Read(std::span<const std::byte> bytes)
{
    return Parser(bytes).Run();
}

Result<Document>
ReadFile(const std::filesystem::path& path)
{
    const auto unreadable = [&](const std::string& why) {
        // On POSIX a path is bytes, and need not be UTF-8; nor need an OS
        // error message be.
        const std::u8string utf8 = path.u8string();
        const std::string text(reinterpret_cast<const char*>(utf8.data()), utf8.size());
        return Result<Document>::Failure(
            MakeDiagnostic(codes::MotionFileUnreadable,
                           "'" + ReplaceInvalidUtf8(text) + "' " + ReplaceInvalidUtf8(why)));
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
    if (size > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()) ||
        size > std::numeric_limits<std::size_t>::max()) {
        return unreadable("is too large to read into memory");
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return unreadable("could not be opened");
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    if (size > 0 &&
        !in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size))) {
        return unreadable("could not be read in full");
    }
    return Read(std::span<const std::byte>(bytes));
}

} // namespace motionVmd
