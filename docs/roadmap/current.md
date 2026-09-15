# Phase 3 — material triad

Status: 🚧 in progress — the canonical semantics and portable realization
graphs are implemented; the current PR is finishing its cross-platform
golden/CI pass and real-model visual review.

`mmdModel` turns a `pmx::Document` into the canonical model — the one
source-to-USD basis conversion, stable identifiers, the canonical joint
order, normalized skinning, face ranges and normalized texture paths — and
the importer authors it: `/Asset` as the `UsdSkelRoot`, the mesh with its UVs
and per-vertex data under `geo`, one material prim per PMX material under
`mtl`, bound through `materialBind` subsets, and the skeleton under `skel`.
Phase 3 authors the full canonical MMD material semantics and the
unlit-compatible `preview` and `mtlx` realization graphs; sphere, toon, edge
and other MMD-specific values remain declarative on the material prim.

## Outcome

```text
Usd.Stage.Open("model.pmx")
    → mmdPmx::Read            source facts, or the fatal diagnostic
    → mmd::Canonicalize       USD basis, meters, identifiers, joint order, weights
    → UsdMmdAuthorer          /Asset, geo/Mesh, mtl/<material> + preview/mtlx, skel/Skeleton
    → every recoverable diagnostic of all three on /Asset
```

The decisions this Phase needed were taken on 2026-09-15 and are recorded
where they belong: STAGE-O1, -O2, -O3 and -O5 in
[STAGE_CONTRACT.md §16](../design/STAGE_CONTRACT.md#16-open-questions), with
the rules the implementation made precise in §6.2, §8.5, §9.1, §9.2 and §9.5;
TEXT-O1 and -O2 in
[TEXT_ENCODING_POLICY.md §10](../design/TEXT_ENCODING_POLICY.md#10-open-questions).
What exists is recorded in [WORKSPACE.md](../architecture/WORKSPACE.md),
[PACKAGE_CONTRACT.md](../architecture/PACKAGE_CONTRACT.md),
[CAPABILITY_MATRIX.md](../reference/CAPABILITY_MATRIX.md),
[DIAGNOSTICS.md](../reference/DIAGNOSTICS.md) and
[SOURCE_MAPPING.md](../reference/SOURCE_MAPPING.md); the commands in
[guides/building.md](../guides/building.md) and
[guides/opening.md](../guides/opening.md); the shipped scope in the
[changelog](../../CHANGELOG.md).

## What remains

- 🚧 Finish the cross-platform standalone golden/CI pass and keep the golden
  free of machine-local paths.
- 🚧 Review usdview output on several real PMX models; the portable paths are
  intentionally unlit-compatible and do not implement MMD toon shading.
- ⬜ Add a future MMD-aware realization/consumer for sphere maps, toon ramps
  and outlines; that work is outside the current generic material triad.

## Completion criteria

[DESIGN_POLICY.md §14](../design/DESIGN_POLICY.md#14-phases)'s acceptance for
Phase 3, and where each stands:

- **Canonical MMD semantics.** Diffuse, specular, ambient, draw flags, edge,
  sphere mode, toon source, shared toon index and provenance are preserved in
  `mmdModel` and authored on each material prim. *Implemented and unit-tested.*
- **Portable preview realization.** Each material has a `preview` graph using
  `UsdPreviewSurface` with its lit response disabled and source color carried
  through emission. *Implemented and stage-tested.*
- **Portable MaterialX realization.** Each material has an `mtlx` graph using
  `ND_gltf_pbr_surfaceshader`, emission-based color, alpha policy and a 1.39
  config marker. *Implemented and stage-tested.*
- **Cross-platform golden and CI.** Standalone bundle verification and the
  committed L5 golden agree on every supported platform. *In progress.*
- **Real-model visual review.** Real PMX models open with useful portable
  colors; MMD-specific toon/sphere/edge rendering remains outside this phase.
  *In progress.*

- **Recognizable character geometry in usdview.** Distributed character
  models held locally import upright, 1.6–1.7 m tall, facing +Z, and render
  intact in Storm — `usdview`'s renderer — through `usdrecord`, in the skinned
  rest pose ([report](../reports/2026-09-15-phase2-local-models.md)). *Met, on
  Windows.*
- **Skeleton and skin binding inspectable.** Every fixture's joint paths,
  bind translations, provenance and influences are asserted against what
  `fixtures.json` computes independently; UsdSkel validates the topology and
  resolves the mesh as the skeleton's skinning target; OpenUSD's validators
  pass on every fixture and on each of the three local models. *Met, on
  Windows.*
- **Japanese names preserved.** Model, material and bone names survive
  byte-exact in their provenance, beside ASCII identifiers; Japanese texture
  filenames resolve, from an ASCII and a Japanese directory. *Met, on
  Windows.*
- Both build modes and the installed prefix: plain `ctest` and
  `ost build`/`ost test` pass, the bundle builds standalone against its
  library closure and passes L0–L5 from the build tree and from the package,
  `mmdModel` builds, tests and verifies its consumer on its own. *Windows:
  met. macOS, Linux: CI.*
