// SPDX-License-Identifier: Apache-2.0
//
// Recoverable diagnostics, bounded as docs/reference/DIAGNOSTICS.md §4 says:
// one code at most kLimit times per section, and the rest counted and reported
// once, after everything else. A motion can hold a hundred thousand keyframes,
// and each recorded diagnostic is a string someone prints.
#pragma once

#include "motionVmd/Diagnostic.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace motionVmd::detail {

class DiagnosticList {
public:
    static constexpr std::size_t kLimit = 16;

    void Add(const Code& code, std::string message, Location location = {})
    {
        Tally& tally = _Find(code, location.section);
        if (++tally.seen <= kLimit) {
            _list.push_back(MakeDiagnostic(code, std::move(message), std::move(location)));
        }
    }

    std::vector<Diagnostic> Take()
    {
        for (const Tally& tally : _tallies) {
            if (tally.seen > kLimit) {
                Location where;
                where.section = tally.section;
                _list.push_back(MakeDiagnostic(*tally.code,
                                               std::to_string(tally.seen - kLimit) + " more in " +
                                                   tally.section + " are not listed",
                                               std::move(where)));
            }
        }
        _tallies.clear();
        return std::move(_list);
    }

private:
    struct Tally {
        const Code* code;
        std::string section;
        std::size_t seen = 0;
    };

    Tally& _Find(const Code& code, const std::string& section)
    {
        for (Tally& tally : _tallies) {
            if (tally.code->id == code.id && tally.section == section) {
                return tally;
            }
        }
        return _tallies.emplace_back(Tally{&code, section});
    }

    std::vector<Tally> _tallies;
    std::vector<Diagnostic> _list;
};

} // namespace motionVmd::detail
