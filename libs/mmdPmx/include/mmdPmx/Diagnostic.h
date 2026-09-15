// SPDX-License-Identifier: Apache-2.0
//
// The diagnostic record every component of the workspace reports with
// (docs/reference/DIAGNOSTICS.md §1). It lives in the lowest library so the
// canonical model and the importer can carry a parser diagnostic unchanged.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace mmd {

/// Most severe last. A code's severity is fixed (DIAGNOSTICS.md §2, §3).
enum class Severity : std::uint8_t {
    Info,     ///< a documented normalization that loses nothing a consumer needs
    Warning,  ///< fidelity loss, an approximation, or an unmapped feature
    Error,    ///< a contract violation; the element is dropped or repaired
    Fatal,    ///< the file cannot be read; nothing is authored
};

std::string_view ToString(Severity severity);

/// Where a problem is: a byte offset for syntax, a table element for
/// semantics, both when both are known. Every member is optional.
struct Location {
    std::optional<std::uint64_t> byteOffset;
    std::string table;                    ///< "bones", "materials"; empty if none
    std::optional<std::uint64_t> index;   ///< element index within `table`
    std::string field;                    ///< "texture", "globals[3]"; empty if whole element

    bool operator==(const Location&) const = default;
};

/// "materials[3].texture at byte 1234", "byte 9", or "" when nothing is known.
std::string ToString(const Location& location);

/// A stable code and the severity it always carries. Components declare their
/// codes as `inline constexpr Code` values, so a severity cannot be chosen at
/// the call site (see mmdPmx/Codes.h).
struct Code {
    std::string_view id;  ///< "MMD_PMX_TRUNCATED_BUFFER"
    Severity severity;
};

struct Diagnostic {
    std::string code;      ///< the contract; tests assert this, never `message`
    Severity severity;     ///< fixed per code
    std::string message;   ///< human-readable, not part of the contract
    Location location;     ///< where, when known
    bool recoverable;      ///< false exactly when severity is Fatal
};

/// Builds a diagnostic whose severity and recoverability follow from `code`.
Diagnostic MakeDiagnostic(const Code& code, std::string message, Location location = {});

/// "CODE: message (location)" -- the form recorded on the stage and printed
/// by tools (DIAGNOSTICS.md §4).
std::string FormatDiagnostic(const Diagnostic& diagnostic);

}  // namespace mmd
