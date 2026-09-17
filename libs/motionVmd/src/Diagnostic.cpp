// SPDX-License-Identifier: Apache-2.0
#include "motionVmd/Diagnostic.h"

#include <utility>

namespace motionVmd {

std::string_view
ToString(Severity severity)
{
    switch (severity) {
    case Severity::Info:
        return "info";
    case Severity::Warning:
        return "warning";
    case Severity::Error:
        return "error";
    case Severity::Fatal:
        return "fatal";
    }
    return "unknown";
}

std::string
ToString(const Location& location)
{
    std::string element = location.section;
    if (location.index) {
        element += "[" + std::to_string(*location.index) + "]";
    }
    if (!location.field.empty()) {
        if (!element.empty()) {
            element += ".";
        }
        element += location.field;
    }

    std::string out = element;
    if (location.byteOffset) {
        if (!out.empty()) {
            out += " at ";
        }
        out += "byte " + std::to_string(*location.byteOffset);
    }
    return out;
}

Diagnostic
MakeDiagnostic(const Code& code, std::string message, Location location)
{
    return Diagnostic{
        std::string(code.id),
        code.severity,
        std::move(message),
        std::move(location),
        code.severity != Severity::Fatal,
    };
}

std::string
FormatDiagnostic(const Diagnostic& diagnostic)
{
    std::string out = diagnostic.code + ": " + diagnostic.message;
    const std::string where = ToString(diagnostic.location);
    if (!where.empty()) {
        out += " (" + where + ")";
    }
    return out;
}

} // namespace motionVmd
