// SPDX-License-Identifier: Apache-2.0
//
// vmd_inspect -- what a VMD contains, without a model and without USD
// (docs/design/MOTION_CONTRACT.md §2).
//
//   vmd_inspect [--json] [--tracks] <file.vmd>
//
// Reads the file with motionVmd and nothing else. Exit status: 0 read
// (warnings at most), 1 read with errors, 2 not read, 3 a usage error.
#include "Report.h"

#include <motionVmd/Reader.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

constexpr int kUsageError = 3;

void
PrintUsage(std::FILE* to)
{
    std::fputs("usage: vmd_inspect [--json] [--tracks] <file.vmd>\n"
               "  --json    one JSON object with every track, for scripts\n"
               "  --tracks  list every bone, morph and IK track (text report)\n"
               "exit status: 0 read, 1 read with errors, 2 not read, 3 usage\n",
               to);
}

/// The report is UTF-8; a console is switched to UTF-8 while this process
/// writes to it, as mmd_inspect does.
class Utf8Console {
public:
    Utf8Console()
    {
#ifdef _WIN32
        const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        if (out != INVALID_HANDLE_VALUE && GetFileType(out) == FILE_TYPE_CHAR) {
            _previous = GetConsoleOutputCP();
            SetConsoleOutputCP(CP_UTF8);
        }
#endif
    }
    ~Utf8Console()
    {
#ifdef _WIN32
        if (_previous != 0) {
            std::fflush(stdout);
            SetConsoleOutputCP(_previous);
        }
#endif
    }
    Utf8Console(const Utf8Console&) = delete;
    Utf8Console& operator=(const Utf8Console&) = delete;

private:
    unsigned _previous = 0;
};

} // namespace

int
main(int argc, char** argv)
{
    bool json = false;
    vmdinspect::ReportOptions options;
    const char* file = nullptr;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--json") {
            json = true;
        } else if (arg == "--tracks") {
            options.tracks = true;
        } else if (arg == "-h" || arg == "--help") {
            PrintUsage(stdout);
            return 0;
        } else if (arg.starts_with("-") || file != nullptr) {
            std::fprintf(stderr, "vmd_inspect: unexpected argument '%s'\n", argv[i]);
            PrintUsage(stderr);
            return kUsageError;
        } else {
            file = argv[i];
        }
    }
    if (file == nullptr) {
        PrintUsage(stderr);
        return kUsageError;
    }

    // From UTF-8, explicitly (TEXT_ENCODING_POLICY.md §4).
    const std::u8string utf8(reinterpret_cast<const char8_t*>(file), std::strlen(file));
    const vmdinspect::Inspection inspection =
        vmdinspect::Inspect(motionVmd::ReadFile(std::filesystem::path(utf8)));

    const Utf8Console console;
    const std::string report = json ? vmdinspect::JsonReport(inspection)
                                    : vmdinspect::TextReport(file, inspection, options);
    std::fwrite(report.data(), 1, report.size(), stdout);
    std::fflush(stdout);
    return vmdinspect::ExitStatus(inspection);
}
