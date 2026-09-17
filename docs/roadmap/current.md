# Phase 9, then Phase 8 — the shared motion core, and avatar runtime composition

Status: ⬜ not started.

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

As of 2026-09-17 `usd-avatar-runtime`, `usd-motion-plugins` and
`motion-connectors` hold no commits. `mmdControl` depends on none of them,
so it is the one item that can start now.

## Outcome

```text
.pmx ─→ usdMmdFileFormat ─→ /Asset stage
.vmd ─→ motionVmd ─→ mmdMotionBinding ─→ mmdControl ─→ mmdMotionAdapter ─→ MotionClip
                                                                              │
usd-motion-plugins:  retarget to any skeleton, record, author UsdSkelAnimation ◀┘
usd-avatar-runtime:  schedules the above per frame, composes the stage, renders
```

## Phase 9 — what remains

- ⬜ **`mmdControl`.** Create the library as
  [WORKSPACE.md §1.2](../architecture/WORKSPACE.md#12-later-only-when-their-responsibility-is-real)
  reserves it: Bézier sampling of a bound motion, evaluation in MMD's order,
  appends, IK with the IK-enable track, with its boundary, unit, robustness
  and sanitizer tests. Synthetic rigs with known answers come from the
  committed fixture generators, never from distributed models.
- ⬜ **MOT-O7**, morphs: which types are evaluated into the pose and which
  travel as channels. Needed before `mmdControl` is complete.
- ⬜ **MOT-O5**, root motion: which MMD bones feed `RootMotion`, measured
  against distributed motions in a dated report.
- ⬜ **MOT-O6**, the humanoid role table and its version, measured against the
  local models the Phase 7 report used.
- ⛔ **`mmdMotionAdapter`** waits for `usd-motion-plugins` to publish an
  installable `motion-core` ([DEPENDENCIES.md §6](../architecture/DEPENDENCIES.md#6-usd-motion-plugins)).
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
