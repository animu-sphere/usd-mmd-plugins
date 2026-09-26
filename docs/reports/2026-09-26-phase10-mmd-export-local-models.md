# Phase 10 step 1: `mmd_export` over the local models (2026-09-26)

Dated evidence from real runs; append-only
([contributing/documentation.md](../contributing/documentation.md)).

## Question

[PACKAGING_POLICY.md](../design/PACKAGING_POLICY.md) §14 asks for the tool's
run over the locally held models. The fixtures prove each rule on its own.
Does every real model package? Does each package open with no MMD plugin and
pass `usdchecker`? Is it the `.pmx`'s stage, value for value? The
[probe report](2026-09-25-usdz-packaging-probe.md) measured OpenUSD's own
packaging, not this tool.

## What was run

- **Build.** `mmd_export`, `usdMmdFileFormat` and `mmdSchema` from the
  Phase 10 step 1 branch, against OpenUSD 26.08, on Windows 11.
- **Models.** The 15 locally held distributions, 41 PMX files, the set the
  probe used. Many sit in directories, or name textures, in Chinese or
  Japanese. They are distributed by their creators, so they are neither
  committed nor named.
- **Per model**, from a scratch script that is not committed:
  1. `mmd_export <model>.pmx <n>.usdz`, with both bundles on the plugin path.
  2. The tool's test probe
     ([stage_probe.py](../../tools/mmdExport/tests/stage_probe.py)) opens the
     package in a process with **no** MMD plugin and checks §8 and §9's
     post-write list: neither the importer nor `mmdSchema` loaded, every
     layer and every asset value inside the package, and `MmdMaterialAPI`
     still authored though not applied.
  3. The same probe dumps the `.pmx` stage, with the plugins, and the package
     stage, without them: every prim's type, specifier, `apiSchemas`,
     metadata, and every property's type, default, time samples, metadata,
     connections and targets. The `.pmx` dump's texture paths are renamed as
     the archive's entries show, and the two dumps are compared whole.
  4. OpenUSD's `usdchecker` on the package, with no MMD plugin. The packages
     were written under ASCII names in `C:/dev/tmp`: on Windows this
     `usdchecker` cannot open a non-ASCII path.

## Results

| | |
| --- | --- |
| Models packaged (exit 0) | 41 of 41 |
| Opened with no MMD plugin, every layer and asset inside the package | 41 of 41 |
| Package stage identical to the `.pmx` stage, texture renames aside | 41 of 41 (28,188 prims) |
| `usdchecker`, no MMD plugin | 41 of 41 pass |
| Textures packaged | 275 entries |
| Textures converted to PNG (`MMD_PKG_TEXTURE_CONVERTED`) | 92, in 31 of the 41 models |
| Time per model (packaging only) | median 0.27 s, at most 2.0 s |

- **Conversions match the probe.** The probe counted 275 texture references
  in these 41 files, 92 of them BMP. The tool packaged 275 entries and
  converted 92, so every reference was one file, and every BMP, and only
  BMP, was converted. None of these models names a TGA, a `.spa`/`.sph` or a
  JPEG, so here only the fixtures exercise those rows of §7.
- **No run stopped.** No model named a missing texture, an unsupported
  format, or a converted name it already had, so no `MMD_PKG_` error was
  raised. The fixtures cover each of those.
- **Identical stages.** Comparing the whole dump includes each material's
  `inputs:mmd:material:*` values and connections, the provenance that keeps
  the source texture path, `/Asset`'s recorded diagnostics and
  `mmd:stageContractVersion`, the skeleton, the skinning and the blend
  shapes. Only the renamed texture paths differ, and only as §7 says.
- **Viewed.** One package, a model with 13 converted textures, was also
  opened in `usdview` from a process with no MMD plugin, and checked by eye.

## Found along the way

- **`UsdUtilsModifyAssetPaths` drops empty array elements.** Asked to keep
  them (`keepEmptyPathsInArrays`), OpenUSD 26.08 still removed the `@@`
  element from `[@./a.bmp@, @./b.png@, @@]`. That breaks §4's "nothing
  dropped", so the tool rewrites the renamed `SdfAssetPath` values itself
  ([PACKAGING_POLICY.md §12](../design/PACKAGING_POLICY.md#12-implementation-boundary)).
  The unit test that found it keeps it fixed.
- **A USDZ stage's root layer is the package.** Its identifier is the
  archive's own path, not `<archive>[<root>.usdc]`, while every resolved asset
  inside it is `<archive>[<entry>]`, spelled as the resolver spells the
  archive's real path (backslashes on Windows). The package checks compare
  paths by file identity for that reason.
- **`usdchecker` and non-ASCII paths.** On Windows, OpenUSD 26.08's
  `usdchecker.exe` fails to open a package whose path leaves the ANSI code
  page ("Failed to open layer"). `mmd_export` embeds a UTF-8 code page and
  runs the same validators in process, so its own validation holds on such
  a path. The fixture test skips the external `usdchecker` only there, on
  Windows.
- **Plug and relative plugin paths.** A relative directory on
  `PXR_PLUGINPATH_NAME` registers nothing, so the tool then reports
  `MMD_PKG_INPUT_UNREADABLE` for a `.pmx`. The guide names both bundles by
  absolute path ([exporting.md](../guides/exporting.md)).

## Conclusions

- Phase 10 step 1's acceptance holds for the fixtures and the local models:
  every package opens with no MMD plugin, passes `usdchecker`, and matches
  the `.pmx` stage prim for prim and value for value, texture renames aside
  ([DESIGN_POLICY.md §14](../design/DESIGN_POLICY.md#14-phases)).
- Next is step 2: name collisions and path normalization against the
  importer's, missing-asset reporting, and a byte-deterministic archive
  (PKG-O1).
