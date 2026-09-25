# USDZ packaging probe (2026-09-25)

Dated evidence from real runs; append-only
([contributing/documentation.md](../contributing/documentation.md)).

## Question

The 2026-09-25 packaging memo asks for a tool, `mmd_usdz`, that turns a PMX
into a USDZ that opens without this repository's plugins, using OpenUSD's own
packaging. Before
[PACKAGING_POLICY.md](../design/PACKAGING_POLICY.md) was written, five things
needed checking:

1. What OpenUSD's packaging does with a `.pmx` as it stands.
2. Whether a materialized stage packages with its textures, and opens without
   the plugins.
3. Whether non-ASCII names survive.
4. Which texture formats local models use, and whether USDZ holds them.
5. Whether the result is deterministic.

## What was run

- **OpenUSD 26.08**, its Python bindings, `usdchecker`, and this repository's
  `usdMmdFileFormat` and `mmdSchema` bundles built from `main` at `97c5462`
  (stage-contract v2). "Without the plugins" means a process whose
  `PXR_PLUGINPATH_NAME` names neither bundle.
- **Packaging** through `UsdUtils.CreateNewUsdzPackage`,
  `UsdUtils.ComputeAllDependencies` and `Sdf.ZipFileWriter`, from scratch
  Python scripts that are not committed.
- **Models.** The 15 locally held distributions, 41 PMX files in all. Two
  were packaged end to end. Model A uses PNG only and sits in a
  directory with a Japanese name. Model B uses BMP, and names textures in
  CJK characters. All 41 went through dependency discovery. They are
  distributed by their creators, so they are neither committed nor named.

## Results

### 1. A `.pmx` given directly is stored as it is

`CreateNewUsdzPackage` on model A's `.pmx` succeeds. It collects every
texture, and stores the PMX bytes as the package's first entry: the root
layer is the `.pmx` itself. Only a host with the importer can read that
package. Packaging needs a materialization step first, as the memo says.

### 2. Materialize, then package: it opens without the plugins

`Sdf.Layer.FindOrOpen("model.pmx").Export("model.usdc")` next to the `.pmx`,
followed by `CreateNewUsdzPackage` on the `.usdc`, gives a package of
`model.usdc` and the ten textures at the paths the stage names them by
(`tex/…`, `toon/…`, `spa/…`). Opened without the plugins:

- no composition error; `defaultPrim` is `Asset`; 1,000 prims;
- every asset-valued attribute resolves inside the package: 29 of 29
  for model A, and 38 of 38 for model B (packaged the same way, §3 below);
- `MmdMaterialAPI` is missing from `GetAppliedSchemas()`, since nothing
  registers it, but the `inputs:mmd:material:*` attributes are present;
- `usdchecker` reports success for model A.

Writing the `.usdc` beside the `.pmx` writes into the model's directory. Two
ways around that were tried, in a private directory:

- **The `.usdc` alone, with its asset paths made absolute.** Packaging
  resolves every texture, but stores them in numbered folders (`0/`, `1/`,
  `2/`) with rewritten paths. For model A it also warned, for seven of its
  ten textures, that a rewritten reference did not resolve.
- **The `.usdc` with the textures copied beside it at their stage-relative
  paths.** The package has the same layout as in place. This is the result
  above for model B.

### 3. Non-ASCII names

- Model B's CJK texture names are stored as UTF-8 archive names. They resolve
  inside the package without the plugins (38 of 38). The ZIP UTF-8 flag
  (general-purpose bit 11) is not set, so Python's `zipfile` shows them as
  CP437 mojibake.
- With model A read from its Japanese-named directory, and the package
  written to an ASCII path, packaging succeeds.
- Writing the package to a path under a Japanese-named directory fails:
  `CreateNewUsdzPackage` raises, and the Python binding cannot decode the
  message (`UnicodeDecodeError`, byte `0x95`, a CP932 lead byte).
  `Sdf.ZipFileWriter` writes the same path and saves successfully, and
  `Sdf.Layer.Export` writes a `.usdc` there too. The Python host has no UTF-8
  code page. A tool with the UTF-8 manifest was not tried.
- Of the 275 texture references the 41 PMX files make, 116 have a non-ASCII
  character in their path.

### 4. Texture formats

`ComputeAllDependencies` on each of the 41 PMX files reports exactly one
layer each (the `.pmx`), and no unresolved path. It reports 275 texture
references: 183 PNG and 92 BMP, where the extension agrees with the content
signature in every case. 31 of the 41 PMX files, from 14 of the 15
distributions, name at least one BMP. None names `.spa`, `.sph`, TGA or JPEG.

Model B's package holds its BMP files as they are. `usdchecker` refuses it:
`UnsupportedFileExtensionInPackage`, "unknown unsupported extension 'bmp'".
OpenUSD 26.08's package validator admits `usda`, `usdc`, `usd`, `usdz`,
`png`, `jpg`, `jpeg`, `exr`, `avif`, `m4a`, `mp3` and `wav`. `Hio`'s own
image plugin reads `bmp`, `jpg`, `jpeg`, `png`, `tga` and `hdr`.

### 5. Determinism

Model A was packaged twice, four seconds apart, through the copied-textures
route. The two archives have the same entries in the same order, and every
entry has the same CRC, size, method, flags and extra field. They differ in
each entry's modification time only, so the files' hashes differ. The
materialized `.usdc` is identical in both. `SdfZipFileWriter` takes an
entry's time from the file's modification time, converted to local time.

## Conclusions

- Materialize, then package with the model's own relative layout. It works
  without the plugins and keeps every name
  ([PACKAGING_POLICY.md §4–§6](../design/PACKAGING_POLICY.md#4-materialization)).
- Call discovery and the ZIP writer separately rather than the one-call
  packager, so entry order and times can be controlled
  ([§12](../design/PACKAGING_POLICY.md#12-implementation-boundary),
  [§13](../design/PACKAGING_POLICY.md#13-determinism)).
- BMP is a normal case, not an edge case. A compliant package needs it
  converted. The user chose lossless PNG conversion on 2026-09-25
  ([§7](../design/PACKAGING_POLICY.md#7-texture-formats)).
- Open: archive times (PKG-O1) and non-ASCII names in non-OpenUSD ZIP tools
  (PKG-O2) ([§18](../design/PACKAGING_POLICY.md#18-open-questions)).
