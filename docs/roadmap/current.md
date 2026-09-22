# Phase 9, then Phase 8 — shared motion, material and avatar composition

Status: 🚧 Phase 9 in progress — `mmdControl`, both shared-motion adapters,
skeletal end-to-end acceptance, MOT-O5, MOT-O6 and MOT-O9 are done; Phase 8
implementation has not started, but its `MmdMaterialAPI` and Hydra boundary are
now decided.

Two Phases remain, and they run in this order although they are numbered the
other way ([DESIGN_POLICY.md §14](../design/DESIGN_POLICY.md#14-phases)):

- **Phase 9 — shared motion core adoption.** This repository evaluates MMD
  motion over the control rig (`mmdControl`) and hands the result to
  `usd-motion-plugins` as a `MotionClip` (`mmdMotionAdapter`), while
  `mmdSkeletonAdapter` exposes the PMX skeleton for generic retargeting
  ([MOTION_CONTRACT.md §10](../design/MOTION_CONTRACT.md#10-normalizing-into-the-shared-motion-core)).
- **Phase 8 — material and avatar runtime composition.** This repository adds
  `MmdMaterialAPI` and its UsdImaging bridge; `usd-avatar-runtime` composes this
  repository with `usd-vrm-plugins`, `usd-motion-plugins`,
  `usd-physics-plugins`, `motion-connectors`, `hydra-toon` and
  `usd-stage-runner` through
  OpenStrata. Most of it is owned outside this repository: the runtime, not
  this repository, is the avatar execution environment.

What is listed here is only the part this repository owes, or waits for; what
the runtime consumes from here today is [v0.1.0](../releases/v0.1.0.md).

As of 2026-09-21 `usd-motion-plugins` v0.5.0 is published with installable
`motionCore`, `motionRetarget` and `motionUsd`
([DEPENDENCIES.md §6](../architecture/DEPENDENCIES.md#6-usd-motion-plugins)).
All three are consumed by digest-pinned artifacts: `motionCore` and
`motionRetarget` by the adapters, and `motionUsd` by the skeletal acceptance
test. `mmdMotionAdapter` and `mmdSkeletonAdapter` are implemented.

## Outcome

```text
.pmx ─→ usdMmdFileFormat ─→ /Asset stage ─→ MmdMaterialAPI ─→ UsdImaging adapter ─→ hydra-toon
.vmd ─→ motionVmd ─→ mmdMotionBinding ─→ mmdControl ─→ mmdMotionAdapter ─→ MotionClip
.pmx ─→ mmdModel ─→ mmdSkeletonAdapter ─→ SkeletonDescriptor / RetargetMap
                                                                              │
usd-motion-plugins:  retarget to any skeleton, record, author UsdSkelAnimation ◀┘
usd-stage-runner:    orders pose → MMD physics sync → physics step → feedback
usd-avatar-runtime:  composes the above per frame and coordinates rendering
```

## Phase 9 — what remains

- ✅ **`mmdControl`** (2026-09-19): Bézier sampling of a bound motion, bone
  and group morphs, evaluation in MMD's order, appends, IK with the IK-enable
  track, as
  [MOTION_CONTRACT.md §11](../design/MOTION_CONTRACT.md#11-evaluating-the-control-rig)
  says, with its unit, robustness, boundary, sanitizer and installed-consumer
  tests over synthetic rigs with known answers, and checked against 13 local
  models and two distributed motions
  ([report](../reports/2026-09-19-phase9-local-control.md)). MOT-O7 is
  resolved with it.
- ✅ **MOT-O9** (2026-09-19): §11.7 kept. Against three.js r168's
  `CCDIKSolver` on the same frames and inputs, at the same 40 iterations,
  §11.7 leaves a median 0.55 mm on an IK-authored motion where the reference
  leaves 10.5 mm, and both leave at most 29 mm
  ([report](../reports/2026-09-19-phase9-ik-reference.md)). MOT-O11 is
  opened with it.
- ⬜ **MOT-O11**: whether a knee's plane angle starts from its keyed rotation
  rather than from zero — all of the reference's advantage on a motion that
  keys its legs alongside their goals. Needs MMD's output on such a motion;
  the rule does not change without it.
- ✅ **MOT-O5 and MOT-O6** (2026-09-19): the humanoid role table, version 1,
  and how evaluated motion becomes `HumanJoint` rotations and root motion
  ([MOTION_CONTRACT.md §12](../design/MOTION_CONTRACT.md#12-the-humanoid-role-table)),
  measured against the 13 local characters and two distributed motions
  ([report](../reports/2026-09-19-phase9-roles-and-root.md)). No MMD bone is chosen as the root: it is the world
  transform of the joint `hips` maps to. MOT-O10, the rest a clip from MMD
  states, is opened with them.
- ⬜ **MOT-O10**: whether `mmdSkeletonAdapter` states a `SourceRestPose`
  measured from the rest bone directions — MMD's arms rest in an A — decided
  by a measured A-pose-to-level-arm retarget comparison. The first generic
  skeletal acceptance used identity rest rotations and therefore could not
  answer it.
- ✅ **`mmdMotionAdapter` and `mmdSkeletonAdapter`** (2026-09-21): digest-pinned
  `motionCore` and `motionRetarget` v0.5.0 packages; role-table version 1;
  stage-token `SkeletonDescriptor`, source rest and target `RetargetMap`;
  evaluated world rotations normalized into `MotionClip`, root motion and
  namespaced morph channels; unit, boundary and installed-consumer tests.
- ✅ **Acceptance end to end** (2026-09-22): through `motionRetarget` and
  `motionUsd`, a VMD-derived clip poses the PMX stage's
  skeleton with legs driven by IK, and retargets to a non-MMD synthetic
  skeleton through a translation unit whose interface and implementation name
  no MMD type. The deterministic test uses generated source and target rigs;
  a separate local run retargeted a 1,201-sample dance onto a 539-joint PMX
  stage, passed `usdchecker`, and was observed moving in `usdview`
  ([report](../reports/2026-09-22-phase9-motion-acceptance.md)).
- ⬜ **Expression interoperability** follows the skeletal adapter path. Keep
  every original `mmd:morph:<source name>` channel, then optionally emit only
  explicit, versioned, high-confidence semantic mappings such as blink and
  basic mouth visemes. Unknown model-specific morphs remain source channels
  and generic motion code contains no MMD name table. Model visibility remains
  independently available under `mmd:model:visibility`
  ([MOTION_CONTRACT.md §10.7](../design/MOTION_CONTRACT.md#107-morphs-as-channels)).

## Phase 8 — what remains

### Material schema and renderer integration

Phase 3 remains complete: contract-v1 stages already preserve every canonical
MMD material value and carry generic `preview` and `mtlx` fallbacks. Phase 8
formalizes that existing contract for an MMD-aware renderer; it does not replace
or reinterpret the fallbacks
([MATERIAL_POLICY.md](../design/MATERIAL_POLICY.md)).

- ✅ **Schema decision and attribute inventory** (2026-09-22):
  `MmdMaterialAPI` is a single-apply API on `UsdShadeMaterial`; existing
  `mmd:material:*` names, types and meanings are retained, neutral fallbacks are
  fixed, provenance stays outside the API, and MAT-O4 is resolved as an
  UsdImaging adapter rather than a third realization graph.
- ⬜ Add the `mmdSchema` plugin with generated C++/Python accessors and tokens;
  admit no other MMD schema by association.
- ⬜ Apply `MmdMaterialAPI` and author through it in `usdMmdFileFormat` as an
  additive stage-contract v1 change. Test both directions of compatibility:
  schema-aware consumers read earlier schema-less v1 attributes, while existing
  consumers can ignore `apiSchemas` and still read the unchanged properties and
  fallback graphs.
- ⬜ Keep `/preview` and `/mtlx` output, appearance and golden coverage stable
  while migrating the canonical authoring path.
- ⬜ Implement a UsdImaging adapter that exposes `MmdMaterialAPI` to Hydra
  without making the importer depend on `hydra-toon`.
- ⬜ Bring up the `hydra-toon` MMD path in this order: diffuse/alpha, toon
  ramp, sphere multiply/add, sub-texture, outline, shadow flags, material morph
  runtime, then advanced UV and vertex-color behavior.
- ⬜ After both `VrmMtoonMaterialAPI` and `MmdMaterialAPI` paths work, evaluate
  renderer-private common code. Do not introduce a USD-level `ToonMaterialAPI`
  until the two concrete implementations demonstrate stable common semantics.

### Avatar runtime composition

- ⬜ Consume the packages from `usd-avatar-runtime` — `usdMmdFileFormat`, and
  `motionVmd`, `mmdMotionBinding`, `mmdControl`, `mmdMotionAdapter` and
  `mmdSkeletonAdapter` for
  motion — and change a contract here only if that consumer shows one is
  wrong ([PACKAGE_CONTRACT.md](../architecture/PACKAGE_CONTRACT.md)).
- ⛔ `usdVmdFileFormat` waits for MOT-O2: `usd-motion-plugins` defines the
  standalone motion stage, and what a model-free VMD can put in it is still
  open ([MOTION_CONTRACT.md §9](../design/MOTION_CONTRACT.md#9-open-questions)).
  When it is answered, the basis functions a model-free `.vmd` stage needs are
  extracted from `mmdModel` with it (MOT-O1).
- ⬜ Verify generic motion targeting PMX with synthetic, BVH-derived and
  VRMA-derived clips through `mmdSkeletonAdapter`; no target path evaluates a
  VMD again.
- ⬜ Verify `motion-connectors → MotionPose → shared retarget → PMX` first from
  deterministic recorded captures. Live devices and network access are demo
  concerns, not CI requirements, and no protocol dependency enters this
  repository.

### Physics runtime integration

Static physics preservation is complete in Phase 6. Phase 8 owns only the
runtime composition described by
[PHYSICS_INTEGRATION.md](../design/PHYSICS_INTEGRATION.md):

- ⬜ verify that `usd-physics-plugins` can consume the existing
  `/Asset/physics` stage contract without backend-specific metadata;
- ⬜ establish the optional MMD coupling-adapter component and installed
  dependency only when that first consumer fixes the required API;
- ⬜ synchronize `followBone` bodies from the evaluated/retargeted pose;
- ⬜ simulate dynamic bodies and constrained pairs;
- ⬜ apply `dynamicWithBone` feedback to the runtime pose outside the importer;
- ⬜ improve limits, springs and damping incrementally, with backend-specific
  tolerances confined to backend tests.

The intended frame order is pose evaluation, bone → body synchronization,
shared physics step, body → bone feedback, then final pose consumption.
`usd-stage-runner` owns that order; `usd-avatar-runtime` owns composition and
playback state. PMX parsing and `Usd.Stage.Open("model.pmx")` remain usable
without any physics runtime.

## Completion criteria

[DESIGN_POLICY.md §14](../design/DESIGN_POLICY.md#14-phases)'s acceptance for
Phase 9 and for Phase 8.
