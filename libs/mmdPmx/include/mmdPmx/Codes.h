// SPDX-License-Identifier: Apache-2.0
//
// The diagnostic codes mmdPmx raises. Each code's meaning is fixed by the
// design section named beside it, and its severity by
// docs/reference/DIAGNOSTICS.md §5; a code is never renamed, reused, or given
// another severity. Codes join this list with the code that raises them, and
// scripts/check_docs.py fails when this list and DIAGNOSTICS.md §5 disagree.
#pragma once

#include "mmdPmx/Diagnostic.h"

namespace mmd::codes {

// PMX syntax -- docs/design/PMX_CONTRACT.md §2-§10.
inline constexpr Code PmxBadSignature{"MMD_PMX_BAD_SIGNATURE", Severity::Fatal};
inline constexpr Code PmxUnsupportedVersion{"MMD_PMX_UNSUPPORTED_VERSION", Severity::Fatal};
inline constexpr Code PmxInvalidGlobals{"MMD_PMX_INVALID_GLOBALS", Severity::Fatal};
inline constexpr Code PmxUnknownGlobals{"MMD_PMX_UNKNOWN_GLOBALS", Severity::Warning};
inline constexpr Code PmxInvalidIndexSize{"MMD_PMX_INVALID_INDEX_SIZE", Severity::Fatal};
inline constexpr Code PmxTruncatedBuffer{"MMD_PMX_TRUNCATED_BUFFER", Severity::Fatal};
inline constexpr Code PmxCountExceedsBuffer{"MMD_PMX_COUNT_EXCEEDS_BUFFER", Severity::Fatal};
inline constexpr Code PmxTrailingBytes{"MMD_PMX_TRAILING_BYTES", Severity::Warning};
inline constexpr Code PmxInvalidDeformType{"MMD_PMX_INVALID_DEFORM_TYPE", Severity::Fatal};
inline constexpr Code PmxFaceCountNotTriangles{"MMD_PMX_FACE_COUNT_NOT_TRIANGLES", Severity::Fatal};
inline constexpr Code PmxFaceIndexOutOfRange{"MMD_PMX_FACE_INDEX_OUT_OF_RANGE", Severity::Fatal};
inline constexpr Code PmxMaterialFacesExceedTable{"MMD_PMX_MATERIAL_FACES_EXCEED_TABLE",
                                                  Severity::Fatal};
inline constexpr Code PmxMaterialFacesShort{"MMD_PMX_MATERIAL_FACES_SHORT", Severity::Error};
inline constexpr Code PmxInvalidMorphType{"MMD_PMX_INVALID_MORPH_TYPE", Severity::Fatal};
inline constexpr Code PmxInvalidLayoutFlag{"MMD_PMX_INVALID_LAYOUT_FLAG", Severity::Fatal};
inline constexpr Code PmxIndexOutOfRange{"MMD_PMX_INDEX_OUT_OF_RANGE", Severity::Error};
inline constexpr Code PmxFileUnreadable{"MMD_PMX_FILE_UNREADABLE", Severity::Fatal};

// Text -- docs/design/TEXT_ENCODING_POLICY.md §3.
inline constexpr Code TextInvalidEncodingFlag{"MMD_TEXT_INVALID_ENCODING_FLAG", Severity::Fatal};
inline constexpr Code TextInvalidUtf8{"MMD_TEXT_INVALID_UTF8", Severity::Error};
inline constexpr Code TextInvalidUtf16{"MMD_TEXT_INVALID_UTF16", Severity::Error};

} // namespace mmd::codes
