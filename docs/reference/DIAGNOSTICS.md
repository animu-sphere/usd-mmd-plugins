# Diagnostics

Every diagnostic the parser, the canonicalizer, the importer, the validator or
a tool reports carries a **stable code** with a **fixed severity**. The code is
the contract; the message is human-readable detail and may change at any time.
Tests assert codes, never prose.

Status (2026-09-17): the record below is code, and every code marked
*emitted* in §5 is raised by the current tree — the parser raises all of its
PMX-syntax and text-decoding codes, the canonical model its identifier, path,
skeleton, weight, material and morph codes, the importer its skinning,
blend-shape and soft-body ones, the VMD reader its motion-syntax and CP932
ones, and binding its name-matching ones. Every other code is *reserved* by a
design document, which is where its meaning is fixed. A code joins its
component's declarations —
[libs/mmdPmx/include/mmdPmx/Codes.h](../../libs/mmdPmx/include/mmdPmx/Codes.h),
[libs/mmdModel/include/mmdModel/Codes.h](../../libs/mmdModel/include/mmdModel/Codes.h),
[libs/motionVmd/include/motionVmd/Codes.h](../../libs/motionVmd/include/motionVmd/Codes.h),
[libs/mmdMotionBinding/include/mmdMotionBinding/Codes.h](../../libs/mmdMotionBinding/include/mmdMotionBinding/Codes.h),
[plugins/usdMmdFileFormat/src/usd/UsdMmdCodes.h](../../plugins/usdMmdFileFormat/src/usd/UsdMmdCodes.h)
— with the code that raises it, and
[scripts/check_docs.py](../../scripts/check_docs.py) fails when those
declarations and §5 disagree: a declared code missing from the catalog, not
marked *emitted*, or listed with another severity, and an *emitted* code
nothing declares.

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

`motionVmd` declares the same record and `Result<T>` in its own namespace
([motionVmd/Diagnostic.h](../../libs/motionVmd/include/motionVmd/Diagnostic.h)),
with `section` where this record has `table`: a VMD parses without a model,
so it may not depend on `mmdPmx`
([MOTION_CONTRACT.md §2](../design/MOTION_CONTRACT.md#2-components-and-boundaries)).
Its codes follow this catalog all the same, and
`mmd::binding::ToDiagnostic` carries one into this record field for field.

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
| `MMD_MOTION_` | VMD and motion binding (Phase 7), control evaluation (Phase 9) |
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
- **Ordered by stage.** The list records the parser's diagnostics, then the
  canonical model's, then the importer's, each in the order it raised them.
- **Bounded.** One code is recorded at most 16 times per table; the rest of its
  occurrences in that table are counted, and reported once, after everything
  else, as that code with the table as its location and the number not listed
  as its message. A malformed file can otherwise raise one diagnostic per
  vertex, and each recorded diagnostic is a string on the stage. Every element
  is still repaired as its section says, listed or not. The parser, the
  canonical model and binding bound their lists with the same
  [`mmd::DiagnosticList`](../../libs/mmdPmx/include/mmdPmx/DiagnosticList.h);
  `motionVmd` bounds its own the same way, per section.
- **Tools** (`mmd_inspect`, `vmd_inspect`) print every diagnostic and set their
  exit status by the most severe one.
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
| `MMD_PMX_TRUNCATED_BUFFER` *emitted* | fatal | `mmdPmx` | [PMX §2](../design/PMX_CONTRACT.md#2-reading-rules) |
| `MMD_PMX_COUNT_EXCEEDS_BUFFER` *emitted* | fatal | `mmdPmx` | [PMX §2](../design/PMX_CONTRACT.md#2-reading-rules) |
| `MMD_PMX_TRAILING_BYTES` *emitted* | warning | `mmdPmx` | [PMX §2](../design/PMX_CONTRACT.md#2-reading-rules) |
| `MMD_PMX_INVALID_DEFORM_TYPE` *emitted* | fatal | `mmdPmx` | [PMX §5](../design/PMX_CONTRACT.md#5-vertices-and-deform) |
| `MMD_PMX_FACE_COUNT_NOT_TRIANGLES` *emitted* | fatal | `mmdPmx` | [PMX §6](../design/PMX_CONTRACT.md#6-faces) |
| `MMD_PMX_FACE_INDEX_OUT_OF_RANGE` *emitted* | fatal | `mmdPmx` | [PMX §6](../design/PMX_CONTRACT.md#6-faces) |
| `MMD_PMX_MATERIAL_FACES_EXCEED_TABLE` *emitted* | fatal | `mmdPmx` | [PMX §8](../design/PMX_CONTRACT.md#8-materials) |
| `MMD_PMX_MATERIAL_FACES_SHORT` *emitted* | error | `mmdPmx` | [PMX §8](../design/PMX_CONTRACT.md#8-materials) |
| `MMD_PMX_INVALID_MORPH_TYPE` *emitted* | fatal | `mmdPmx` | [PMX §10](../design/PMX_CONTRACT.md#10-morphs) |
| `MMD_PMX_INVALID_LAYOUT_FLAG` *emitted* | fatal | `mmdPmx` | [PMX §2](../design/PMX_CONTRACT.md#2-reading-rules) |
| `MMD_PMX_INDEX_OUT_OF_RANGE` *emitted* | error | `mmdPmx` | [PMX §4](../design/PMX_CONTRACT.md#4-indices) |
| `MMD_PMX_FILE_UNREADABLE` *emitted* | fatal | `mmdPmx` (`ReadFile`) | [PMX §2](../design/PMX_CONTRACT.md#2-reading-rules) |

### 5.2 Text

| Code | Severity | Raised by | Defined in |
| --- | --- | --- | --- |
| `MMD_TEXT_INVALID_ENCODING_FLAG` *emitted* | fatal | `mmdPmx` | [TEXT §3](../design/TEXT_ENCODING_POLICY.md#3-decoding-pmx-text) |
| `MMD_TEXT_INVALID_UTF8` *emitted* | error | `mmdPmx` | [TEXT §3](../design/TEXT_ENCODING_POLICY.md#3-decoding-pmx-text) |
| `MMD_TEXT_INVALID_UTF16` *emitted* | error | `mmdPmx` | [TEXT §3](../design/TEXT_ENCODING_POLICY.md#3-decoding-pmx-text) |
| `MMD_TEXT_TRAILING_NUL` *emitted* | info | `mmdModel` | [TEXT §3](../design/TEXT_ENCODING_POLICY.md#3-decoding-pmx-text) |
| `MMD_TEXT_TRUNCATED_CP932` *emitted* | info | `motionVmd` | [MOTION §4](../design/MOTION_CONTRACT.md#4-text) |
| `MMD_TEXT_INVALID_CP932` *emitted* | error | `motionVmd` | [MOTION §4](../design/MOTION_CONTRACT.md#4-text) |

### 5.3 Paths

| Code | Severity | Raised by | Defined in |
| --- | --- | --- | --- |
| `MMD_PATH_UNSAFE_TEXTURE_PATH` *emitted* | warning | `mmdModel` | [TEXT §7.2](../design/TEXT_ENCODING_POLICY.md#72-unsafe-paths) |
| `MMD_PATH_TEXTURE_NOT_FOUND` | warning | validation | [TEXT §7.3](../design/TEXT_ENCODING_POLICY.md#73-no-filesystem-access-while-authoring) |

### 5.4 Skeleton and skinning

| Code | Severity | Raised by | Defined in |
| --- | --- | --- | --- |
| `MMD_SKEL_INVALID_PARENT` *emitted* | error | `mmdModel` | [STAGE §9.1](../design/STAGE_CONTRACT.md#91-canonical-joint-order) |
| `MMD_SKEL_PARENT_CYCLE` *emitted* | error | `mmdModel` | [STAGE §9.1](../design/STAGE_CONTRACT.md#91-canonical-joint-order) |
| `MMD_SKEL_JOINTS_REORDERED` *emitted* | info | `mmdModel` | [STAGE §9.1](../design/STAGE_CONTRACT.md#91-canonical-joint-order) |
| `MMD_SKEL_WEIGHTS_NORMALIZED` *emitted* | info | `mmdModel` | [STAGE §9.5](../design/STAGE_CONTRACT.md#95-weight-normalization) |
| `MMD_SKEL_ZERO_WEIGHTS` *emitted* | warning | `mmdModel` | [STAGE §9.5](../design/STAGE_CONTRACT.md#95-weight-normalization) |
| `MMD_SKEL_SDEF_APPROXIMATED` *emitted* | warning | `usdMmdFileFormat` | [STAGE §9.4](../design/STAGE_CONTRACT.md#94-skinning) |
| `MMD_SKEL_QDEF_APPROXIMATED` *emitted* | warning | `usdMmdFileFormat` | [STAGE §9.4](../design/STAGE_CONTRACT.md#94-skinning) |

### 5.5 Morphs, materials, physics

| Code | Severity | Raised by | Defined in |
| --- | --- | --- | --- |
| `MMD_MORPH_NO_SKELETON` *emitted* | warning | `usdMmdFileFormat` | [STAGE §4.1](../design/STAGE_CONTRACT.md#41-why-asset-is-the-skelroot) |
| `MMD_MORPH_UNKNOWN_PANEL` *emitted* | warning | `mmdModel` | [PMX §10](../design/PMX_CONTRACT.md#10-morphs) |
| `MMD_MORPH_GROUP_CYCLE` *emitted* | error | `mmdModel` | [PMX §10](../design/PMX_CONTRACT.md#10-morphs) |
| `MMD_MATERIAL_UNSUPPORTED_SPHERE_MODE` *emitted* | warning | `mmdModel` | [PMX §8](../design/PMX_CONTRACT.md#8-materials) |
| `MMD_MATERIAL_UNSUPPORTED_TOON_SLOT` *emitted* | warning | `mmdModel` | [PMX §8](../design/PMX_CONTRACT.md#8-materials) |
| `MMD_PHYSICS_SOFT_BODY_UNSUPPORTED` *emitted* | warning | `usdMmdFileFormat` | [PMX §12](../design/PMX_CONTRACT.md#12-soft-bodies-21) |
| `MMD_PHYSICS_JOINT_UNMAPPED` *emitted* | warning | `usdMmdFileFormat` | [STAGE §13.2](../design/STAGE_CONTRACT.md#132-joints) |
| `MMD_PHYSICS_UNKNOWN_SHAPE` *emitted* | warning | `mmdModel` | [PMX §13](../design/PMX_CONTRACT.md#13-rigid-bodies-and-joints) |
| `MMD_PHYSICS_UNKNOWN_MODE` *emitted* | warning | `mmdModel` | [PMX §13](../design/PMX_CONTRACT.md#13-rigid-bodies-and-joints) |
| `MMD_PHYSICS_UNKNOWN_JOINT_TYPE` *emitted* | warning | `mmdModel` | [PMX §13](../design/PMX_CONTRACT.md#13-rigid-bodies-and-joints) |

### 5.6 Motion (Phases 7 and 9)

| Code | Severity | Raised by | Defined in |
| --- | --- | --- | --- |
| `MMD_MOTION_BAD_SIGNATURE` *emitted* | fatal | `motionVmd` | [MOTION §3](../design/MOTION_CONTRACT.md#3-vmd-source-facts) |
| `MMD_MOTION_TRUNCATED_BUFFER` *emitted* | fatal | `motionVmd` | [MOTION §3](../design/MOTION_CONTRACT.md#3-vmd-source-facts) |
| `MMD_MOTION_COUNT_EXCEEDS_BUFFER` *emitted* | fatal | `motionVmd` | [MOTION §3](../design/MOTION_CONTRACT.md#3-vmd-source-facts) |
| `MMD_MOTION_TRAILING_BYTES` *emitted* | warning | `motionVmd` | [MOTION §3](../design/MOTION_CONTRACT.md#3-vmd-source-facts) |
| `MMD_MOTION_FILE_UNREADABLE` *emitted* | fatal | `motionVmd` (`ReadFile`) | [MOTION §3](../design/MOTION_CONTRACT.md#3-vmd-source-facts) |
| `MMD_MOTION_DUPLICATE_KEYFRAME` *emitted* | warning | `motionVmd` (`BuildMotion`) | [MOTION §5](../design/MOTION_CONTRACT.md#5-time) |
| `MMD_MOTION_UNMATCHED_BONE` *emitted* | info | `mmdMotionBinding` | [MOTION §8.1](../design/MOTION_CONTRACT.md#81-name-matching) |
| `MMD_MOTION_UNMATCHED_MORPH` *emitted* | info | `mmdMotionBinding` | [MOTION §8.1](../design/MOTION_CONTRACT.md#81-name-matching) |
| `MMD_MOTION_AMBIGUOUS_NAME` *emitted* | warning | `mmdMotionBinding` | [MOTION §8.1](../design/MOTION_CONTRACT.md#81-name-matching) |
| `MMD_MOTION_UNENCODABLE_NAME` *emitted* | info | `mmdMotionBinding` | [MOTION §8.1](../design/MOTION_CONTRACT.md#81-name-matching) |
| `MMD_MOTION_EXTERNAL_PARENT_IGNORED` *emitted* | info | `mmdControl` (`Prepare`) | [MOTION §11.8](../design/MOTION_CONTRACT.md#118-diagnostics) |
| `MMD_MOTION_LOCAL_APPEND_APPROXIMATED` *emitted* | info | `mmdControl` (`Prepare`) | [MOTION §11.8](../design/MOTION_CONTRACT.md#118-diagnostics) |
| `MMD_MOTION_IK_LOOP_CLAMPED` *emitted* | warning | `mmdControl` (`Prepare`) | [MOTION §11.8](../design/MOTION_CONTRACT.md#118-diagnostics) |
| `MMD_MOTION_INVALID_SAMPLE_RANGE` *emitted* | fatal | `mmdMotionAdapter` (`BuildClip`) | [MOTION §10.8](../design/MOTION_CONTRACT.md#108-diagnostics) |
| `MMD_MOTION_NON_FINITE_SAMPLE` *emitted* | fatal | `mmdMotionAdapter` (`BuildClip`) | [MOTION §10.8](../design/MOTION_CONTRACT.md#108-diagnostics) |
| `MMD_MOTION_MISSING_REQUIRED_JOINT` *emitted* | warning | `mmdMotionAdapter` (`BuildClip`) | [MOTION §12.4](../design/MOTION_CONTRACT.md#124-required-joints) |

### 5.7 USD boundary

| Code | Severity | Raised by | Defined in |
| --- | --- | --- | --- |
| `MMD_USD_IDENTIFIER_COLLISION` *emitted* | info | `mmdModel` | [TEXT §6.1](../design/TEXT_ENCODING_POLICY.md#61-algorithm-contract-v1) |

Validation codes for the [stage checklist](../design/STAGE_CONTRACT.md#14-validation-checklist)
(default prim, up axis, unit, skel binding, material binding) are added with the
validator, in the `MMD_USD_` family.
