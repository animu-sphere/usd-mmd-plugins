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
| [current.md](current.md) | The remaining milestones — Phase 9, shared motion core adoption (in progress), then Phase 8, avatar runtime composition — and the part of each this repository owes or waits for. |
| [../releases/](../releases/README.md) | What each released version shipped, and how a release is cut. |

## One sequence

This repository has one phase sequence, `Phase 0`–`Phase 9`, defined in
[DESIGN_POLICY.md §14](../design/DESIGN_POLICY.md#14-phases). A phase is a
unit of scope; a release is a scheduling decision. The mapping between them is
made **only** in the table below.

## Status at a glance

**This table is the single source of truth for which release a Phase lands
in.** No other document states a version for a Phase.

| Phase | Scope | Status | Release |
| --- | --- | --- | --- |
| 0 | workspace skeleton | ✅ done | [v0.1.0](../releases/v0.1.0.md) |
| 1 | PMX structural parser | ✅ done | [v0.1.0](../releases/v0.1.0.md) |
| 2 | canonical stage | ✅ done | [v0.1.0](../releases/v0.1.0.md) |
| 3 | material triad | ✅ done | [v0.1.0](../releases/v0.1.0.md) |
| 4 | morphs | ✅ done | [v0.1.0](../releases/v0.1.0.md) |
| 5 | control semantics | ✅ done | [v0.1.0](../releases/v0.1.0.md) |
| 6 | physics preservation | ✅ done | [v0.1.0](../releases/v0.1.0.md) |
| 7 | VMD | ✅ done | [v0.1.0](../releases/v0.1.0.md) |
| 8 | avatar runtime composition | ⬜ | unassigned, and owned mostly outside this repository |
| 9 | shared motion core adoption — runs before Phase 8 | 🚧 `mmdControl` done | unassigned; its adapter waits for `usd-motion-plugins`' first release |

Phases 0–7 ship together in v0.1.0, the first release, decided on
2026-09-17: it is the one that meets
[DESIGN_POLICY.md §14.1](../design/DESIGN_POLICY.md#141-first-substantial-release--definition-of-done)
— Phases 0–4 plus the preservation parts of 5 and 6 it names — and it carries
Phase 7 too. It is a 0.x release because no consumer has used the packages
yet: Phase 8's consumer may still show a contract wrong
([current.md](current.md)). No earlier Phase had a release of its own.

Where things stand, as of 2026-09-19:

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
- Phase 6, physics preservation, is done: every rigid body and joint is under
  `/Asset/physics`, as `UsdPhysics` where it matches and `mmd:physics:*`
  throughout, with PMX-O1 and the physics half of STAGE-O6 resolved and a
  consumer able to recover every body and joint from the stage alone —
  asserted on every fixture and on sixteen distributed models
  ([report](../reports/2026-09-17-phase6-local-models.md)).
- Phase 7, VMD, is done: `motionVmd` reads every section of a VMD without a
  model, through a CP932 table the project generates, `vmd_inspect` reports
  on it, and `mmdMotionBinding` binds a motion to a canonical model by MMD's
  byte rule, in the model's basis — with MOT-O1 and MOT-O3 resolved, nothing
  baked, and the reader and binding checked against three distributed motions
  and 25 models ([report](../reports/2026-09-17-phase7-local-motions.md)).
  `usdVmdFileFormat` was never part of it: it waits for MOT-O2.
- The motion architecture was settled on 2026-09-17 by the
  `usd-motion-plugins` design policy, and the design documents were aligned
  with it
  ([DESIGN_POLICY.md §20](../design/DESIGN_POLICY.md#20-alignment-with-the-usd-motion-plugins-design-policy)):
  VMD stays here, MMD IK and append evaluation moves here from the runtime
  (MOT-O3 superseded), and Phase 9 was added for the hand-off to the shared
  motion core.
- Phase 9, shared motion core adoption, is the current milestone. Its first
  part is done: `mmdControl` evaluates a bound motion over the control rig —
  Bézier curves, bone and group morphs, appends, IK — deterministically, as
  [MOTION_CONTRACT.md §11](../design/MOTION_CONTRACT.md#11-evaluating-the-control-rig)
  says, with MOT-O7 resolved and legs following their IK goals over 13 local
  models and two distributed motions
  ([report](../reports/2026-09-19-phase9-local-control.md)).
  `mmdMotionAdapter` waits for `usd-motion-plugins`, whose first tag
  (2026-09-19) is a pre-release without `SkeletonDescriptor` or `RetargetMap`.
- Phase 8, avatar runtime composition, follows Phase 9 and is owned mostly
  outside this repository.

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
| STAGE-O6 | Rig and physics prim shapes | [STAGE §16](../design/STAGE_CONTRACT.md#16-open-questions) | resolved in Phases 5 and 6 |
| PMX-O1 | Euler order of rigid-body and joint rotations | [PMX §16](../design/PMX_CONTRACT.md#16-open-questions) | resolved in Phase 6 |
| MOT-O1 | Home of the shared basis-conversion functions | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) | resolved in Phase 7 |
| MOT-O7 | Morphs evaluated into the pose versus carried as channels | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) | resolved in Phase 9 |
| MOT-O5 | Which MMD bones feed `RootMotion` | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) | `mmdMotionAdapter` (Phase 9) |
| MOT-O6 | The humanoid role table and its version | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) | `mmdMotionAdapter` (Phase 9) |
| MOT-O2 | What a directly opened `.vmd` stage looks like | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) | `usdVmdFileFormat` |
| MOT-O3 | Which runtime owns MMD IK and append evaluation for baking | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) | resolved in Phase 7; superseded 2026-09-17 — `mmdControl`, here |
| PMX-O2 | QDEF verification | [PMX §16](../design/PMX_CONTRACT.md#16-open-questions) | a consumer |
| PMX-O4 | Morph category from display frames | [PMX §16](../design/PMX_CONTRACT.md#16-open-questions) | a consumer |
| MAT-O4 | How `hydra-toon` reads MMD semantics | [MATERIAL §13](../design/MATERIAL_POLICY.md#13-open-questions) | `hydra-toon` |
| MOT-O4 | Camera and light tracks | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) | a consumer |
| MOT-O8 | Evaluating MMD motion from a stage alone | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) | a consumer that holds only the stage |
| MOT-O9 | MMD's own IK distance at a model's loop count | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) | nothing (a reference to compare against) |

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
