// SPDX-License-Identifier: Apache-2.0
#include "Identifiers.h"

#include "mmdModel/Codes.h"

#include <set>

namespace mmd::detail {

namespace {

bool
IsIdentifierChar(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

/// The key two identifiers collide on: equal ignoring ASCII case, so the stage
/// stays unambiguous in case-insensitive tools and filesystems.
std::string
CollisionKey(std::string_view identifier)
{
    std::string key(identifier);
    for (char& c : key) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return key;
}

} // namespace

std::string
IdentifierCandidate(std::string_view english)
{
    // Byte by byte: every byte of a multi-byte UTF-8 sequence is outside the
    // set, so a non-ASCII character joins the run around it.
    std::string replaced;
    bool inRun = false;
    for (char c : english) {
        if (IsIdentifierChar(c)) {
            replaced.push_back(c);
            inRun = false;
        } else if (!inRun) {
            replaced.push_back('_');
            inRun = true;
        }
    }
    const std::size_t first = replaced.find_first_not_of('_');
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = replaced.find_last_not_of('_');
    std::string candidate = replaced.substr(first, last - first + 1);
    if (candidate.size() > kIdentifierCap) {
        candidate.resize(kIdentifierCap);
    }
    if (candidate[0] >= '0' && candidate[0] <= '9') {
        candidate.insert(candidate.begin(), '_');
    }
    return candidate;
}

std::string
FallbackIdentifier(std::string_view kind, std::size_t sourceIndex)
{
    std::string digits = std::to_string(sourceIndex);
    if (digits.size() < 4) {
        digits.insert(0, 4 - digits.size(), '0');
    }
    return std::string(kind) + "_" + digits;
}

std::vector<std::string>
AssignIdentifiers(std::string_view kind, const std::vector<std::string>& englishNames,
                  const std::string& table, DiagnosticList& diagnostics)
{
    std::vector<std::string> identifiers;
    identifiers.reserve(englishNames.size());
    std::set<std::string> taken;
    const auto isFree = [&taken](const std::string& identifier) {
        return taken.find(CollisionKey(identifier)) == taken.end();
    };

    for (std::size_t i = 0; i < englishNames.size(); ++i) {
        std::string candidate = IdentifierCandidate(englishNames[i]);
        if (candidate.empty()) {
            candidate = FallbackIdentifier(kind, i);
        }
        std::string identifier = candidate;
        if (!isFree(identifier)) {
            identifier = candidate + "_" + std::to_string(i);
            if (!isFree(identifier)) {
                identifier = FallbackIdentifier(kind, i);
                for (std::size_t n = 2; !isFree(identifier); ++n) {
                    identifier = FallbackIdentifier(kind, i) + "_" + std::to_string(n);
                }
            }
            Location where;
            where.table = table;
            where.index = i;
            where.field = "englishName";
            diagnostics.Add(codes::UsdIdentifierCollision,
                            "'" + candidate + "' is taken by an earlier " + std::string(kind) +
                                "; this one is '" + identifier + "'",
                            std::move(where));
        }
        taken.insert(CollisionKey(identifier));
        identifiers.push_back(std::move(identifier));
    }
    return identifiers;
}

} // namespace mmd::detail
