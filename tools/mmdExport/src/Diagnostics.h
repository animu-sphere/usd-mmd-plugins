// SPDX-License-Identifier: Apache-2.0
//
// What mmd_export reports (docs/design/PACKAGING_POLICY.md §11).
//
// The tool may not link mmdPmx, whose diagnostic record the parser and the
// importer share (docs/architecture/WORKSPACE.md §2.2, invariant 7), so it
// keeps the smallest list that prints "CODE: message" as the other tools do.
// The importer's own diagnostics reach it as the text the stage records, and
// are relayed unchanged.
#pragma once

#include "Codes.h"

#include <string>
#include <string_view>
#include <vector>

namespace mmdexport {

struct Diagnostic {
    /// Empty for a line relayed from the importer, which already begins with
    /// its own code.
    std::string code;
    Severity severity = Severity::Info;
    std::string message;
};

class Diagnostics {
public:
    void Add(const Code& code, std::string message)
    {
        _all.push_back({std::string(code.id), code.severity, std::move(message)});
    }

    /// A line the importer wrote, printed as it is. It describes the stage,
    /// not the export, so it never decides the exit status.
    void Relay(std::string line) { _all.push_back({{}, Severity::Info, std::move(line)}); }

    const std::vector<Diagnostic>& All() const { return _all; }

    bool HasErrors() const
    {
        for (const Diagnostic& d : _all) {
            if (d.severity == Severity::Error || d.severity == Severity::Fatal) {
                return true;
            }
        }
        return false;
    }

    bool HasFatal() const
    {
        for (const Diagnostic& d : _all) {
            if (d.severity == Severity::Fatal) {
                return true;
            }
        }
        return false;
    }

    std::size_t Count(const Code& code) const
    {
        std::size_t n = 0;
        for (const Diagnostic& d : _all) {
            n += d.code == code.id ? 1 : 0;
        }
        return n;
    }

    static std::string Format(const Diagnostic& d)
    {
        return d.code.empty() ? d.message : d.code + ": " + d.message;
    }

private:
    std::vector<Diagnostic> _all;
};

} // namespace mmdexport
