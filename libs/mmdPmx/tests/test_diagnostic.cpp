// SPDX-License-Identifier: Apache-2.0
#include "mmdPmx/Codes.h"
#include "mmdPmx/Diagnostic.h"
#include "mmdPmx/Result.h"

#include "DiagnosticList.h"

#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

void
TestSeverityFollowsCode()
{
    const mmd::Diagnostic fatal =
        mmd::MakeDiagnostic(mmd::codes::PmxTruncatedBuffer, "short");
    assert(fatal.code == "MMD_PMX_TRUNCATED_BUFFER");
    assert(fatal.severity == mmd::Severity::Fatal);
    assert(!fatal.recoverable);

    const mmd::Diagnostic warning =
        mmd::MakeDiagnostic(mmd::codes::PmxUnknownGlobals, "extra");
    assert(warning.severity == mmd::Severity::Warning);
    assert(warning.recoverable);
}

void
TestLocationText()
{
    assert(mmd::ToString(mmd::Location{}).empty());

    mmd::Location byteOnly;
    byteOnly.byteOffset = 9;
    assert(mmd::ToString(byteOnly) == "byte 9");

    mmd::Location element;
    element.table = "materials";
    element.index = 3;
    element.field = "texture";
    assert(mmd::ToString(element) == "materials[3].texture");

    element.byteOffset = 1234;
    assert(mmd::ToString(element) == "materials[3].texture at byte 1234");
}

void
TestFormat()
{
    mmd::Location location;
    location.byteOffset = 4;
    const mmd::Diagnostic d = mmd::MakeDiagnostic(
        mmd::codes::PmxUnsupportedVersion, "PMX version 3 is not 2.0 or 2.1",
        location);
    assert(mmd::FormatDiagnostic(d)
           == "MMD_PMX_UNSUPPORTED_VERSION: PMX version 3 is not 2.0 or 2.1 "
              "(byte 4)");

    const mmd::Diagnostic bare =
        mmd::MakeDiagnostic(mmd::codes::PmxUnknownGlobals, "extra");
    assert(mmd::FormatDiagnostic(bare) == "MMD_PMX_UNKNOWN_GLOBALS: extra");
}

void
TestResult()
{
    const mmd::Diagnostic warning =
        mmd::MakeDiagnostic(mmd::codes::PmxUnknownGlobals, "extra");

    const auto success = mmd::Result<int>::Success(7, {warning});
    assert(success.ok());
    assert(static_cast<bool>(success));
    assert(success.value() == 7);
    assert(success.fatal() == nullptr);
    assert(success.diagnostics().size() == 1);

    // A failure keeps what was raised before the fatal diagnostic.
    const auto failure = mmd::Result<int>::Failure(
        mmd::MakeDiagnostic(mmd::codes::PmxBadSignature, "not PMX"), {warning});
    assert(!failure.ok());
    assert(failure.fatal() != nullptr);
    assert(failure.fatal()->code == "MMD_PMX_BAD_SIGNATURE");
    assert(failure.diagnostics().size() == 1);
    assert(failure.diagnostics()[0].code == "MMD_PMX_UNKNOWN_GLOBALS");

    bool threw = false;
    try {
        (void)failure.value();
    } catch (const std::bad_optional_access&) {
        threw = true;
    }
    assert(threw);

    // An rvalue result moves its value out.
    auto moved = mmd::Result<std::string>::Success(std::string("pmx"));
    const std::string taken = std::move(moved).value();
    assert(taken == "pmx");
}

void
TestDiagnosticList()
{
    using mmd::pmx::detail::DiagnosticList;
    DiagnosticList list;
    const auto at = [](const char* table, std::uint64_t index) {
        mmd::Location where;
        where.table = table;
        where.index = index;
        return where;
    };
    for (std::uint64_t i = 0; i < DiagnosticList::kLimit + 5; ++i) {
        list.Add(mmd::codes::PmxIndexOutOfRange, "bad", at("vertices", i));
    }
    list.Add(mmd::codes::TextInvalidUtf8, "bad", at("vertices", 0));
    list.Add(mmd::codes::PmxIndexOutOfRange, "bad", at("bones", 0));

    const std::vector<mmd::Diagnostic> taken = list.Take();
    // The limit is per code and table: the other two are recorded in full.
    assert(taken.size() == DiagnosticList::kLimit + 2 + 1);
    assert(taken[DiagnosticList::kLimit].code == "MMD_TEXT_INVALID_UTF8");
    assert(taken[DiagnosticList::kLimit + 1].location.table == "bones");
    const mmd::Diagnostic& summary = taken.back();
    assert(summary.code == "MMD_PMX_INDEX_OUT_OF_RANGE");
    assert(summary.severity == mmd::Severity::Error);
    assert(summary.location.table == "vertices" && !summary.location.index);
    assert(summary.message == "5 more in vertices are not listed");
}

}  // namespace

void
TestDiagnostic()
{
    TestSeverityFollowsCode();
    TestLocationText();
    TestFormat();
    TestResult();
    TestDiagnosticList();
}
