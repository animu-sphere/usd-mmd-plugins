// SPDX-License-Identifier: Apache-2.0
//
// What vmd_inspect prints: a VMD's header, sections, tracks and diagnostics,
// as text for a person or as JSON for a script. Source facts only, in the
// file's own basis and units -- nothing here converts or binds a value.
#pragma once

#include <motionVmd/Motion.h>
#include <motionVmd/Reader.h>

#include <optional>
#include <string>
#include <vector>

namespace vmdinspect {

/// A read, and the motion built from it when the read succeeded. The
/// diagnostics are both steps', in order: the reader's, then BuildMotion's.
struct Inspection {
    motionVmd::Result<motionVmd::Document> read;
    std::optional<motionVmd::Motion> motion;
    std::vector<motionVmd::Diagnostic> diagnostics;
};

Inspection Inspect(motionVmd::Result<motionVmd::Document> read);

struct ReportOptions {
    bool tracks = false; ///< text only: list every track
};

/// The human-readable report. `file` is printed as given.
std::string TextReport(const std::string& file, const Inspection& inspection,
                       const ReportOptions& options);

/// The JSON report: one object, always with every track. Keys are stable;
/// strings are UTF-8, and each name's CP932 bytes are also given in hex.
std::string JsonReport(const Inspection& inspection);

/// 0 when nothing worse than a warning was raised, 1 when an error was, 2
/// when the file could not be read (DIAGNOSTICS.md §4).
int ExitStatus(const Inspection& inspection);

} // namespace vmdinspect
