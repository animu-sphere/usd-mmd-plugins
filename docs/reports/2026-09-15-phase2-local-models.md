# Phase 2 against distributed models (2026-09-15)

Dated evidence from real runs; append-only
([contributing/documentation.md](../contributing/documentation.md)).

## What was run

The Phase 2 importer (branch `phase2/canonical-stage`, Windows 11, MSVC,
OpenUSD 26.08) opened four PMX files held locally: three distinct
distributed MMD models — two characters and one stage set — and a second
file of character B under a Japanese filename, 65 bytes apart from the first
and importing to the same counts. None is committed or
redistributed, and no image of one is kept
([DESIGN_POLICY.md §13](../design/DESIGN_POLICY.md#13-testing-policy)).

Each was opened with `Usd.Stage.Open` from Python, checked with UsdSkel's
and UsdGeom's own queries, validated with `usdchecker` (every validator
OpenUSD registers), and one character was rendered with `usdrecord` (Storm).

## What it showed

| | Character A | Character B | Stage set |
| --- | --- | --- | --- |
| Points, faces | 47,181, 68,512 | 26,487, 41,368 | 315,922, 224,449 |
| Height of the bounds | 1.712 m | 1.575 m | 91.1 m |
| Joints | 271 | 283 | 3 |
| Joints named by the `bone_NNNN` fallback | 99 | 256 | 0 |
| Influences per vertex | 4 | 2 | 1 |
| Materials, subsets | 49, 49 (a valid partition) | 30, 30 (a valid partition) | 39, 39 (a valid partition) |
| Base textures that resolve | 39 of 39 | 30 of 30 | 38 of 38 |
| Diagnostics | 27 identifier collisions, SDEF approximated | 17 identifier collisions, SDEF approximated | 1 identifier collision |
| Opened in | 0.47 s | 0.21 s | 1.18 s |
| `usdchecker` | Success | Success | Success |

- **Scale.** At 0.08 m per MMD unit both characters are 1.6–1.7 m tall, which
  is what STAGE-O1 assumed
  ([STAGE_CONTRACT.md §6.2](../design/STAGE_CONTRACT.md#62-unit-scale)).
- **Orientation and skinning.** The `usdrecord` render of character B shows
  the model upright, facing the camera on +Z, smoothly shaded — normals
  pointing outward after the Z mirror and the winding reversal — and intact
  in the skinned rest pose, in the renderer's fallback grey (no shading
  network before Phase 3).
- **Skeleton.** Every joint topology is valid, and UsdSkel resolves the mesh
  as the skeleton's one skinning target. No model needed its joints
  reordered or its parents repaired, and none had a weight to normalize.
- **Identifiers.** Character B's English bone names are mostly empty, so 256
  of its 283 joints fall back to `bone_NNNN`; its display names survive in
  `mmd:bone:sourceName`. Collisions are common (17–27 per character), from
  English names repeated across bones (`waist` twice, for example); each is
  recorded as an info diagnostic.
- **Japanese filenames.** Every base texture resolved, Japanese filenames
  included. The file with a Japanese filename opened from Python; OpenUSD's
  own `usdcat` and `usdchecker` could not open it on this host, whose ANSI
  code page is 932: they received the path in CP932 and read it as UTF-8, so
  the layer was not found before the plugin was reached — the host problem
  [TEXT_ENCODING_POLICY.md §4](../design/TEXT_ENCODING_POLICY.md#4-no-locale-anywhere)
  describes, recorded in [guides/opening.md](../guides/opening.md).
