# Diagnostics

Every diagnostic the parser, the canonicalizer, the importer, the validator or
a tool reports carries a **stable code** with a **fixed severity**. The code is
the contract; the message is human-readable detail and may change at any time.
Tests assert codes, never prose.

Status (2026-09-15): the record below is code, and the Phase 0 header reader
**emits** seven codes — each marked *emitted* in §5. Every other code is
*reserved* by a design document, which is where its meaning is fixed. A code
joins [libs/mmdPmx/include/mmdPmx/Codes.h](../../libs/mmdPmx/include/mmdPmx/Codes.h)
(or its component's equivalent) with the code that raises it; when the Phase 1
parser lands, this page is generated from those declarations, as
`usd-vrm-plugins` does for its own diagnostics.

## 1. The record

Declared in
[libs/mmdPmx/include/mmdPmx/Diagnostic.h](../../libs/mmdPmx/include/mmdPmx/Diagnostic.h),
the lowest library, so the canonical model and the importer carry a parser
diagnostic unchanged:

```cpp
namespace mmd {
struct Location {
    std::optional<std::uint64_t> byteOffset;
    std::string table;                    // "bones", "materials"; empty if none
    std::optional<std::uint64_t> index;   // element index within `table`
    std::string field;                    // "texture", "globals[3]"; empty if whole element
};

struct Code {                  // declared once per code, as `inline constexpr`
    std::string_view id;       // "MMD_PMX_TRUNCATED_BUFFER"
    Severity severity;         // fixed per code (§2)
};

struct Diagnostic {
    std::string code;          // the contract; tests assert this
    Severity    severity;      // fixed per code (§2)
    std::string message;       // human-readable, not part of the contract
    Location    location;      // where, when known
    bool        recoverable;   // false exactly when severity is fatal
};

Diagnostic MakeDiagnostic(const Code&, std::string message, Location = {});
std::string FormatDiagnostic(const Diagnostic&);  // "CODE: message (location)"
}
```

A diagnostic is built only from a declared `Code`, so a call site cannot
choose a severity. `Location` names what a reader needs to find the problem: a
byte offset for syntax errors, a table element (`bones[12]`,
`materials[3].texture`) for semantic ones, and both when both are known —
formatted as `materials[3].texture at byte 1234`.

`Result<T>`
([Result.h](../../libs/mmdPmx/include/mmdPmx/Result.h)) carries either a value
and the recoverable diagnostics raised while producing it, or the fatal
diagnostic that prevented one together with the recoverable ones raised
before it.

## 2. Severity

Most severe first. Tools exit non-zero on any `error` or `fatal`.

| Severity | Meaning | Recoverable |
| --- | --- | --- |
| `fatal` | the file cannot be read or would author a structurally wrong stage; `Read` fails and the stage does not open | no |
| `error` | the source violates its contract; the offending element or relation is dropped or repaired, and the import continues | yes |
| `warning` | fidelity loss, an approximation, or an unmapped feature; the import is complete but not exact | yes |
| `info` | a deliberate, documented normalization that loses nothing a consumer needs | yes |

## 3. Families and naming

Codes are `MMD_<FAMILY>_<EVENT>`, upper snake case.

| Family | Raised for |
| --- | --- |
| `MMD_PMX_` | PMX syntax and structure |
| `MMD_TEXT_` | text decoding |
| `MMD_PATH_` | texture paths and their resolution |
| `MMD_SKEL_` | skeleton and skinning |
| `MMD_MORPH_` | morphs |
| `MMD_MATERIAL_` | materials |
| `MMD_PHYSICS_` | rigid bodies, joints, soft bodies |
| `MMD_MOTION_` | VMD and motion binding (Phase 7) |
| `MMD_USD_` | the source → USD boundary |

A code is never renamed, never reused for a different event, and never changes
severity. An event that needs a different severity gets a new code.

## 4. Surfacing

- **Fatal:** `SdfFileFormat::Read` returns false and posts a runtime error whose
  text begins with the code.
- **Recoverable:** each is recorded, in emission order, on the stage as
  `/Asset.customData["mmd:diagnostics"]` (`string[]`, `CODE: message`, with
  the location in parentheses when known; the key is authored only when the
  list is non-empty)
  ([STAGE_CONTRACT.md §5](../design/STAGE_CONTRACT.md#5-model-metadata-and-provenance)),
  and `error` and `warning` are also posted as warnings. A diagnostic raised
  once per import (the SDEF approximation, weight normalization) carries a count
  rather than repeating per element.
- **Tools** (`mmd_inspect`) print every diagnostic and set their exit status by
  the most severe one.
- **Validation** codes are raised by checks over an already-imported stage, not
  by the importer.

## 5. Reserved catalog

"Raised by" is the component that detects the event. "Defined in" is the
design section that fixes its meaning. *emitted* marks a code the current code
raises; every other code is reserved.

### 5.1 PMX syntax

| Code | Severity | Raised by | Defined in |
| --- | --- | --- | --- |
| `MMD_PMX_BAD_SIGNATURE` *emitted* | fatal | `mmdPmx` | [PMX §3](../design/PMX_CONTRACT.md#3-header-and-globals) |
| `MMD_PMX_UNSUPPORTED_VERSION` *emitted* | fatal | `mmdPmx` | [PMX §3](../design/PMX_CONTRACT.md#3-header-and-globals) |
| `MMD_PMX_INVALID_GLOBALS` *emitted* | fatal | `mmdPmx` | [PMX §3](../design/PMX_CONTRACT.md#3-header-and-globals) |
| `MMD_PMX_UNKNOWN_GLOBALS` *emitted* | warning | `mmdPmx` | [PMX §3](../design/PMX_CONTRACT.md#3-header-and-globals) |
| `MMD_PMX_INVALID_INDEX_SIZE` *emitted* | fatal | `mmdPmx` | [PMX §3](../design/PMX_CONTRACT.md#3-header-and-globals) |
| `MMD_PMX_TRUNCATED_BUFFER` *emitted* (header) | fatal | `mmdPmx` | [PMX §2](../design/PMX_CONTRACT.md#2-reading-rules) |
| `MMD_PMX_COUNT_EXCEEDS_BUFFER` | fatal | `mmdPmx` | [PMX §2](../design/PMX_CONTRACT.md#2-reading-rules) |
| `MMD_PMX_TRAILING_BYTES` | warning | `mmdPmx` | [PMX §2](../design/PMX_CONTRACT.md#2-reading-rules) |
| `MMD_PMX_INVALID_DEFORM_TYPE` | fatal | `mmdPmx` | [PMX §5](../design/PMX_CONTRACT.md#5-vertices-and-deform) |
| `MMD_PMX_FACE_COUNT_NOT_TRIANGLES` | fatal | `mmdPmx` | [PMX §6](../design/PMX_CONTRACT.md#6-faces) |
| `MMD_PMX_FACE_INDEX_OUT_OF_RANGE` | fatal | `mmdPmx` | [PMX §6](../design/PMX_CONTRACT.md#6-faces) |
| `MMD_PMX_MATERIAL_FACES_EXCEED_TABLE` | fatal | `mmdPmx` | [PMX §8](../design/PMX_CONTRACT.md#8-materials) |
| `MMD_PMX_MATERIAL_FACES_SHORT` | error | `mmdPmx` | [PMX §8](../design/PMX_CONTRACT.md#8-materials) |
| `MMD_PMX_INVALID_MORPH_TYPE` | fatal | `mmdPmx` | [PMX §10](../design/PMX_CONTRACT.md#10-morphs) |
| `MMD_PMX_INDEX_OUT_OF_RANGE` | error | `mmdPmx` | [PMX §4](../design/PMX_CONTRACT.md#4-indices) |

### 5.2 Text

| Code | Severity | Raised by | Defined in |
| --- | --- | --- | --- |
| `MMD_TEXT_INVALID_ENCODING_FLAG` *emitted* | fatal | `mmdPmx` | [TEXT §3](../design/TEXT_ENCODING_POLICY.md#3-decoding-pmx-text) |
| `MMD_TEXT_INVALID_UTF8` | error | `mmdPmx` | [TEXT §3](../design/TEXT_ENCODING_POLICY.md#3-decoding-pmx-text) |
| `MMD_TEXT_INVALID_UTF16` | error | `mmdPmx` | [TEXT §3](../design/TEXT_ENCODING_POLICY.md#3-decoding-pmx-text) |
| `MMD_TEXT_TRAILING_NUL` | info | `mmdModel` | [TEXT §3](../design/TEXT_ENCODING_POLICY.md#3-decoding-pmx-text) |
| `MMD_TEXT_TRUNCATED_CP932` | info | `motionVmd` | [MOTION §4](../design/MOTION_CONTRACT.md#4-text) |

### 5.3 Paths

| Code | Severity | Raised by | Defined in |
| --- | --- | --- | --- |
| `MMD_PATH_UNSAFE_TEXTURE_PATH` | warning | `mmdModel` | [TEXT §7.2](../design/TEXT_ENCODING_POLICY.md#72-unsafe-paths) |
| `MMD_PATH_TEXTURE_NOT_FOUND` | warning | validation | [TEXT §7.3](../design/TEXT_ENCODING_POLICY.md#73-no-filesystem-access-while-authoring) |

### 5.4 Skeleton and skinning

| Code | Severity | Raised by | Defined in |
| --- | --- | --- | --- |
| `MMD_SKEL_INVALID_PARENT` | error | `mmdModel` | [STAGE §9.1](../design/STAGE_CONTRACT.md#91-canonical-joint-order) |
| `MMD_SKEL_PARENT_CYCLE` | error | `mmdModel` | [STAGE §9.1](../design/STAGE_CONTRACT.md#91-canonical-joint-order) |
| `MMD_SKEL_JOINTS_REORDERED` | info | `mmdModel` | [STAGE §9.1](../design/STAGE_CONTRACT.md#91-canonical-joint-order) |
| `MMD_SKEL_WEIGHTS_NORMALIZED` | info | `mmdModel` | [STAGE §9.5](../design/STAGE_CONTRACT.md#95-weight-normalization) |
| `MMD_SKEL_ZERO_WEIGHTS` | warning | `mmdModel` | [STAGE §9.5](../design/STAGE_CONTRACT.md#95-weight-normalization) |
| `MMD_SKEL_SDEF_APPROXIMATED` | warning | `usdMmdFileFormat` | [STAGE §9.4](../design/STAGE_CONTRACT.md#94-skinning) |
| `MMD_SKEL_QDEF_APPROXIMATED` | warning | `usdMmdFileFormat` | [STAGE §9.4](../design/STAGE_CONTRACT.md#94-skinning) |

### 5.5 Morphs, materials, physics

| Code | Severity | Raised by | Defined in |
| --- | --- | --- | --- |
| `MMD_MORPH_NO_SKELETON` | warning | `usdMmdFileFormat` | [STAGE §4.1](../design/STAGE_CONTRACT.md#41-why-asset-is-the-skelroot) |
| `MMD_MORPH_UNKNOWN_PANEL` | warning | `mmdModel` | [PMX §10](../design/PMX_CONTRACT.md#10-morphs) |
| `MMD_MORPH_GROUP_CYCLE` | error | `mmdModel` | [PMX §10](../design/PMX_CONTRACT.md#10-morphs) |
| `MMD_MATERIAL_UNSUPPORTED_SPHERE_MODE` | warning | `mmdModel` | [PMX §8](../design/PMX_CONTRACT.md#8-materials) |
| `MMD_PHYSICS_SOFT_BODY_UNSUPPORTED` | warning | `usdMmdFileFormat` | [PMX §12](../design/PMX_CONTRACT.md#12-soft-bodies-21) |

### 5.6 Motion (Phase 7)

| Code | Severity | Raised by | Defined in |
| --- | --- | --- | --- |
| `MMD_MOTION_DUPLICATE_KEYFRAME` | warning | `motionVmd` | [MOTION §5](../design/MOTION_CONTRACT.md#5-time) |
| `MMD_MOTION_UNMATCHED_BONE` | info | binding | [MOTION §8.1](../design/MOTION_CONTRACT.md#81-name-matching) |
| `MMD_MOTION_AMBIGUOUS_NAME` | warning | binding | [MOTION §8.1](../design/MOTION_CONTRACT.md#81-name-matching) |

### 5.7 USD boundary

| Code | Severity | Raised by | Defined in |
| --- | --- | --- | --- |
| `MMD_USD_IDENTIFIER_COLLISION` | info | `mmdModel` | [TEXT §6.1](../design/TEXT_ENCODING_POLICY.md#61-algorithm-contract-v1) |

Validation codes for the [stage checklist](../design/STAGE_CONTRACT.md#14-validation-checklist)
(default prim, up axis, unit, skel binding, material binding) are added with the
validator, in the `MMD_USD_` family.
