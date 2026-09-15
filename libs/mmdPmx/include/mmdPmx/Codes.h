// SPDX-License-Identifier: Apache-2.0
//
// The diagnostic codes mmdPmx raises. Each code's meaning is fixed by the
// design section named beside it, and its severity by
// docs/reference/DIAGNOSTICS.md §5; a code is never renamed, reused, or given
// another severity. Codes join this list with the code that raises them.
#pragma once

#include "mmdPmx/Diagnostic.h"

namespace mmd::codes {

// PMX syntax -- docs/design/PMX_CONTRACT.md §2, §3.
inline constexpr Code PmxBadSignature{"MMD_PMX_BAD_SIGNATURE", Severity::Fatal};
inline constexpr Code PmxUnsupportedVersion{"MMD_PMX_UNSUPPORTED_VERSION", Severity::Fatal};
inline constexpr Code PmxInvalidGlobals{"MMD_PMX_INVALID_GLOBALS", Severity::Fatal};
inline constexpr Code PmxUnknownGlobals{"MMD_PMX_UNKNOWN_GLOBALS", Severity::Warning};
inline constexpr Code PmxInvalidIndexSize{"MMD_PMX_INVALID_INDEX_SIZE", Severity::Fatal};
inline constexpr Code PmxTruncatedBuffer{"MMD_PMX_TRUNCATED_BUFFER", Severity::Fatal};

// Text -- docs/design/TEXT_ENCODING_POLICY.md §3.
inline constexpr Code TextInvalidEncodingFlag{"MMD_TEXT_INVALID_ENCODING_FLAG", Severity::Fatal};

}  // namespace mmd::codes
