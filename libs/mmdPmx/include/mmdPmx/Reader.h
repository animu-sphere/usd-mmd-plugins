// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "mmdPmx/Document.h"
#include "mmdPmx/Result.h"

#include <cstddef>
#include <filesystem>
#include <span>

namespace mmd::pmx {

/// Reads a PMX file held in memory: the header, every table in file order,
/// and a check that nothing follows the last one. Every read is bounded by
/// `bytes`; nothing is read from disk, and nothing depends on the host's byte
/// order or locale (PMX_CONTRACT.md §2). Taking a byte span keeps the parser
/// directly fuzzable (DESIGN_POLICY.md §10).
///
/// A fatal diagnostic -- the rest of the file cannot be located, or a face
/// would draw a vertex that does not exist -- returns no document. Everything
/// else is recoverable: an undecodable string is empty, an out-of-range index
/// is kNoIndex, each with a diagnostic (PMX_CONTRACT.md §15).
Result<Document> Read(std::span<const std::byte> bytes);

/// Reads the file at `path` and parses it with Read. A file that cannot be
/// opened or read in full is MMD_PMX_FILE_UNREADABLE.
///
/// The path is taken as a std::filesystem::path, never as a narrow string: on
/// Windows a narrow path would be decoded in the ANSI code page
/// (TEXT_ENCODING_POLICY.md §4). A caller holding UTF-8 converts it through
/// char8_t. The file-format plugin does not call this -- it reads through Ar.
Result<Document> ReadFile(const std::filesystem::path& path);

}  // namespace mmd::pmx
