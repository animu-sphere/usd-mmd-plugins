# Phase 8 — avatar runtime composition

Status: ⬜ not started.

Phase 8 composes this repository with `usd-vrm-plugins`, `usd-motion-plugins`,
`motion-connectors`, `hydra-toon` and `usd-stage-runner` through OpenStrata,
under `usd-avatar-runtime`
([DESIGN_POLICY.md §14](../design/DESIGN_POLICY.md#14-phases)). Most of it is
owned outside this repository: the runtime, not this repository, is the avatar
execution environment. What is listed here is only the part this repository
owes, or waits for.

## Outcome

```text
usd-avatar-runtime
    → opens a .pmx through usdMmdFileFormat's installed package
    → reads a .vmd through motionVmd and binds it through mmdMotionBinding
    → evaluates MMD IK and append transforms over /Asset/rig, and bakes
      (MOT-O3: the runtime's, not this repository's)
```

## What remains

- ⬜ Decide which version carries the first substantial release: every Phase
  [DESIGN_POLICY.md §14.1](../design/DESIGN_POLICY.md#141-first-substantial-release--definition-of-done)
  names has landed ([README.md](README.md#status-at-a-glance)).
- ⬜ Consume the packages from `usd-avatar-runtime` — `usdMmdFileFormat`, and
  `motionVmd` and `mmdMotionBinding` for motion — and change a contract here
  only if that consumer shows one is wrong
  ([PACKAGE_CONTRACT.md](../architecture/PACKAGE_CONTRACT.md)).
- ⛔ `usdVmdFileFormat` waits for MOT-O2, the shared motion contract's answer
  to what a directly opened `.vmd` stage looks like
  ([MOTION_CONTRACT.md §9](../design/MOTION_CONTRACT.md#9-open-questions)).
  When it is answered, the basis functions a model-free `.vmd` stage needs are
  extracted from `mmdModel` with it (MOT-O1).
- ⛔ `motionVmd` moves to `usd-motion-plugins`, or depends on its contract,
  once that repository publishes an installable one
  ([MOTION_CONTRACT.md §2](../design/MOTION_CONTRACT.md#2-components-and-boundaries)).

## Completion criteria

[DESIGN_POLICY.md §14](../design/DESIGN_POLICY.md#14-phases)'s acceptance for
Phase 8: the runtime, not this repository, is the avatar execution
environment.
