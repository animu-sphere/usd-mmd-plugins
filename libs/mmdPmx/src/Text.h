// SPDX-License-Identifier: Apache-2.0
//
// PMX text decoding (TEXT_ENCODING_POLICY.md §3): UTF-16LE or UTF-8 in,
// validated UTF-8 out. Malformed text is rejected, never repaired -- there is
// no U+FFFD substitution -- and a valid string is preserved exactly: no
// trimming, no BOM stripping, no normalization, embedded U+0000 kept.
#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>

namespace mmd::pmx::detail {

/// Why and where, within the string's bytes, decoding stopped.
struct DecodeError {
    std::size_t offset = 0;
    const char* reason = "";
};

/// Validates `bytes` as UTF-8 (Unicode's well-formed byte sequences: no
/// overlong form, no encoded surrogate, nothing above U+10FFFF, no truncated
/// sequence) and copies it to `out`. On failure `out` is empty.
std::optional<DecodeError> DecodeUtf8(std::span<const std::byte> bytes, std::string& out);

/// Decodes `bytes` as UTF-16LE into UTF-8 in `out`. The length must be even
/// and every surrogate paired. On failure `out` is empty.
std::optional<DecodeError> DecodeUtf16Le(std::span<const std::byte> bytes, std::string& out);

} // namespace mmd::pmx::detail
