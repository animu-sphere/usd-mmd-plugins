# Phase 9, then Phase 8 — the shared motion core, and avatar runtime composition

Status: 🚧 Phase 9 in progress — `mmdControl` is done; Phase 8 not started.

Two Phases remain, and they run in this order although they are numbered the
other way ([DESIGN_POLICY.md §14](../design/DESIGN_POLICY.md#14-phases)):

- **Phase 9 — shared motion core adoption.** This repository evaluates MMD
  motion over the control rig (`mmdControl`) and hands the result to
  `usd-motion-plugins` as a `MotionClip` (`mmdMotionAdapter`)
  ([MOTION_CONTRACT.md §10](../design/MOTION_CONTRACT.md#10-normalizing-into-the-shared-motion-core)).
- **Phase 8 — avatar runtime composition.** `usd-avatar-runtime` composes this
  repository with `usd-vrm-plugins`, `usd-motion-plugins`,
  `motion-connectors`, `hydra-toon` and `usd-stage-runner` through
  OpenStrata. Most of it is owned outside this repository: the runtime, not
  this repository, is the avatar execution environment.

What is listed here is only the part this repository owes, or waits for; what
the runtime consumes from here today is [v0.1.0](../releases/v0.1.0.md).

As of 2026-09-19 `usd-avatar-runtime` holds no commits and
`motion-connectors` a scaffold. `usd-motion-plugins` has tagged
`v0.1.0-alpha.1`, a source-only pre-release of `motionCore` alone, without
`SkeletonDescriptor` or `RetargetMap`
([DEPENDENCIES.md §6](../architecture/DEPENDENCIES.md#6-usd-motion-plugins)),
so `mmdMotionAdapter` still cannot start. `mmdControl` depended on none of
them, and is done.

## Outcome

```text
.pmx ─→ usdMmdFileFormat ─→ /Asset stage
.vmd ─→ motionVmd ─→ mmdMotionBinding ─→ mmdControl ─→ mmdMotionAdapter ─→ MotionClip
                                                                              │
usd-motion-plugins:  retarget to any skeleton, record, author UsdSkelAnimation ◀┘
usd-avatar-runtime:  schedules the above per frame, composes the stage, renders
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
- ⬜ **MOT-O9**: whether MMD's own IK leaves a reachable goal closer than
  §11.7 does at a model's loop count — at 40 iterations the report measures a
  leg's effector up to 29 mm from a goal within reach. Needs a reference to
  compare against; the rule does not change without one.
- ⬜ **MOT-O5**, root motion: which MMD bones feed `RootMotion`, measured
  against distributed motions in a dated report.
- ⬜ **MOT-O6**, the humanoid role table and its version, measured against the
  local models the Phase 7 report used.
- ⛔ **`mmdMotionAdapter`** waits for `usd-motion-plugins` to publish an
  installable `motion-core` with `SkeletonDescriptor` and `RetargetMap`
  ([DEPENDENCIES.md §6](../architecture/DEPENDENCIES.md#6-usd-motion-plugins)).
  `mmdControl`'s pose is its input: local transforms per canonical joint in
  the USD basis, and the morph channels.
  When it does, the edge is declared in the manifest and gated as
  [WORKSPACE.md §2.4](../architecture/WORKSPACE.md#24-edges-out-of-this-repository)
  says, and `SkeletonDescriptor`, `RetargetMap` and `MotionClip` are built as
  MOTION_CONTRACT §10.4–§10.7 say.
- ⛔ **Acceptance end to end** waits for `usd-motion-plugins`' retarget and
  `UsdSkelAnimation` authoring: a VMD-derived clip poses the PMX stage's
  skeleton with legs driven by IK, and retargets to a non-MMD synthetic
  skeleton with no MMD code on that path.

## Phase 8 — what remains

- ⬜ Consume the packages from `usd-avatar-runtime` — `usdMmdFileFormat`, and
  `motionVmd`, `mmdMotionBinding`, `mmdControl` and `mmdMotionAdapter` for
  motion — and change a contract here only if that consumer shows one is
  wrong ([PACKAGE_CONTRACT.md](../architecture/PACKAGE_CONTRACT.md)).
- ⛔ `usdVmdFileFormat` waits for MOT-O2: `usd-motion-plugins` defines the
  standalone motion stage, and what a model-free VMD can put in it is still
  open ([MOTION_CONTRACT.md §9](../design/MOTION_CONTRACT.md#9-open-questions)).
  When it is answered, the basis functions a model-free `.vmd` stage needs are
  extracted from `mmdModel` with it (MOT-O1).

## Completion criteria

[DESIGN_POLICY.md §14](../design/DESIGN_POLICY.md#14-phases)'s acceptance for
Phase 9 and for Phase 8.
