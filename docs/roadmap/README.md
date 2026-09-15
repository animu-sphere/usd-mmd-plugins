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
| [current.md](current.md) | The current milestone — Phase 1, the PMX structural parser — and what remains of it. |

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
| 1 | PMX structural parser | 🚧 awaiting CI | unassigned |
| 2 | canonical stage | ⬜ | unassigned |
| 3 | material triad | ⬜ | unassigned |
| 4 | morphs | ⬜ | unassigned |
| 5 | control semantics | ⬜ | unassigned |
| 6 | physics preservation | ⬜ | unassigned |
| 7 | VMD | ⬜ | unassigned |
| 8 | avatar runtime composition | ⬜ | unassigned, and owned mostly outside this repository |

No release number is assigned yet. The first release that claims PMX import is
the one that meets
[DESIGN_POLICY.md §14.1](../design/DESIGN_POLICY.md#141-first-substantial-release--definition-of-done)
— Phases 0–4 plus the preservation parts of 5 and 6 it names. Whether earlier
Phases get releases of their own is decided here; Phase 0 is done and the
question is still open.

Where things stand, as of 2026-09-15:

- The documentation baseline exists: the design policy, five focused design
  contracts, the workspace contract, and reference pages that state what is
  implemented.
- Phase 0, the workspace skeleton, is done: `.pmx` opens as a stage with the
  Phase 0 metadata, through `ost` and plain CMake and from an installed
  prefix, green in CI on Windows, macOS and Linux.
- Phase 1, the PMX structural parser, is implemented and verified on Windows,
  and its sanitizer build on Linux (GCC): every table of PMX 2.0 and 2.1 is
  parsed, and `mmd_inspect` reports on it. Its first CI run, including the new
  `parser-sanitizers.yml`, is what remains ([current.md](current.md)).
- Five places where the design departs from the 2026-09-15 implementation
  policy are listed in
  [DESIGN_POLICY.md §19](../design/DESIGN_POLICY.md#19-where-this-document-departs-from-the-implementation-policy).
  Each stays `proposed` until the Phase that first authors it lands with a
  fixture.

## Open decisions

Every open question the design documents carry, in the order they block work.
The owning document holds the question and the proposed answer; this list only
schedules them.

| Id | Question | Owner | Blocks |
| --- | --- | --- | --- |
| PMX-O3 | Globals count above 8 in the wild | [PMX §16](../design/PMX_CONTRACT.md#16-open-questions) | Phase 1 (non-blocking) |
| STAGE-O1 | Unit scale (`0.08` m per MMD unit) | [STAGE §16](../design/STAGE_CONTRACT.md#16-open-questions) | Phase 2 |
| STAGE-O2 | Mesh prim name, one-mesh rule | [STAGE §16](../design/STAGE_CONTRACT.md#16-open-questions) | Phase 2 |
| STAGE-O3 | Double-sidedness rule | [STAGE §16](../design/STAGE_CONTRACT.md#16-open-questions) | Phase 2 |
| STAGE-O5 | Weight normalization tolerance, zero weights | [STAGE §16](../design/STAGE_CONTRACT.md#16-open-questions) | Phase 2 |
| TEXT-O1 | Texture paths escaping the model directory | [TEXT §10](../design/TEXT_ENCODING_POLICY.md#10-open-questions) | Phase 2 |
| TEXT-O2 | Identifier length cap | [TEXT §10](../design/TEXT_ENCODING_POLICY.md#10-open-questions) | Phase 2 |
| MAT-O1 | Roughness from specular power | [MATERIAL §13](../design/MATERIAL_POLICY.md#13-open-questions) | Phase 3 |
| MAT-O2 | Alpha mode without decoding images | [MATERIAL §13](../design/MATERIAL_POLICY.md#13-open-questions) | Phase 3 |
| MAT-O3 | Missing individual toon texture | [MATERIAL §13](../design/MATERIAL_POLICY.md#13-open-questions) | Phase 3 |
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
