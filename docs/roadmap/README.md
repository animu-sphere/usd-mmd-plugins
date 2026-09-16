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
| [current.md](current.md) | The current milestone — Phase 6, physics preservation — and what remains of it. |

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
| 3 | material triad | ✅ done | unassigned |
| 4 | morphs | ✅ done | unassigned |
| 5 | control semantics | ✅ done | unassigned |
| 6 | physics preservation | ⬜ | unassigned |
| 7 | VMD | ⬜ | unassigned |
| 8 | avatar runtime composition | ⬜ | unassigned, and owned mostly outside this repository |

No release number is assigned yet. The first release that claims PMX import is
the one that meets
[DESIGN_POLICY.md §14.1](../design/DESIGN_POLICY.md#141-first-substantial-release--definition-of-done)
— Phases 0–4 plus the preservation parts of 5 and 6 it names. Phases 0–4 are
done, and so is Phase 5's preservation, so what that definition still wants
is Phase 6's. Whether earlier Phases get releases of their own is decided
here, and the question is still open.

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
- Phase 2, the canonical stage, is done: `mmdModel` canonicalizes, the
  importer authors the mesh, material prims and subsets, skeleton and
  skinning, and the workspace, standalone, sanitizer and documentation lanes
  cover the implementation.
- Phase 3, the material triad, is done: canonical MMD material semantics on
  every material prim, plus the unlit `preview` and MaterialX `mtlx`
  realizations. Its last two items closed on 2026-09-16 — the standalone
  golden agrees on every supported platform, and three distributed models
  render through Storm with their source colors
  ([report](../reports/2026-09-16-phase4-local-models.md)).
- Phase 4, morphs, is done: every PMX morph is a prim under `/Asset/morph`,
  vertex morphs as `UsdSkelBlendShape` the mesh names and every other type
  preserved declaratively, with STAGE-O4 resolved and the group-cycle,
  unknown-panel and no-skeleton diagnostics behind fixtures.
- All five places where the design departs from the 2026-09-15 implementation
  policy
  ([DESIGN_POLICY.md §19](../design/DESIGN_POLICY.md#19-where-this-document-departs-from-the-implementation-policy))
  are now authored with fixtures and binding.
- Phase 5, control semantics, is done: every bone's control semantics are
  under `/Asset/rig`, per-joint arrays on `Bones` and one prim per IK chain,
  with the rig half of STAGE-O6 decided and a consumer able to reconstruct
  every IK chain and append relation from the stage alone — asserted on every
  fixture and on ten distributed models
  ([report](../reports/2026-09-16-phase5-local-models.md)).
- Phase 6, physics preservation, is the current milestone, and nothing of it
  is implemented: `/Asset/physics` is still reserved and empty.

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
| STAGE-O4 | Encoding of non-vertex morph semantics | [STAGE §16](../design/STAGE_CONTRACT.md#16-open-questions) | resolved in Phase 4 |
| STAGE-O6 | Physics prim shapes (the rig half resolved in Phase 5) | [STAGE §16](../design/STAGE_CONTRACT.md#16-open-questions) | Phase 6 |
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
