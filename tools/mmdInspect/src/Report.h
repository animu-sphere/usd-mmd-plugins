// SPDX-License-Identifier: Apache-2.0
//
// What mmd_inspect prints: a PMX's header, model text, table sizes, element
// summaries and diagnostics, as text for a person or as JSON for a script.
// Source facts only -- nothing here converts, normalizes or judges a value;
// that is canonicalization's, and mmd_inspect has none.
#pragma once

#include <mmdPmx/Document.h>
#include <mmdPmx/Result.h>

#include <string>

namespace mmdinspect {

struct ReportOptions {
    bool elements = false;  ///< text only: list every element of every table
};

/// The human-readable report. `file` is printed as given.
std::string TextReport(const std::string& file, const mmd::Result<mmd::pmx::Document>& result,
    const ReportOptions& options);

/// The JSON report: one object, always with every element. Keys are stable;
/// strings are UTF-8.
std::string JsonReport(const mmd::Result<mmd::pmx::Document>& result);

/// 0 when nothing worse than a warning was raised, 1 when an error was, 2
/// when the file could not be read (DIAGNOSTICS.md §4: a tool sets its exit
/// status by the most severe diagnostic).
int ExitStatus(const mmd::Result<mmd::pmx::Document>& result);

}  // namespace mmdinspect
