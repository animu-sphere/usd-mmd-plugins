# Phase 2 — canonical stage

Status: 🚧 in progress — implemented and verified on Windows, and both
libraries' sanitizer builds on Linux; waiting for its first CI run.

`mmdModel` turns a `pmx::Document` into the canonical model — the one
source-to-USD basis conversion, stable identifiers, the canonical joint
order, normalized skinning, face ranges and normalized texture paths — and
the importer authors it: `/Asset` as the `UsdSkelRoot`, the mesh with its UVs
and per-vertex data under `geo`, one material prim per PMX material under
`mtl`, bound through `materialBind` subsets, and the skeleton under `skel`.
Materials have no shading network yet — that is Phase 3, the material triad.

## Outcome

```text
Usd.Stage.Open("model.pmx")
    → mmdPmx::Read            source facts, or the fatal diagnostic
    → mmd::Canonicalize       USD basis, meters, identifiers, joint order, weights
    → UsdMmdAuthorer          /Asset (SkelRoot), geo/Mesh, mtl/<material>, skel/Skeleton
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

- 🚧 The first CI run of this Phase, green on every cell: `ost-source-ci.yml`
  (the graph cell now resolves `mmdModel` and its edge to `mmdPmx`; the
  workspace cells run the new unit, robustness and boundary tests, the stage
  checks and OpenUSD's validators over all 33 fixtures, and the extended
  installed-consumer lane; the macOS and Linux bundle cells compare the new
  golden at L5), `docs-check.yml`, and `parser-sanitizers.yml`, whose
  `mmdModel` steps have not run in CI yet. macOS and Linux have not built
  this Phase at all, and `-ffp-contract=off` is what makes their stages
  byte-identical — the one golden both compare against is the test of it.
- ⬜ Once CI is green: this page replaced by the Phase 3 plan, and the
  roadmap's status table updated. Phase 3 needs MAT-O1, -O2 and -O3 answered
  first ([open decisions](README.md#open-decisions)).

## Completion criteria

[DESIGN_POLICY.md §14](../design/DESIGN_POLICY.md#14-phases)'s acceptance for
Phase 2, and where each stands:

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
