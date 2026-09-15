// SPDX-License-Identifier: Apache-2.0
//
// Stable identifiers against TEXT_ENCODING_POLICY.md §6.1, its examples first.
#include "Identifiers.h"

#include <cassert>
#include <string>
#include <vector>

namespace {

using namespace mmd::detail;

std::vector<std::string>
Codes(mmd::DiagnosticList& list)
{
    std::vector<std::string> codes;
    for (const mmd::Diagnostic& d : list.Take()) {
        codes.push_back(d.code);
    }
    return codes;
}

void
TestCandidate()
{
    assert(IdentifierCandidate("LeftArm") == "LeftArm");
    assert(IdentifierCandidate("smile 2") == "smile_2");
    assert(IdentifierCandidate("").empty());
    // Every non-ASCII character is outside the set: a run of them is one `_`,
    // and a name of nothing else yields nothing.
    assert(IdentifierCandidate("センター").empty());
    assert(IdentifierCandidate("Left腕Arm") == "Left_Arm");
    assert(IdentifierCandidate("  a -- b  ") == "a_b");
    assert(IdentifierCandidate("__x__") == "x");
    assert(IdentifierCandidate("a__b") == "a__b");  // `_` is in the set
    assert(IdentifierCandidate("2nd bone") == "_2nd_bone");
    assert(IdentifierCandidate(std::string("tail\0\0", 6)) == "tail");

    // Cut to 64, then the digit prefix (TEXT-O2).
    const std::string longName(100, 'x');
    assert(IdentifierCandidate(longName) == std::string(64, 'x'));
    const std::string digits = "9" + std::string(99, 'y');
    assert(IdentifierCandidate(digits) == "_9" + std::string(63, 'y'));
}

void
TestFallback()
{
    assert(FallbackIdentifier("bone", 3) == "bone_0003");
    assert(FallbackIdentifier("material", 0) == "material_0000");
    assert(FallbackIdentifier("bone", 12345) == "bone_12345");
}

void
TestAssignment()
{
    mmd::DiagnosticList list;
    // The policy's examples: an English name, none, and a case-insensitive
    // collision that the later element loses.
    std::vector<std::string> english(41);
    english[0] = "LeftArm";
    english[1] = "Hair";
    english[40] = "hair";
    const auto ids = AssignIdentifiers("bone", english, "bones", list);
    assert(ids[0] == "LeftArm");
    assert(ids[1] == "Hair");
    assert(ids[3] == "bone_0003");
    assert(ids[40] == "hair_40");
    const auto diagnostics = list.Take();
    assert(diagnostics.size() == 1);
    assert(diagnostics[0].code == "MMD_USD_IDENTIFIER_COLLISION");
    assert(diagnostics[0].location.table == "bones" && diagnostics[0].location.index == 40u);

    // A renamed identifier is itself taken for the elements after it, ignoring
    // case: "A" becomes "A_1", so a later "a_1" becomes "a_1_2".
    const std::vector<std::string> cased{"a", "A", "a_1"};
    const auto renamed = AssignIdentifiers("morph", cased, "morphs", list);
    assert((renamed == std::vector<std::string>{"a", "A_1", "a_1_2"}));
    assert(Codes(list).size() == 2);

    // Every step of the rule in turn: the candidate "Q" is taken (by "q"),
    // "Q_4" is taken, the fallback "morph_0004" is taken, and so is
    // "morph_0004_2" -- so the element counts on to "morph_0004_3".
    const std::vector<std::string> crowded{"q", "q_4", "morph_0004", "morph_0004_2", "Q"};
    const auto last = AssignIdentifiers("morph", crowded, "morphs", list);
    assert(last[4] == "morph_0004_3");
    assert(Codes(list) == std::vector<std::string>{"MMD_USD_IDENTIFIER_COLLISION"});
}

}  // namespace

void
TestIdentifiers()
{
    TestCandidate();
    TestFallback();
    TestAssignment();
}
