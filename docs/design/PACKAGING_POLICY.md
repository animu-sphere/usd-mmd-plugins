# USDZ packaging policy

> Status: **proposed**, 2026-09-25. Nothing here is implemented. Each section
> becomes binding when the Phase 10 step that first implements it lands with a
> fixture ([DESIGN_POLICY.md §14](DESIGN_POLICY.md#14-phases)).
>
> This document owns `mmd_export`, the tool that writes an MMD model out as
> conventional OpenUSD, and the distribution boundary it sits on. Its first
> format, and the only one this document yet defines, is a self-contained
> USDZ. It is
> distilled from the 2026-09-25 USDZ packaging memo. §17 lists where it
> departs from the memo, and why. It was measured first, with OpenUSD 26.08's
> own packaging functions over locally held models
> ([report](../reports/2026-09-25-usdz-packaging-probe.md)).
>
> The stage a package carries is the one
> [STAGE_CONTRACT.md](STAGE_CONTRACT.md) fixes. On that stage, the stage
> contract wins. This document adds only what packaging does to it, and that
> is one thing: it rewrites the path of a texture it converts (§7).
>
> Section numbers are stable.

---

## 1. Scope

`mmd_export` takes a model this repository's importer opens (`.pmx`) and writes
it out in the format its output's extension names. Version 1 writes one
format, a USDZ archive (§10). The archive holds the stage the importer authors, as a
`.usdc`, and every texture that stage names. It opens in an OpenUSD
installation that has none of this repository's plugins.

Packaging is a **distribution** step. It is not import, and it is not
authoring. The importer's job is unchanged:

```text
MMD format ──usdMmdFileFormat──→ conventional OpenUSD         (import; DESIGN_POLICY.md §1)
conventional OpenUSD ──mmd_export──→ self-contained USDZ      (distribution; this document)
```

## 2. The boundary

- **A tool, not the file format.** `usdMmdFileFormat` never writes a package,
  and nothing about packaging enters it. `mmd_export` is its own executable in
  `tools/mmdExport/`
  ([WORKSPACE.md §1.2](../architecture/WORKSPACE.md#12-later-only-when-their-responsibility-is-real)).
- **What the importer authors, and nothing else.** The tool opens the model
  through the registered `SdfFileFormat`, as any OpenUSD host does, and links
  neither the parser, the canonical model nor the importer. So a package
  cannot disagree with `Usd.Stage.Open("model.pmx")`: it is that stage,
  written down.
- **No MMD source in the package.** The PMX never enters the archive, and
  nothing in the package depends on an MMD file format at run time. OpenUSD's
  own packaging, given a `.pmx` directly, stores the PMX bytes as the
  package's root layer, which only a host with this repository's importer can
  read ([report](../reports/2026-09-25-usdz-packaging-probe.md)). That is why
  §4 exists.
- **Standard USDZ, written by OpenUSD.** No ZIP writer, codec or archive
  layout of this repository's own. Discovery, the archive and validation are
  OpenUSD's (§6, §9); the image conversion is OpenUSD's `Hio` (§7).
- **Package is not flatten.** Materialization copies the importer's one
  layer, spec for spec (§4). It never composes a stage and flattens it.
  Today the two would agree, because the importer authors no composition arc.
  If it ever authors one, a flatten would silently bake it, while discovery
  stops the run and asks for a decision (§6).

## 3. Pipeline

```text
model.pmx ─Open─→ SdfLayer (usdMmdFileFormat, through Plug)
          ─Discover─→ the textures it names, resolved     UsdUtilsComputeAllDependencies
          ─Convert─→ PNG for a texture USDZ cannot hold    Hio (§7)
          ─Materialize─→ <name>.usdc                      SdfLayer::TransferContent + Save
          ─Package─→ <name>.usdz, in a private directory   SdfZipFileWriter
          ─Validate─→ the written package                  UsdStage, the usdchecker validators (§9)
          ─Move─→ the output path
```

Each step is a function of its own, callable without the command line (§12).
Nothing reaches the output path until validation passes: a failed run leaves
no file behind, and never a partial one.

## 4. Materialization

The importer returns an anonymous layer that exists only while its `.pmx` is
open. Materialization writes that layer's content, spec for spec, into a new
binary layer, `<name>.usdc`, in a private temporary directory. `<name>` is the
output file's stem: `mmd_export model.pmx out.usdz` writes `out.usdc`.

- **Nothing added, nothing dropped.** Stage metadata (`defaultPrim = "Asset"`,
  `upAxis = "Y"`, `metersPerUnit = 1`), `/Asset` and everything below it,
  `/Asset.customData["mmd:stageContractVersion"]`, `mmd:diagnostics` and every
  provenance key travel unchanged. The package adds no version, time,
  path or tool name of its own
  ([STAGE_CONTRACT.md §3](STAGE_CONTRACT.md#3-authoring-conventions)).
- **One change.** A converted texture's asset path is rewritten to the
  converted file's name (§7). The verbatim source path stays in the material's
  provenance, `mmd:sourceTexturePath` and its siblings
  ([MATERIAL_POLICY.md §4.2](MATERIAL_POLICY.md#42-provenance)).
- **Binary.** The root layer is `.usdc`, for size and load time. A package is
  a distribution artifact, not a file to edit.
- **The stage contract is the importer's.** A package carries whichever
  stage-contract version the importer stamped. A consumer that reads the
  version reads it from `/Asset`, as it would from the `.pmx`.

The same `.pmx` materializes to the same `.usdc` bytes on every run
([report](../reports/2026-09-25-usdz-packaging-probe.md)).

## 5. Package layout

```text
out.usdz
├── out.usdc                  the root layer, first entry
└── <every texture at the path the stage names it by, relative to the model's directory>
    e.g.  tex/髪.png
          toon/skin.png
          spa/hair_s.bmp.png   (converted, §7)
```

The layout mirrors the model's own directory. The alternative, one
`textures/` folder, would rewrite every asset path in the stage. The mirrored
layout rewrites none, because the stage's texture paths are already
package-ready: every authored texture path is `./`-relative and stays inside
the model's directory. The importer refuses an absolute path, a drive, a URI
and any `..` that escapes, and authors nothing for it
([TEXT_ENCODING_POLICY.md §7.2](TEXT_ENCODING_POLICY.md#72-unsafe-paths)). Its
path then resolves inside the package exactly as it did beside the `.pmx`.
When OpenUSD's own packaging is given the same stage in place, it produces
this layout too.

- **Names are kept.** Japanese and other non-ASCII names are stored as they
  are, in UTF-8, and resolve inside the package with no plugin
  ([report](../reports/2026-09-25-usdz-packaging-probe.md)). OpenUSD's ZIP
  writer does not set the ZIP UTF-8 flag. So a general ZIP tool may show
  those names garbled, while every OpenUSD reader resolves them (PKG-O2).
- **One entry per file.** A texture that several materials name is stored
  once.
- **Order.** The root layer first, as the USDZ specification requires, then
  the textures in byte order of their archive paths.

## 6. Discovery and localization

`UsdUtilsComputeAllDependencies`, run on the `.pmx`, reports every file the
imported layer names, resolved against the model's directory, and every path
that does not resolve
([report](../reports/2026-09-25-usdz-packaging-probe.md)). Because the layout
is the model's own (§5), localization is a copy. Each resolved file enters
the archive at the path the stage names it by, less the leading `./`.

- **Missing is an error.** A path the stage authors that does not resolve is
  `MMD_PKG_MISSING_ASSET`, and no package is written. A package that silently
  drops a texture draws differently from the stage it came from.
  `--allow-missing-assets` is later (§16).
- **Refused paths are already absent.** A texture path the importer refused
  has no asset path on the stage, so there is nothing to package. The
  importer's `MMD_PATH_UNSAFE_TEXTURE_PATH` is relayed (§11), and the package
  draws as the stage does.
- **Shared toon ramps are an index, not a file.** MMD's `toon01.bmp`–
  `toon10.bmp` belong to MMD and are not redistributed. A shared slot is
  `inputs:mmd:material:sharedToonIndex`, and it travels as an integer
  ([MATERIAL_POLICY.md §7](MATERIAL_POLICY.md#7-toon-ramps)).
- **Case and Unicode form.** What does not resolve beside the `.pmx` does not
  resolve for packaging either
  ([TEXT_ENCODING_POLICY.md §7.4](TEXT_ENCODING_POLICY.md#74-known-limitations-stated-rather-than-worked-around)).
- **Anything else is a surprise.** The importer authors no sublayer,
  reference or payload. If discovery reports any layer other than the input,
  the importer has changed underneath this tool. That is
  `MMD_PKG_UNEXPECTED_DEPENDENCY`, and no package is written until this
  document says what such a layer becomes.

## 7. Texture formats

USDZ holds images only as PNG, JPEG, OpenEXR or AVIF: OpenUSD 26.08's
`usdchecker` refuses any other extension inside a package. MMD models use BMP
widely: 31 of the 41 locally held PMX files, from 14 of 15 distributions,
name at least one BMP, and 92 of their 275 texture references are BMP
([report](../reports/2026-09-25-usdz-packaging-probe.md)). MMD also names
sphere maps `.spa` and `.sph`, and those files are usually BMP.

Decided 2026-09-25: a texture USDZ cannot hold is **converted to PNG,
losslessly**, rather than stored as it is (non-compliant) or refused.

| Source file | Enters the package as |
| --- | --- |
| content PNG under `.png`, or JPEG under `.jpg` or `.jpeg` | the file itself, byte for byte |
| extension `.exr` or `.avif` | the file itself, byte for byte |
| content BMP (`BM` signature), whatever its extension (`.bmp`, `.spa`, `.sph`, …) | a PNG, converted |
| extension `.tga` | a PNG, converted |
| content PNG or JPEG under any other extension (`.spa`, `.sph`, …) | the file itself, byte for byte, renamed |
| anything else (DDS, GIF, …) | `MMD_PKG_UNSUPPORTED_TEXTURE`; no package |

- **Lossless.** `Hio` decodes the source and writes it as PNG. The PNG must
  decode to the same 8-bit values as the source, alpha included when the
  source has it. The tests compare the decoded pixels, not the files.
- **Named by appending.** A converted file gains `.png`: `toon/skin.bmp`
  becomes `toon/skin.bmp.png`. A renamed file gains the extension of its
  content: `spa/s.spa` holding a PNG becomes `spa/s.spa.png`, and holding a
  JPEG, `spa/s.spa.jpg`. A name made this way does
  not collide with another source file, unless the model already has a file
  of that exact name. That is `MMD_PKG_ASSET_NAME_COLLISION`, and no package
  is written.
- **Only the path changes.** The stage's asset path for that slot is
  rewritten to the new name (§4). The slot's `colorSpace`, the realizations'
  connections, and the verbatim source path in provenance stay as they are.
  In contract v2 the realizations read the texture through the canonical
  input, so the one rewrite reaches them all
  ([MATERIAL_POLICY.md §5](MATERIAL_POLICY.md#5-usdpreviewsurface-realization)).
- **One diagnostic per file.** `MMD_PKG_TEXTURE_CONVERTED` (info) names each
  converted file once.
- **No other conversion.** A PNG or JPEG is never re-encoded, resized or
  recompressed.

## 8. What a consumer without this repository sees

The package needs neither `usdMmdFileFormat` nor `mmdSchema` to open. Opened
with neither on the plugin path
([report](../reports/2026-09-25-usdz-packaging-probe.md)):

- the stage composes with no error, `defaultPrim` is `Asset`, and every
  texture path resolves inside the package;
- the mesh, skeleton, skinning, blend shapes and both realization graphs are
  standard schemas. A generic USD tool and a MaterialX-aware renderer read
  them as they read the importer's stage
  ([DESIGN_POLICY.md §11](DESIGN_POLICY.md#11-interoperability-levels),
  levels 1–2);
- `MmdMaterialAPI` stays in each material's `apiSchemas`, but an
  installation without `mmdSchema` does not recognize it, and
  `GetAppliedSchemas()` omits it. The `inputs:mmd:material:*` values are
  still there as ordinary attributes, and the graphs still connect to them;
- an MMD-aware consumer (level 3) installs `mmdSchema`, the schema bundle
  alone, and never the importer.

A package never carries a plugin.

## 9. Validation

Validation always runs. There is no flag to ask for it and none to skip it.

Before the package is written, on the materialized layer:

- the layer opens as a stage with no composition error;
- `defaultPrim == "Asset"`, `/Asset` exists, `upAxis == "Y"`,
  `metersPerUnit == 1`, and `/Asset` carries `mmd:stageContractVersion`;
- every asset path resolves (§6).

After it is written, on the package in its private directory:

- the package opens as a stage, and its root layer is the package's first
  entry;
- every layer the stage uses is inside the package, so no MMD source is
  needed to build it;
- every asset-valued attribute resolves to a file inside the package, and no
  absolute path remains;
- every validator `usdchecker` runs by default passes, the package-extension
  check included.

A failure is `MMD_PKG_VALIDATION_FAILED`, naming the check, and the output
path is left untouched.

Only a process with no MMD plugin on its path proves that a package opens
without the plugin. So that proof is a test (§14), not a per-run check.

## 10. Command line

```text
mmd_export <input.pmx> <output.usdz>
```

- The output's extension names the format. Version 1 writes `.usdz` alone,
  and any other extension is a usage error until a section here defines it
  (§16). An existing file at the output path is replaced only after
  validation passes.
- `--help` and `--version` are the only options in version 1. Every
  archive-level detail is OpenUSD's, including compression, alignment and
  entry method, and the command line exposes none of them.
- The importer must be on OpenUSD's plugin path, as it is for any host. The
  installed product arranges this; a development tree sets
  `PXR_PLUGINPATH_NAME`. Without it, `.pmx` has no file format, and the run
  fails with `MMD_PKG_INPUT_UNREADABLE`.
- Paths are Unicode on every platform. On Windows the executable embeds the
  UTF-8 `activeCodePage` manifest, as `mmd_inspect` does, and never builds a
  path from a narrow string
  ([TEXT_ENCODING_POLICY.md §4](TEXT_ENCODING_POLICY.md#4-no-locale-anywhere)).

## 11. Diagnostics and exit status

The tool prints every diagnostic, `CODE: message`, as the other tools do. It
exits non-zero on any `error` or `fatal`
([reference/DIAGNOSTICS.md §2](../reference/DIAGNOSTICS.md#2-severity)). For
this tool an `error` also means no package is written. The importer's own
recoverable diagnostics, recorded in `/Asset.customData["mmd:diagnostics"]`,
are printed first, unchanged. They describe the stage, which is also the
package's stage.

| Code | Severity | Raised when |
| --- | --- | --- |
| `MMD_PKG_INPUT_UNREADABLE` | fatal | the input does not exist, no registered file format opens it, or the importer fails to read it (its own fatal code is printed first) |
| `MMD_PKG_MISSING_ASSET` | error | an asset path the stage authors does not resolve (§6) |
| `MMD_PKG_UNEXPECTED_DEPENDENCY` | error | discovery reports a layer other than the input (§6) |
| `MMD_PKG_UNSUPPORTED_TEXTURE` | error | a texture is neither USDZ-ready nor convertible (§7) |
| `MMD_PKG_ASSET_NAME_COLLISION` | error | a converted or renamed texture's name is taken (§7) |
| `MMD_PKG_VALIDATION_FAILED` | error | a §9 check fails |
| `MMD_PKG_WRITE_FAILED` | fatal | the `.usdc`, a converted image or the archive cannot be written |
| `MMD_PKG_TEXTURE_CONVERTED` | info | a texture was converted to PNG (§7) |

`MMD_PATH_TEXTURE_NOT_FOUND` stays the stage validator's warning. Packaging
needs an error for the same event, and a code never changes severity, so
packaging has a code of its own.

## 12. Implementation boundary

- **One executable, one private library.** The steps of §3 live in a
  tool-private static library in `tools/mmdExport/`, each a function with a
  narrow input and output:

  ```cpp
  // tools/mmdExport/src -- private to the tool; not installed.
  SdfLayerRefPtr     OpenModel(const fs::path& input, Diagnostics*);
  PackagePlan        Discover(const SdfLayerHandle&, Diagnostics*);  // files, archive paths, conversions
  bool               ConvertTextures(PackagePlan*, const fs::path& scratch, Diagnostics*);
  SdfLayerRefPtr     Materialize(const SdfLayerHandle&, const PackagePlan&, const fs::path& scratch, Diagnostics*);
  bool               WritePackage(const SdfLayerHandle& root, const PackagePlan&, const fs::path& out, Diagnostics*);
  bool               ValidatePackage(const fs::path& usdz, Diagnostics*);
  ```

  `main` parses the command line and calls them in order. The unit tests call
  each one directly.
- **No public packaging library yet.** `usd-vrm-plugins`' `vrm_export`
  may one day want the same archive steps. A shared library is extracted when
  it does, from two working tools (§16), not before.
- **Why not `UsdUtilsCreateNewUsdzPackage`.** It was the first thing
  measured, and it does discovery and archiving in one call. It gives no
  control over entry order or timestamps, which §13 needs. Given a stage
  whose texture paths are absolute, it scatters the files into numbered
  folders (`0/`, `1/`). And from a Python host on Windows it failed when the
  output path was not ASCII, while `SdfZipFileWriter`, which it uses
  underneath, wrote the same path. That host had no UTF-8 code page, so the
  tool's manifest may avoid the failure, but nothing here relies on that
  ([report](../reports/2026-09-25-usdz-packaging-probe.md)). The tool
  therefore calls OpenUSD's lower-level functions itself: discovery with
  `UsdUtilsComputeAllDependencies`, the archive with `SdfZipFileWriter`.
  Both are OpenUSD's, so this is not a ZIP writer of the tool's own.
- **Build.** `mmd_export` is built after OpenUSD is resolved and ships in the
  product beside `mmd_inspect`. Its tests need the importer and schema
  bundles built, as the stage tests do.

## 13. Determinism

The same input should give the same package, byte for byte, so that a
release artifact can be checked. The materialized `.usdc` already is
([report](../reports/2026-09-25-usdz-packaging-probe.md)). The archive is not
yet. `SdfZipFileWriter` records each file's modification time, converted
from local time into the ZIP's DOS format. A copy made during the run
therefore carries the current time. Two runs of OpenUSD's one-call packager
on the same model, four seconds apart, gave archives whose entries differed
in that field and in no other. Phase 10 step 2
makes the archive deterministic (PKG-O1). Until then, §5's entry order is
already fixed.

## 14. Testing

Fixtures stay synthetic
([DESIGN_POLICY.md §13](DESIGN_POLICY.md#13-testing-policy)):
`generate_fixtures.py` writes the PMX files, and writes the textures they
name, including BMP, a `.spa` holding BMP, TGA, non-ASCII names, a texture
two materials share, and one missing file.

- **Unit.** Materialization keeps every spec (layer diff empty except the
  rewritten asset paths). Each row of §7 gives its outcome, and a converted
  PNG decodes to the source's pixels. The name-collision, missing-file and
  unsupported-format cases write nothing.
- **Integration.** `mmd_export` runs on each fixture. A **separate process with
  no MMD plugin on its path** opens the result, checks §9's post-write list,
  and runs `usdchecker`.
- **Compatibility.** For each fixture, the `.pmx` opened with the plugins and
  the `.usdz` opened without them have the same prim paths, types, applied
  schema tokens, metadata and property values. Texture asset values are
  compared through §7's renaming.
- **Non-ASCII paths.** An input directory and an output path that no single
  ANSI code page can spell, as
  [the stage tests](../../tests/integration/test_unicode_paths.py) already use.
- **Local models.** A dated report records the tool over the locally held
  models. Models distributed by their creators are never committed.

## 15. Non-goals

- A general converter. `mmd_export` writes conventional OpenUSD and nothing
  else: never PMX, FBX, glTF or another non-USD format. A USD format other
  than USDZ joins it only with its own section here (§16).
- A ZIP, image codec or archive layout of this repository's own.
- The PMX, or any MMD source, inside the package.
- Flattening composition as the default.
- Image conversion beyond §7: no resizing, recompression, or conversion of a
  format USDZ already holds.
- Renderer-specific baking, such as a toon realization or MToon.
- MMD's shared toon ramps, any plugin, or VMD motion inside the package.
- ARKit's stricter profile (`UsdUtilsCreateNewARKitUsdzPackage`).

## 16. Later

Each is added only when a user needs it, with its own section here first:

- `.usdc` and `.usda` output, when `usdcat` over the file format proves
  insufficient: the materialized layer written to the output path, with the
  textures it names written beside it at the same relative paths, so it
  resolves anywhere it is moved together with them. This is what
  WORKSPACE.md's former `mmd_convert` reservation was for;
- `--portable-paths`: ASCII archive names (`textures/tex_0001.png`), with the
  asset paths rewritten to match (the answer to PKG-O2 for tools that are not
  OpenUSD);
- `--flatten`, for consumers that cannot compose;
- `--report <file.json>`: the files packaged, those converted, and every
  diagnostic;
- `--material preview|native`, if a package ever needs a different material
  realization;
- `--allow-missing-assets`;
- PMD input, once `mmdPmd` exists
  ([WORKSPACE.md §1.2](../architecture/WORKSPACE.md#12-later-only-when-their-responsibility-is-real));
- a packaging library shared with `usd-vrm-plugins`, once its `vrm_export`
  packages too and the common part is visible in both.

## 17. Where this document departs from the packaging memo

| Memo | Here | Why |
| --- | --- | --- |
| §18 — no texture transcoding | Lossless PNG conversion of a texture USDZ cannot hold (§7) | The memo also guarantees a standard-compliant USDZ (§14), and `usdchecker` refuses BMP, which 31 of the 41 local PMX files name. Decided by the user on 2026-09-25. |
| §8 — textures under one `textures/` folder | The model's own relative layout (§5) | The stage's paths are already relative and inside the model's directory, so this layout rewrites no path and keeps every name, as the memo's §9 asks for version 1. A flat folder is `--portable-paths`. |
| §5 — `--validate` as an option | Validation always runs (§9) | The memo's version-1 guarantees (§14–§15) are validation results. An unvalidated package would claim them without checking. |
| §3, §14 — `.pmx` or `.pmd` input | `.pmx` only | This repository has no PMD reader yet (`mmdPmd` is reserved). The tool accepts whatever the registered file formats open, so PMD follows `mmdPmd` with no change here. |
| §10 — OpenUSD's USDZ packaging API | OpenUSD's discovery and ZIP writer, called step by step (§12) | The one-call API gives no control over entry order or timestamps, so the archive cannot be made deterministic, and it failed on a non-ASCII output path from a Python host. Still no ZIP writer of this repository's own. |
| §11 — `tools/mmd_usdz/` | `tools/mmdExport/` | Executables are `snake_case` in a lower-camel directory, like `tools/mmdInspect/` ([WORKSPACE.md §1.2](../architecture/WORKSPACE.md#12-later-only-when-their-responsibility-is-real)). |
| §3 — the tool is `mmd_usdz`; §4 names `mmd_export` the better choice once the tool writes more than one format | `mmd_export` (§1, §10), writing `.usdz` only in version 1 | It pairs with `usd-vrm-plugins`' `vrm_export`, which writes `.usda`, `.usdc` and `.usdz` from one tool, so `usd-avatar-runtime` and a user see one convention in both repositories. The format follows the output's extension, so more formats need no new tool, and WORKSPACE.md's `mmd_convert` reservation is folded in. Decided by the user on 2026-09-26. |
| §13 — links `usdMmd` and friends | Links OpenUSD only, and reaches the importer through Plug | The importer is a plugin bundle, not a library to link. Opening it as any host does is also what guarantees the package matches `Usd.Stage.Open` (§2). |
| §21 — Phase 1–4 | Phase 10, steps 1–4 | This repository has one phase sequence ([DESIGN_POLICY.md §14](DESIGN_POLICY.md#14-phases)). |

## 18. Open questions

| Id | Question | Proposed answer | Resolve by |
| --- | --- | --- | --- |
| PKG-O1 | How the archive becomes byte-deterministic | Copy every entry into the private directory and give each copy one fixed modification time before `SdfZipFileWriter` adds it. The time is built from a fixed *local* date, so the DOS time, which is converted from local time, is the same in every time zone. Entry order is §5's. | Phase 10 step 2 |
| PKG-O2 | Non-ASCII archive names in ZIP tools that are not OpenUSD | Keep UTF-8 names (OpenUSD reads them). A consumer that needs ASCII uses `--portable-paths`. The flag is not patched into OpenUSD's writer. | Phase 10 step 3 |
