// SPDX-License-Identifier: Apache-2.0
//
// The bounded list every component collects its recoverable diagnostics in
// (docs/reference/DIAGNOSTICS.md §4). It lives beside the record, in the lowest
// library, so the parser and the canonical model bound their diagnostics the
// same way rather than each keeping a copy (docs/architecture/WORKSPACE.md §7).
#pragma once

#include "mmdPmx/Diagnostic.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace mmd {

/// Recoverable diagnostics in emission order, bounded: one code is recorded at
/// most kLimit times per table, and the rest of that code's occurrences there
/// are counted and reported once, after everything else
/// (DIAGNOSTICS.md §4). A malformed file can otherwise raise one diagnostic
/// per vertex, and every recorded diagnostic is a string on the stage.
class DiagnosticList {
public:
    static constexpr std::size_t kLimit = 16;

    void Add(const Code& code, std::string message, Location location = {})
    {
        Tally& tally = _Find(code, location.table);
        if (++tally.seen <= kLimit) {
            _list.push_back(MakeDiagnostic(code, std::move(message), std::move(location)));
        }
    }

    /// Everything recorded, then one summary per code and table that went
    /// over the limit, in the order that code first appeared in that table.
    std::vector<Diagnostic> Take()
    {
        for (const Tally& tally : _tallies) {
            if (tally.seen > kLimit) {
                Location where;
                where.table = tally.table;
                _list.push_back(MakeDiagnostic(*tally.code,
                    std::to_string(tally.seen - kLimit) + " more in " + tally.table
                        + " are not listed",
                    std::move(where)));
            }
        }
        _tallies.clear();
        return std::move(_list);
    }

private:
    struct Tally {
        const Code* code;
        std::string table;
        std::size_t seen = 0;
    };

    Tally& _Find(const Code& code, const std::string& table)
    {
        for (Tally& tally : _tallies) {
            if (tally.code->id == code.id && tally.table == table) {
                return tally;
            }
        }
        return _tallies.emplace_back(Tally{&code, table});
    }

    std::vector<Tally> _tallies;
    std::vector<Diagnostic> _list;
};

}  // namespace mmd
