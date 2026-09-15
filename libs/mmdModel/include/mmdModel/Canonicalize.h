// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "mmdModel/CanonicalDocument.h"

#include <mmdPmx/Document.h>
#include <mmdPmx/Result.h>

namespace mmd {

/// The PMX source facts made canonical (docs/design/PMX_CONTRACT.md §14):
/// stable identifiers, the single basis and unit conversion, the canonical
/// joint order with every bone index remapped to it, normalized skinning, the
/// mesh with its winding reversed and `st` flipped, material face ranges, and
/// normalized texture paths.
///
/// It never reads a file, never touches OpenUSD, and never evaluates anything;
/// the same document always produces the same result. Nothing in a document
/// the parser accepted is fatal here: every repair is a recoverable
/// diagnostic, in the order it was made, bounded as the parser's are
/// (docs/reference/DIAGNOSTICS.md §4).
Result<CanonicalDocument> Canonicalize(const pmx::Document& document);

} // namespace mmd
