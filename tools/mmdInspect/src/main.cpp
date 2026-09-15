// SPDX-License-Identifier: Apache-2.0
//
// mmd_inspect -- what a PMX contains, without USD (DESIGN_POLICY.md §5.5).
//
//   mmd_inspect [--json] [--elements] <file.pmx>
//
// Reads the file with mmdPmx and nothing else, so a parser question can be
// answered with no importer in the way. Exit status: 0 read (warnings at
// most), 1 read with errors, 2 not read, 3 a usage error.
#include "Report.h"

#include <mmdPmx/Reader.h>

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
    std::fputs("usage: mmd_inspect [--json] [--elements] <file.pmx>\n"
               "  --json      one JSON object with every element, for scripts\n"
               "  --elements  list every element of every table (text report)\n"
               "exit status: 0 read, 1 read with errors, 2 not read, 3 usage\n",
               to);
}

/// The report is UTF-8, and so is `argv` (the executable's manifest makes the
/// process code page UTF-8 on Windows). A console still shows bytes in its own
/// output code page, so it is switched to UTF-8 while this process writes to
/// it and switched back after. Pipes and files get the bytes unchanged.
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

void
Write(const std::string& text)
{
    std::fwrite(text.data(), 1, text.size(), stdout);
}

} // namespace

int
main(int argc, char** argv)
{
    bool json = false;
    mmdinspect::ReportOptions options;
    const char* file = nullptr;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--json") {
            json = true;
        } else if (arg == "--elements") {
            options.elements = true;
        } else if (arg == "-h" || arg == "--help") {
            PrintUsage(stdout);
            return 0;
        } else if (arg.starts_with("-") || file != nullptr) {
            std::fprintf(stderr, "mmd_inspect: unexpected argument '%s'\n", argv[i]);
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

    // From UTF-8, explicitly: a narrow path would be decoded in the ANSI
    // code page on Windows (TEXT_ENCODING_POLICY.md §4).
    const std::u8string utf8(reinterpret_cast<const char8_t*>(file), std::strlen(file));
    const auto result = mmd::pmx::ReadFile(std::filesystem::path(utf8));

    const Utf8Console console;
    Write(json ? mmdinspect::JsonReport(result) : mmdinspect::TextReport(file, result, options));
    std::fflush(stdout);
    return mmdinspect::ExitStatus(result);
}
