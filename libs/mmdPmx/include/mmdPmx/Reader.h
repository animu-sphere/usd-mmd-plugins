// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "mmdPmx/Document.h"
#include "mmdPmx/Result.h"

#include <cstddef>
#include <span>

namespace mmd::pmx {

/// Reads a PMX file held in memory. Every read is bounded by `bytes`; nothing
/// is read from disk, and nothing depends on the host's byte order or locale
/// (PMX_CONTRACT.md §2). Taking a byte span keeps the parser directly
/// fuzzable (DESIGN_POLICY.md §10).
///
/// Phase 0 scaffold: validates the signature, the version and the globals, and
/// returns a document holding the header. The bytes after the globals are not
/// examined yet.
Result<Document> Read(std::span<const std::byte> bytes);

}  // namespace mmd::pmx
