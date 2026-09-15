# Text, identifier and path policy

> Status: **proposed**; binding from Phase 1 (decoding) and Phase 2
> (identifiers, texture paths). It fixes how PMX text is decoded, how a source
> name relates to a USD identifier, how collisions are resolved, and how a
> texture string becomes an `SdfAssetPath`. Japanese names and Japanese
> filenames are the ordinary case here, not an edge case. Section numbers are
> stable.

---

## 1. Scope

Every string that crosses from a PMX file into the canonical model or the
stage: model, material, bone, morph, display-frame, rigid-body and joint names
and their English counterparts; comments and memos; texture paths. Also the
paths the importer and tools themselves open.

## 2. One internal representation

Text is decoded once, in `mmdPmx`, into **validated UTF-8 in `std::string`**,
and stays UTF-8 everywhere after that — canonical model, stage, diagnostics,
tools. There is no second encoding in the system, no `wchar_t` in any interface,
and no code page.

## 3. Decoding PMX text

The PMX header's encoding flag (globals[0]) selects the decoder for every
string in the file: `0` UTF-16LE, `1` UTF-8. Any other value is
`MMD_TEXT_INVALID_ENCODING_FLAG` (fatal).

**UTF-16LE.** The byte length must be even, and surrogates must pair; an odd
length or an unpaired surrogate is `MMD_TEXT_INVALID_UTF16`.

**UTF-8.** Overlong forms, encoded surrogates (U+D800–U+DFFF), code points
above U+10FFFF and truncated sequences are `MMD_TEXT_INVALID_UTF8`.

**Malformed text is rejected, never repaired.** The string is replaced by the
empty string in the canonical model and the diagnostic records the field, the
table index and the byte offset. There is no U+FFFD substitution: a substituted
name would look like data while silently being different data. The
consequences are local and deterministic — an invalid name falls back to an
index identifier (§6), an invalid texture path leaves its slot empty (§7) — so
the diagnostic is recoverable.

**Verbatim otherwise.** A valid string is preserved exactly: no trimming, no
case change, no Unicode normalization, no BOM stripping. One exception, applied
in canonicalization and not in the parser: trailing U+0000 characters, which
some writers use as padding, are dropped from display names with
`MMD_TEXT_TRAILING_NUL` (info). The syntax layer keeps them.

## 4. No locale, anywhere

Nothing depends on the process locale, the Windows ANSI code page, or the
filesystem's encoding. Concretely:

- MSVC targets compile with `/utf-8`, so source literals and narrow strings are
  UTF-8.
- On Windows, a `std::filesystem::path` is never constructed from a narrow
  `std::string` (that conversion uses the ANSI code page). Paths cross the API
  as `std::filesystem::path` or are converted from UTF-8 explicitly
  (`char8_t`).
- **The file format reads through `ArGetResolver().OpenAsset()`**, as the
  `usda` format itself does, and never opens the file by its narrow path. A
  plugin runs inside a host process whose code page is not the project's;
  `usd-vrm-plugins` measured that a `.vrm` under a non-ASCII directory could not
  be opened from any host until its importer read through `Ar`.
- Executables (`mmd_inspect`, later tools) embed a UTF-8 `activeCodePage`
  manifest on Windows.
- A Unicode-path test runs the importer, from a Python host, and every tool
  under a directory whose name no single ANSI code page can spell (for example
  `ユニコード-é`), and compares each result with an ASCII-directory twin. The
  Python-host leg is the one that catches importer regressions: a tool with the
  manifest would mask them. The test also calls
  `Sdf.FileFormat.FindByExtension("pmx").CanRead(path)` directly, because
  `Usd.Stage.Open` selects a format by extension and never reaches `CanRead`.

## 5. Identity versus display

A source name, an English name and a USD identifier are three different
things, and every canonical element carries all three:

```cpp
struct Name {
    std::string source;    // decoded source name, e.g. "左腕"
    std::string english;   // decoded English name, e.g. "LeftArm" — often empty
    std::string stableId;  // USD-safe identifier (§6)
};
```

- The original text is never lost: it is preserved byte-exact as provenance
  ([STAGE_CONTRACT.md §3](STAGE_CONTRACT.md#3-authoring-conventions)).
- The original text is never required to be a valid USD identifier.
- The filesystem's encoding never determines identity.
- Name-based matching (VMD → PMX, §9) compares **source names**, never
  identifiers.

## 6. Stable identifiers

### 6.1 Algorithm (contract v1)

For each element, in source-table order, per kind:

1. **Candidate from the English name.** Take the decoded English name; replace
   every maximal run of characters outside `[A-Za-z0-9_]` — spaces,
   punctuation, and every non-ASCII character — with one `_`; strip leading
   and trailing `_`; truncate to 64 characters; prefix `_` if the result
   starts with a digit.
2. **Fallback.** If the candidate is empty, the candidate is
   `<kind>_<NNNN>`: the kind prefix below and the source index, zero-padded to
   four digits (more when the index needs them).
3. **Collision.** Two identifiers of the same kind collide when they are equal
   **ignoring ASCII case** — so the stage stays unambiguous in case-insensitive
   tools and filesystems. The element with the lower source index keeps the
   candidate; a later one becomes `<candidate>_<sourceIndex>`, and if that is
   also taken, `<kind>_<NNNN>`, and if even that is taken, it gains `_2`,
   `_3`, … until it is free. Each renamed element raises
   `MMD_USD_IDENTIFIER_COLLISION` (info).

| Kind | Prefix | Used for |
| --- | --- | --- |
| material | `material` | `/Asset/mtl/<id>`, the material's subset |
| bone | `bone` | joint path components |
| morph | `morph` | `/Asset/morph/<id>`, blend-shape tokens |
| rigid body | `rigidBody` | `/Asset/physics/rigidBodies/<id>` |
| joint | `joint` | `/Asset/physics/joints/<id>` |

Identifiers are unique within their kind across the whole model, which is
stricter than USD needs for joint paths and keeps every element addressable by
identifier alone.

Examples:

| Source | English | Identifier |
| --- | --- | --- |
| `左腕` | `LeftArm` | `LeftArm` |
| `センター` | *(empty)* | `bone_0003` |
| `まばたき` | `Blink` | `Blink` |
| `笑い` | `smile 2` | `smile_2` |
| `髪` | `Hair` (second `hair` at index 40) | `Hair`, and `hair_40` |

### 6.2 Stability

The identifier is a pure function of the element's English name, its source
index, and the names of lower-indexed elements of the same kind — that is,
of the source bytes. It does not depend on the locale, the platform, or the
importer's hash order. Editing a model can change identifiers (renaming an
English name, inserting a bone); that is a different source, not instability.

### 6.3 What contract v1 deliberately does not do

- **No transliteration.** Romanizing `左腕` as `hidariude` needs a dictionary;
  the dictionary would become part of the stage ABI, and its every revision a
  contract bump.
- **No recognized-role table.** Mapping MMD's conventional bone names (`センター`,
  `上半身`, `左腕`) to role identifiers is useful, but the table is a policy that
  would be frozen into paths. Roles, if needed, belong in semantics a consumer
  reads, not in identifiers.
- **No UTF-8 identifiers**, although OpenUSD accepts UTF-8 prim names since
  24.03: visually identical names in different Unicode normal forms or
  widths would be different paths; many PMX names contain characters that are
  not valid identifier characters anyway (`・`, `（`, `＋`), so a sanitizer would
  still be needed; and not every tool and DCC in the pipeline accepts them.

Any of the three can be introduced later as a new contract version
([STAGE_CONTRACT.md §2](STAGE_CONTRACT.md#2-contract-version)). The
implementation policy's precedence listed transliteration and roles between
the English name and the fallback; contract v1 leaves both out for the reasons
above
([DESIGN_POLICY.md §19](DESIGN_POLICY.md#19-where-this-document-departs-from-the-implementation-policy)).

## 7. Texture paths

### 7.1 Preserve, then normalize

The decoded source string is preserved verbatim as provenance
(`mmd:sourceTexturePath` and siblings,
[MATERIAL_POLICY.md §4.2](MATERIAL_POLICY.md#42-provenance)), whatever happens
next. The authored path is a normalized **logical** path:

- `\` becomes `/`, and repeated `/` collapse;
- `.` segments are removed, and `x/..` pairs are resolved lexically;
- case is kept; Unicode is not normalized; nothing is renamed;
- the result is authored anchored to the PMX layer: `@./tex/髪.png@`.

The `./` prefix matters: `ArDefaultResolver` treats a relative path without it
as a search path, which would make resolution depend on the resolver's search
configuration.

### 7.2 Unsafe paths

A path is **unsafe** when, after normalization, it is absolute (`/…`),
drive-qualified (`C:…`), UNC (`//server/…`), carries a URI scheme
(`scheme:…`), or escapes the PMX's directory (a leading `..`). An unsafe path
is not dereferenced: no `SdfAssetPath` is authored for it, the slot stays empty
in every realization, the source string is still preserved, and
`MMD_PATH_UNSAFE_TEXTURE_PATH` (warning) is raised. The reader keeps the fact;
the resolver policy refuses to follow it.

Whether a model directory's parent should ever be reachable — some
distributions share a `../toon/` folder between models — is TEXT-O1. The
default is no, and a later opt-in would be a file-format argument, never a
silent default.

### 7.3 No filesystem access while authoring

Authoring never checks whether a texture exists. The authored asset path is a
function of the source string alone, so the same bytes produce the same stage
on any machine. Whether it resolves is a question for resolution time, and the
stage tests and validation ask it (`MMD_PATH_TEXTURE_NOT_FOUND`).

### 7.4 Known limitations, stated rather than worked around

- **Case.** A PMX authored on Windows may name `Tex.PNG` for a file called
  `tex.png`. On a case-sensitive filesystem it does not resolve. Paths are not
  lowercased (that would break the correct case) and no case-insensitive
  search is performed.
- **Unicode normal form.** A filename stored in NFD (for example after a trip
  through an older macOS filesystem) and referenced in NFC does not resolve on
  a byte-exact filesystem. No normalization is applied.
- **Archive mojibake.** Filenames garbled by extracting a Shift-JIS-named ZIP
  with the wrong code page are a property of the extracted files, not of the
  PMX, and are out of scope.

### 7.5 No package resolver

A PMX references external files; nothing is embedded. Standard `ArResolver`
behavior resolves the anchored relative paths, and no MMD resolver exists unless
a real source-container requirement appears
([DESIGN_POLICY.md §3](DESIGN_POLICY.md#3-relationship-to-usd-vrm-plugins)).

## 8. Diagnostics

| Code | Severity | Recoverable |
| --- | --- | --- |
| `MMD_TEXT_INVALID_ENCODING_FLAG` | fatal | no |
| `MMD_TEXT_INVALID_UTF8` | error | yes |
| `MMD_TEXT_INVALID_UTF16` | error | yes |
| `MMD_TEXT_TRAILING_NUL` | info | yes |
| `MMD_USD_IDENTIFIER_COLLISION` | info | yes |
| `MMD_PATH_UNSAFE_TEXTURE_PATH` | warning | yes |
| `MMD_PATH_TEXTURE_NOT_FOUND` | warning (validation) | yes |

The catalog is [reference/DIAGNOSTICS.md](../reference/DIAGNOSTICS.md).

## 9. PMD and VMD

PMD and VMD predate PMX's encoding flag: their strings are fixed-length,
NUL-padded **Shift-JIS (CP932)** fields, and a long name can be cut in the
middle of a two-byte character. That policy stays out of `mmdPmx` entirely:

- a PMD reader would be its own library, `libs/mmdPmd`;
- VMD is `motionVmd`'s ([MOTION_CONTRACT.md §4](MOTION_CONTRACT.md#4-text));
- both decode CP932 with a mapping table owned by the project — never an OS
  API, `iconv`, or the process locale — and drop a truncated trailing lead byte
  with a diagnostic.

## 10. Open questions

| Id | Question | Proposed answer | Resolve by |
| --- | --- | --- | --- |
| TEXT-O1 | May a texture path escape the model directory? | no by default; an explicit file-format argument later if needed | Phase 2 |
| TEXT-O2 | The 64-character identifier cap | keep; collisions after truncation use §6.1 step 3 | Phase 2 |
