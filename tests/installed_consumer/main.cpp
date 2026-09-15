// SPDX-License-Identifier: Apache-2.0
//
// Reads one PMX through the installed mmdPmx, canonicalizes it through the
// installed mmdModel, and prints what it found:
//   version=2.0                 on success
//   joints=4 faces=3            the canonical skeleton and mesh
//   fatal=MMD_PMX_...           when the file is refused
// Exit status: 0 read, 1 refused, 2 usage or I/O error.
#include <mmdModel/Canonicalize.h>
#include <mmdPmx/Reader.h>

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

int
main(int argc, char** argv)
{
    if (argc != 2) {
        std::fprintf(stderr, "usage: pmx_header_probe <file.pmx>\n");
        return 2;
    }
    std::ifstream in(std::filesystem::path(argv[1]), std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }
    const std::vector<char> raw{std::istreambuf_iterator<char>(in),
                                std::istreambuf_iterator<char>()};
    const auto bytes = std::as_bytes(std::span<const char>(raw));

    const auto result = mmd::pmx::Read(bytes);
    for (const mmd::Diagnostic& d : result.diagnostics()) {
        std::printf("diagnostic=%s\n", mmd::FormatDiagnostic(d).c_str());
    }
    if (!result.ok()) {
        std::printf("fatal=%s\n", result.fatal()->code.c_str());
        return 1;
    }
    std::printf("version=%s\n",
                std::string(mmd::pmx::ToString(result.value().header.version)).c_str());

    const auto canonical = mmd::Canonicalize(result.value());
    std::printf("joints=%zu faces=%zu\n",
                canonical.value().skeleton.bones.size(),
                canonical.value().mesh.FaceCount());
    return 0;
}
