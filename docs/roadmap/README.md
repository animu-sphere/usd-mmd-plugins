# Roadmap

The roadmap holds only **incomplete** work. When a Phase lands, its task detail
leaves this directory: shipped scope goes to the [changelog](../../CHANGELOG.md)
and, once versions are tagged, to per-version release records; the implemented
state goes to [architecture/](../architecture/) and
[reference/](../reference/). The roadmap is not a second changelog. Design
rationale lives in [design/](../design/).

Legend: ✅ done · 🚧 in progress · ⬜ not started · ⛔ blocked

| Document | Contents |
| --- | --- |
| [current.md](current.md) | The current milestone — Phase 3, the material triad — and what remains of it. |

## One sequence

This repository has one phase sequence, `Phase 0`–`Phase 8`, defined in
[DESIGN_POLICY.md §14](../design/DESIGN_POLICY.md#14-phases). A phase is a
unit of scope; a release is a scheduling decision. The mapping between them is
made **only** in the table below.

## Status at a glance

**This table is the single source of truth for which release a Phase lands
in.** No other document states a version for a Phase.

| Phase | Scope | Status | Release |
| --- | --- | --- | --- |
| 0 | workspace skeleton | ✅ done | unassigned |
| 1 | PMX structural parser | ✅ done | unassigned |
| 2 | canonical stage | ✅ done | unassigned |
| 3 | material triad | 🚧 in progress | unassigned |
| 4 | morphs | ⬜ | unassigned |
| 5 | control semantics | ⬜ | unassigned |
| 6 | physics preservation | ⬜ | unassigned |
| 7 | VMD | ⬜ | unassigned |
| 8 | avatar runtime composition | ⬜ | unassigned, and owned mostly outside this repository |

No release number is assigned yet. The first release that claims PMX import is
the one that meets
[DESIGN_POLICY.md §14.1](../design/DESIGN_POLICY.md#141-first-substantial-release--definition-of-done)
— Phases 0–4 plus the preservation parts of 5 and 6 it names. Whether earlier
Phases get releases of their own is decided here; Phases 0 and 1 are done and
the question is still open.

Where things stand, as of 2026-09-16:

- The documentation baseline exists: the design policy, five focused design
  contracts, the workspace contract, and reference pages that state what is
  implemented.
- Phase 0, the workspace skeleton, is done: `.pmx` opens as a stage through
  `ost` and plain CMake and from an installed prefix, green in CI on Windows,
  macOS and Linux.
- Phase 1, the PMX structural parser, is done: every table of PMX 2.0 and 2.1
  is parsed, `mmd_inspect` reports on it, and CI is green on every cell,
  including the parser's sanitizer and fuzzing lane.
- Phase 2, the canonical stage, is complete: `mmdModel` canonicalizes, the
  importer authors the mesh, material prims and subsets, skeleton and
  skinning, and the workspace, standalone, sanitizer and documentation lanes
  cover the implementation.
- Of the five places where the design departs from the 2026-09-15
  implementation policy
  ([DESIGN_POLICY.md §19](../design/DESIGN_POLICY.md#19-where-this-document-departs-from-the-implementation-policy)),
  four are authored with fixtures and binding; the material graphs of the
  fifth are now implemented as the Phase 3 material triad: canonical MMD
  semantics, a VRM-like unlit `UsdPreviewSurface` fallback, and an unlit
  MaterialX `gltf_pbr` path. MMD-specific sphere, toon, edge and source
  values remain on the material prim for an MMD-aware consumer.
- Phase 3 is the current milestone. Its portable realizations and canonical
  semantics are implemented; remaining work is the final CI/golden pass,
  visual quality review across real models, and a future MMD-aware renderer
  for sphere, toon and edge realization.

## Open decisions

Every open question the design documents carry, in the order they block work.
The owning document holds the question and the proposed answer; this list only
schedules them.

| Id | Question | Owner | Blocks |
| --- | --- | --- | --- |
| PMX-O3 | Globals count above 8 in the wild | [PMX §16](../design/PMX_CONTRACT.md#16-open-questions) | nothing (non-blocking) |
| MAT-O1 | Roughness from specular power | [MATERIAL §13](../design/MATERIAL_POLICY.md#13-open-questions) | resolved in Phase 3 |
| MAT-O2 | Alpha mode without decoding images | [MATERIAL §13](../design/MATERIAL_POLICY.md#13-open-questions) | resolved in Phase 3 |
| MAT-O3 | Missing individual toon texture | [MATERIAL §13](../design/MATERIAL_POLICY.md#13-open-questions) | resolved in Phase 3 |
| STAGE-O4 | Encoding of non-vertex morph semantics | [STAGE §16](../design/STAGE_CONTRACT.md#16-open-questions) | Phase 4 |
| STAGE-O6 | Rig and physics prim shapes | [STAGE §16](../design/STAGE_CONTRACT.md#16-open-questions) | Phases 5, 6 |
| PMX-O1 | Euler order of rigid-body and joint rotations | [PMX §16](../design/PMX_CONTRACT.md#16-open-questions) | Phase 6 |
| MOT-O1 | Home of the shared basis-conversion functions | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) | Phase 7 |
| MOT-O2 | What a directly opened `.vmd` stage looks like | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) | Phase 7 |
| MOT-O3 | Which runtime owns MMD IK and append evaluation for baking | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) | Phase 7 |
| PMX-O2 | QDEF verification | [PMX §16](../design/PMX_CONTRACT.md#16-open-questions) | a consumer |
| PMX-O4 | Morph category from display frames | [PMX §16](../design/PMX_CONTRACT.md#16-open-questions) | a consumer |
| MAT-O4 | How `hydra-toon` reads MMD semantics | [MATERIAL §13](../design/MATERIAL_POLICY.md#13-open-questions) | `hydra-toon` |
| MOT-O4 | Camera and light tracks | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) | a consumer |

## Quality bar (applies to every Phase)

- The importer authors data and never evaluates or simulates it.
- The authored stage does not change meaning without a stage-contract bump.
- The dependency directions in
  [WORKSPACE.md §2](../architecture/WORKSPACE.md#2-dependency-directions) are
  enforced by CI, not by convention.
- Both build modes work; every bundle builds against installed siblings.
- Every capability claim has a fixture behind it.
- Every documented command is one that has actually been run, and no document
  contradicts what CI does.
