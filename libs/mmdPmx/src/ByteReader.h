// SPDX-License-Identifier: Apache-2.0
//
// The one cursor every PMX read goes through (PMX_CONTRACT.md §2): bounded,
// explicitly little-endian, and never handing out a raw pointer.
#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace mmd::pmx::detail {

class ByteReader {
public:
    explicit ByteReader(std::span<const std::byte> bytes) : _bytes(bytes) {}

    std::size_t offset() const noexcept { return _offset; }
    std::size_t remaining() const noexcept { return _bytes.size() - _offset; }

    /// The next `count` bytes, or nothing -- without advancing -- if fewer
    /// remain.
    std::optional<std::span<const std::byte>> Bytes(std::size_t count)
    {
        if (count > remaining()) {
            return std::nullopt;
        }
        const std::span<const std::byte> out = _bytes.subspan(_offset, count);
        _offset += count;
        return out;
    }

    std::optional<std::uint8_t> U8()
    {
        const auto b = Bytes(1);
        if (!b) {
            return std::nullopt;
        }
        return std::to_integer<std::uint8_t>((*b)[0]);
    }

    std::optional<std::uint32_t> U32()
    {
        const auto b = Bytes(4);
        if (!b) {
            return std::nullopt;
        }
        return std::to_integer<std::uint32_t>((*b)[0])
            | (std::to_integer<std::uint32_t>((*b)[1]) << 8)
            | (std::to_integer<std::uint32_t>((*b)[2]) << 16)
            | (std::to_integer<std::uint32_t>((*b)[3]) << 24);
    }

    /// IEEE-754 binary32, little-endian.
    std::optional<float> F32()
    {
        static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
        const auto bits = U32();
        if (!bits) {
            return std::nullopt;
        }
        return std::bit_cast<float>(*bits);
    }

private:
    std::span<const std::byte> _bytes;
    std::size_t _offset = 0;
};

}  // namespace mmd::pmx::detail
