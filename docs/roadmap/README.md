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
| [current.md](current.md) | The remaining milestones — Phase 9 shared motion adoption, then the Phase 8 material-schema, renderer, avatar and physics composition work, and Phase 10 USDZ packaging — and the part of each this repository owes or waits for. |
| [../releases/](../releases/README.md) | What each released version shipped, and how a release is cut. |

## One sequence

This repository has one phase sequence, `Phase 0`–`Phase 10`, defined in
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
| 8 | `MmdMaterialAPI`, renderer bridge and avatar runtime composition, including optional physics coupling | ⬜ | unassigned; schema/adapter work is here, composition is owned mostly outside this repository |
| 9 | shared motion core adoption — runs before Phase 8 | 🚧 skeletal evaluator, adapters and end-to-end acceptance done; the A-pose rest (MOT-O10) is measured and, with expression interoperability, waits on `usd-motion-plugins`; role-table version 2 resolves MOT-O12 | unassigned |
| 10 | USDZ packaging (`mmd_export`) | ⬜ designed ([PACKAGING_POLICY.md](../design/PACKAGING_POLICY.md)), measured, not started | unassigned |

Phases 0–7 ship together in v0.1.0, the first release, decided on
2026-09-17: it is the one that meets
[DESIGN_POLICY.md §14.1](../design/DESIGN_POLICY.md#141-first-substantial-release--definition-of-done)
— Phases 0–4 plus the preservation parts of 5 and 6 it names — and it carries
Phase 7 too. It is a 0.x release because no consumer has used the packages
yet: Phase 8's consumer may still show a contract wrong
([current.md](current.md)). No earlier Phase had a release of its own.

Where things stand, as of 2026-09-25:

- The documentation baseline exists: the design policy, seven focused design
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
- Phase 3, the material triad, is done: canonical MMD material semantics as
  schema-less `mmd:material:*` attributes on every contract-v1 material prim,
  plus the unlit `preview` and MaterialX `mtlx`
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
- Phase 9, shared motion core adoption, is the current milestone. `mmdControl`
  evaluates a bound motion over the control rig —
  Bézier curves, bone and group morphs, appends, IK — deterministically, as
  [MOTION_CONTRACT.md §11](../design/MOTION_CONTRACT.md#11-evaluating-the-control-rig)
  says, with MOT-O7 resolved and legs following their IK goals over 13 local
  models and two distributed motions
  ([report](../reports/2026-09-19-phase9-local-control.md)).
  The humanoid role table and root motion are decided — MOT-O5 and MOT-O6
  ([MOTION_CONTRACT.md §12](../design/MOTION_CONTRACT.md#12-the-humanoid-role-table),
  [report](../reports/2026-09-19-phase9-roles-and-root.md)).
  `mmdMotionAdapter` and `mmdSkeletonAdapter` consume the released,
  digest-pinned `motionCore` and `motionRetarget` v0.5.0 packages and are
  covered by unit, boundary and installed-consumer tests. A deterministic
  acceptance test now carries VMD-derived, IK-evaluated legs through
  digest-pinned `motionUsd` and `motionRetarget`, onto both a PMX-derived
  stage skeleton and a non-MMD skeleton
  ([report](../reports/2026-09-22-phase9-motion-acceptance.md)). MOT-O10 was
  measured on 2026-09-25: an arm-chain rest is exact, but only once a PMX
  target can state it too, which waits on `usd-motion-plugins`
  ([report](../reports/2026-09-25-phase9-rest-pose-comparison.md)). So does
  expression interoperability: that repository has promoted no common
  expression semantic. MOT-O12, found by the same run, is resolved by
  role-table version 2
  ([report](../reports/2026-09-25-phase9-upper-chest.md)).
- Phase 8 follows Phase 9. Before renderer composition, this repository adds
  the admitted `MmdMaterialAPI`, applies it as stage-contract v2 with the
  canonical values as Material interface inputs `inputs:mmd:material:*` that
  the fallback graphs connect to
  ([report](../reports/2026-09-25-phase8-material-inputs.md)), and supplies the UsdImaging bridge consumed by `hydra-toon`. The shared renderer
  may normalize MMD and MToon privately, but their USD schemas remain separate.
  The rest of avatar composition is owned mostly outside this repository. Its
  MMD physics work consumes the already-authored stage through
  `usd-physics-plugins`; the importer never gains a renderer or solver
  ([PHYSICS_INTEGRATION.md](../design/PHYSICS_INTEGRATION.md)).
- Phase 10, USDZ packaging, was added on 2026-09-25 from the packaging memo,
  and designed after measuring OpenUSD's own packaging over the local models
  ([PACKAGING_POLICY.md](../design/PACKAGING_POLICY.md),
  [report](../reports/2026-09-25-usdz-packaging-probe.md)). `mmd_export`
  materializes the importer's stage and packages it with its textures. It
  converts BMP, which 31 of the 41 local PMX files name, to PNG, so that the
  package is standard USDZ. It needs only the importer's stage, so it does
  not wait for Phases 8 or 9. The tool, first reserved as `mmd_usdz`, became
  `mmd_export` on 2026-09-26, to pair with `usd-vrm-plugins`' `vrm_export`;
  `.usdz` is its first format.

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
| MOT-O5 | Which MMD bones feed `RootMotion` | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) | resolved in Phase 9 |
| MOT-O6 | The humanoid role table and its version | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) | resolved in Phase 9 |
| MOT-O9 | MMD's own IK distance at a model's loop count | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) | resolved in Phase 9 — §11.7 kept |
| MOT-O10 | The rest a clip from MMD states | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) | measured 2026-09-25; `usd-motion-plugins`: a target rest distinct from the bind rest, and public T-pose directions |
| MOT-O2 | What a directly opened `.vmd` stage looks like | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) | `usdVmdFileFormat` |
| MOT-O3 | Which runtime owns MMD IK and append evaluation for baking | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) | resolved in Phase 7; superseded 2026-09-17 — `mmdControl`, here |
| PMX-O2 | QDEF verification | [PMX §16](../design/PMX_CONTRACT.md#16-open-questions) | a consumer |
| PMX-O4 | Morph category from display frames | [PMX §16](../design/PMX_CONTRACT.md#16-open-questions) | a consumer |
| MAT-O4 | How `hydra-toon` reads MMD semantics | [MATERIAL §13](../design/MATERIAL_POLICY.md#13-open-questions) | resolved 2026-09-22 — `MmdMaterialAPI` + UsdImaging adapter; implementation is Phase 8 |
| MAT-O5 | How a changed canonical value reaches a realization | [MATERIAL §13](../design/MATERIAL_POLICY.md#13-open-questions) | resolved 2026-09-25 — `inputs:mmd:material:*`, connected; stage-contract v2 |
| MAT-O6 | The untextured `/preview`, which cannot split a `color4f` diffuse | [MATERIAL §13](../design/MATERIAL_POLICY.md#13-open-questions) | the Phase 8 importer migration |
| MOT-O4 | Camera and light tracks | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) | a consumer |
| MOT-O8 | Evaluating MMD motion from a stage alone | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) | a consumer that holds only the stage |
| MOT-O11 | Whether a knee starts from its keyed rotation | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) | nothing (MMD's output to compare against) |
| MOT-O12 | Where `上半身3` falls in the role table | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) | resolved 2026-09-25 — table version 2 |
| PKG-O1 | How the USDZ archive becomes byte-deterministic | [PACKAGING §18](../design/PACKAGING_POLICY.md#18-open-questions) | Phase 10 step 2 |
| PKG-O2 | Non-ASCII archive names in ZIP tools that are not OpenUSD | [PACKAGING §18](../design/PACKAGING_POLICY.md#18-open-questions) | Phase 10 step 3 |

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
