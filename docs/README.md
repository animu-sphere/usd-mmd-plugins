# usd-mmd-plugins documentation

Documentation is organized by responsibility: each category answers one class
of question. The layout is the one `usd-vrm-plugins`, `open-strata` and
`hydra-merlin` use, so the repositories read the same way.

**The tree holds Phases 0–4 (2026-09-16):** the PMX structural parser reads
every table of a PMX 2.0 or 2.1 file, `mmd_inspect` reports on it, and `.pmx`
opens as the canonical stage — mesh, UVs, material prims and subsets,
skeleton and skinning, Y-up, in meters. Phase 3 adds canonical MMD material
semantics and unlit-compatible `preview` and `mtlx` realizations, and Phase 4
every morph: vertex morphs as blend shapes, every other type preserved
declaratively. Control and physics semantics are not authored. Everything
else in `design/` is intended behavior; [reference/](reference/) is the only
place that says what is implemented.

| Category | Answers | Start here |
| --- | --- | --- |
| [architecture/](architecture/) | How the workspace is structured: component identities, dependency directions, build modes, external dependencies, installed packages. | [WORKSPACE.md](architecture/WORKSPACE.md) · [DEPENDENCIES.md](architecture/DEPENDENCIES.md) · [PACKAGE_CONTRACT.md](architecture/PACKAGE_CONTRACT.md) |
| [design/](design/) | What the importer authors and why. | [DESIGN_POLICY.md](design/DESIGN_POLICY.md) |
| [guides/](guides/) | How to accomplish a task, with commands that have been run. | [building.md](guides/building.md) · [opening.md](guides/opening.md) · [inspecting.md](guides/inspecting.md) |
| [reference/](reference/) | Facts about the current tree: what is supported, which diagnostics exist, where each PMX field lands. | [CAPABILITY_MATRIX.md](reference/CAPABILITY_MATRIX.md) · [DIAGNOSTICS.md](reference/DIAGNOSTICS.md) · [SOURCE_MAPPING.md](reference/SOURCE_MAPPING.md) |
| [roadmap/](roadmap/) | What is planned next (incomplete work only), and which release carries it. | [README.md](roadmap/README.md) · [current.md](roadmap/current.md) |
| [contributing/](contributing/) | How to maintain these documents. | [documentation.md](contributing/documentation.md) |
| `releases/` | Immutable per-version release records. | added with the first release |
| [reports/](reports/) | Dated evidence from real runs; append-only. | [2026-09-15-phase2-local-models.md](reports/2026-09-15-phase2-local-models.md) · [2026-09-16-phase4-local-models.md](reports/2026-09-16-phase4-local-models.md) |

## Canonical documents

- [design/DESIGN_POLICY.md](design/DESIGN_POLICY.md) is the **design policy**:
  the central rule (parse MMD, normalize it once, author conventional OpenUSD,
  keep evaluation and rendering outside the importer), the component
  responsibilities, the schema admission test, the testing policy, the
  **Phase 0–8** sequence, the decisions frozen early, and where the design
  departs from the 2026-09-15 implementation policy it was distilled from.
- Five focused contracts own one area each, and on that area they win over the
  design policy:
  - [design/STAGE_CONTRACT.md](design/STAGE_CONTRACT.md) — the exact authored
    stage: hierarchy, types, metadata, the coordinate conversion, skeleton,
    skinning, morph layout, and the stage-contract version;
  - [design/PMX_CONTRACT.md](design/PMX_CONTRACT.md) — how PMX 2.0/2.1 bytes are
    read and what each source concept becomes in the canonical model;
  - [design/MATERIAL_POLICY.md](design/MATERIAL_POLICY.md) — `UsdPreviewSurface`,
    MaterialX `gltf_pbr`, and the native MMD material semantics;
  - [design/TEXT_ENCODING_POLICY.md](design/TEXT_ENCODING_POLICY.md) — text
    decoding, source names versus USD identifiers, collisions, texture paths;
  - [design/MOTION_CONTRACT.md](design/MOTION_CONTRACT.md) — the MMD-specific
    motion boundary for VMD.
- [architecture/WORKSPACE.md](architecture/WORKSPACE.md) is the binding
  **workspace contract**. When a document disagrees with it about structure, it
  wins, and structural changes go there first, in their own pull request.

## Source-of-truth rules

- Code is authoritative for implemented behavior; `architecture/` and
  `reference/` record it and change with it.
- `design/` defines intended contracts and must label what is not yet
  implemented.
- `roadmap/` holds incomplete work only; the release a Phase lands in is stated
  only in its [status table](roadmap/README.md#status-at-a-glance).
- The details are in [contributing/documentation.md](contributing/documentation.md).
