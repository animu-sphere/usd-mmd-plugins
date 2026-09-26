// SPDX-License-Identifier: Apache-2.0
//
// mmd_export -- an MMD model written out as conventional OpenUSD, in the
// format the output's extension names (docs/design/PACKAGING_POLICY.md).
// Version 1 writes one format, a self-contained USDZ.
//
//   mmd_export <input.pmx> <output.usdz>
//
// Opens the model through the registered importer, packages the stage it
// authors with every texture it names, validates the package, and only then
// replaces the output. Exit status: 0 packaged, 1 not packaged because of an
// error, 2 not read or not written, 3 a usage error.
#include "Packaging.h"

#include <pxr/base/tf/diagnosticMgr.h>
#include <pxr/base/tf/error.h>

#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

constexpr int kPackaged = 0;
constexpr int kNotPackaged = 1;
constexpr int kNotReadOrWritten = 2;
constexpr int kUsageError = 3;

void
PrintUsage(std::FILE* to)
{
    std::fputs("usage: mmd_export <input.pmx> <output.usdz>\n"
               "  The output's extension names the format; this version writes .usdz.\n"
               "  --help     this text\n"
               "  --version  the tool's version\n"
               "The importer must be on OpenUSD's plugin path (PXR_PLUGINPATH_NAME).\n"
               "exit status: 0 packaged, 1 not packaged, 2 not read or not written, 3 usage\n",
               to);
}

/// Diagnostics are UTF-8, and so is `argv` (the executable's manifest makes
/// the process code page UTF-8 on Windows). A console shows bytes in its own
/// output code page, so it is switched to UTF-8 while this process writes to
/// it, as mmd_inspect does.
class Utf8Console {
public:
    Utf8Console()
    {
#ifdef _WIN32
        const HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
        if (err != INVALID_HANDLE_VALUE && GetFileType(err) == FILE_TYPE_CHAR) {
            _previous = GetConsoleOutputCP();
            SetConsoleOutputCP(CP_UTF8);
        }
#endif
    }
    ~Utf8Console()
    {
#ifdef _WIN32
        if (_previous != 0) {
            std::fflush(stderr);
            SetConsoleOutputCP(_previous);
        }
#endif
    }
    Utf8Console(const Utf8Console&) = delete;
    Utf8Console& operator=(const Utf8Console&) = delete;

private:
    unsigned _previous = 0;
};

/// OpenUSD's own diagnostics, kept off the console. Each step folds the
/// errors it expects into an MMD_PKG_ diagnostic; the importer's warnings
/// are already recorded on the stage and relayed from there. An error no
/// step expected is still printed.
class Quiet : public TfDiagnosticMgr::Delegate {
public:
    Quiet() { TfDiagnosticMgr::GetInstance().AddDelegate(this); }
    ~Quiet() override { TfDiagnosticMgr::GetInstance().RemoveDelegate(this); }
    Quiet(const Quiet&) = delete;
    Quiet& operator=(const Quiet&) = delete;

    void IssueError(const TfError& error) override
    {
        std::fprintf(stderr, "OpenUSD: %s\n", error.GetCommentary().c_str());
    }
    void IssueFatalError(const TfCallContext&, const std::string& message) override
    {
        std::fprintf(stderr, "OpenUSD: %s\n", message.c_str());
    }
    void IssueStatus(const TfStatus&) override {}
    void IssueWarning(const TfWarning&) override {}
};

/// A private directory for everything written before the package is
/// validated, removed however the run ends.
class Scratch {
public:
    explicit Scratch(std::error_code& ec)
    {
        std::random_device random;
        const mmdexport::fs::path base = mmdexport::fs::temp_directory_path(ec);
        if (ec) {
            return;
        }
        for (int attempt = 0; attempt < 16; ++attempt) {
            const mmdexport::fs::path candidate =
                base / ("mmd_export-" + std::to_string(random()) + std::to_string(random()));
            if (mmdexport::fs::create_directory(candidate, ec)) {
                _path = candidate;
                return;
            }
        }
        if (!ec) {
            ec = std::make_error_code(std::errc::file_exists);
        }
    }
    ~Scratch()
    {
        std::error_code ignored;
        if (!_path.empty()) {
            mmdexport::fs::remove_all(_path, ignored);
        }
    }
    Scratch(const Scratch&) = delete;
    Scratch& operator=(const Scratch&) = delete;

    const mmdexport::fs::path& path() const { return _path; }

private:
    mmdexport::fs::path _path;
};

int
Run(const mmdexport::fs::path& input, const mmdexport::fs::path& output,
    mmdexport::Diagnostics* diagnostics)
{
    using namespace mmdexport;

    std::error_code ec;
    const fs::path target = fs::absolute(output, ec);
    if (ec || !fs::is_directory(target.parent_path(), ec)) {
        diagnostics->Add(code::WriteFailed,
                         "the output's directory " + Utf8(target.parent_path()) +
                             " does not exist");
        return kNotReadOrWritten;
    }
    const Scratch scratch(ec);
    if (ec) {
        diagnostics->Add(code::WriteFailed,
                         "could not create a private directory (" + ec.message() + ")");
        return kNotReadOrWritten;
    }
    const fs::path usdz = scratch.path() / target.filename();

    // Every layer and stage is released before the scratch directory is
    // removed: Windows keeps an open layer's file.
    {
        const SdfLayerRefPtr model = OpenModel(input, diagnostics);
        if (!model) {
            return kNotReadOrWritten;
        }
        const PackagePlan plan = Discover(model, Utf8(target.stem()) + ".usdc", diagnostics);
        if (diagnostics->HasErrors()) {
            return kNotPackaged;
        }
        if (!ConvertTextures(plan, scratch.path(), diagnostics)) {
            return diagnostics->HasFatal() ? kNotReadOrWritten : kNotPackaged;
        }
        const SdfLayerRefPtr root = Materialize(model, plan, scratch.path(), diagnostics);
        if (!root) {
            return kNotReadOrWritten;
        }
        if (!ValidateMaterialized(root, diagnostics)) {
            return kNotPackaged;
        }
        if (!WritePackage(root, plan, usdz, diagnostics)) {
            return kNotReadOrWritten;
        }
        if (!ValidatePackage(usdz, plan, diagnostics)) {
            return kNotPackaged;
        }
    }
    return MoveIntoPlace(usdz, target, diagnostics) ? kPackaged : kNotReadOrWritten;
}

bool
IsUsdz(const mmdexport::fs::path& path)
{
    std::string extension = mmdexport::Utf8(path.extension());
    for (char& c : extension) {
        c = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    }
    return !path.stem().empty() && extension == ".usdz";
}

} // namespace

int
main(int argc, char** argv)
{
    std::vector<const char*> paths;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            PrintUsage(stdout);
            return kPackaged;
        }
        if (arg == "--version") {
            std::printf("mmd_export %s\n", MMDEXPORT_VERSION);
            return kPackaged;
        }
        if (arg.starts_with("-") || paths.size() == 2) {
            std::fprintf(stderr, "mmd_export: unexpected argument '%s'\n", argv[i]);
            PrintUsage(stderr);
            return kUsageError;
        }
        paths.push_back(argv[i]);
    }
    if (paths.size() != 2) {
        PrintUsage(stderr);
        return kUsageError;
    }

    // From UTF-8, explicitly: a narrow path would be decoded in the ANSI code
    // page on Windows (TEXT_ENCODING_POLICY.md §4).
    const mmdexport::fs::path input = mmdexport::PathFromUtf8(paths[0]);
    const mmdexport::fs::path output = mmdexport::PathFromUtf8(paths[1]);
    if (!IsUsdz(output)) {
        std::fprintf(stderr,
                     "mmd_export: this version writes .usdz only, and '%s' names another "
                     "format\n",
                     paths[1]);
        return kUsageError;
    }

    const Utf8Console console;
    const Quiet quiet;
    mmdexport::Diagnostics diagnostics;
    const int status = Run(input, output, &diagnostics);
    for (const mmdexport::Diagnostic& d : diagnostics.All()) {
        const std::string line = mmdexport::Diagnostics::Format(d) + "\n";
        std::fwrite(line.data(), 1, line.size(), stderr);
    }
    std::fflush(stderr);
    return status;
}
