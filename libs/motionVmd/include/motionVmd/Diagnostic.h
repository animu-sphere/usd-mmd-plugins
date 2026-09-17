// SPDX-License-Identifier: Apache-2.0
//
// motionVmd's diagnostic record (docs/reference/DIAGNOSTICS.md §1).
//
// **A record of its own, on purpose.** The workspace's record lives in mmdPmx,
// and motionVmd may not depend on mmdPmx: it is extraction-ready, and leaves
// this repository for the shared motion architecture without a model library
// in tow (docs/architecture/WORKSPACE.md §2.2, §7). So it carries the same
// fields -- a stable code, a fixed severity, a message, a location -- and
// nothing that would make carrying one into an `mmd::Diagnostic` lossy: the
// binding step (mmdMotionBinding) does exactly that, field for field.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace motionVmd {

/// Most severe last. A code's severity is fixed (DIAGNOSTICS.md §2, §3).
enum class Severity : std::uint8_t {
    Info,    ///< a documented normalization that loses nothing a consumer needs
    Warning, ///< fidelity loss, or source data a later record overrides
    Error,   ///< a contract violation; the value is dropped or emptied
    Fatal,   ///< the file cannot be read; nothing is returned
};

std::string_view ToString(Severity severity);

/// Where a problem is. `section` is "boneKeyframes", "morphKeyframes", ... ;
/// `index` the record within it. Every member is optional.
struct Location {
    std::optional<std::uint64_t> byteOffset;
    std::string section;
    std::optional<std::uint64_t> index;
    std::string field;

    bool operator==(const Location&) const = default;
};

/// "boneKeyframes[3].bone at byte 1234", "byte 9", or "".
std::string ToString(const Location& location);

/// A stable code and the severity it always carries, declared once per code
/// as an `inline constexpr` value (motionVmd/Codes.h).
struct Code {
    std::string_view id;
    Severity severity;
};

struct Diagnostic {
    std::string code;    ///< the contract; tests assert this, never `message`
    Severity severity;   ///< fixed per code
    std::string message; ///< human-readable, not part of the contract
    Location location;
    bool recoverable; ///< false exactly when severity is Fatal
};

Diagnostic MakeDiagnostic(const Code& code, std::string message, Location location = {});

/// "CODE: message (location)", the form every tool prints.
std::string FormatDiagnostic(const Diagnostic& diagnostic);

} // namespace motionVmd
