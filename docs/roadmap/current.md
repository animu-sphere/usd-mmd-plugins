# Phase 6 — physics preservation

Status: ⬜ not started.

`/Asset/physics` is the reserved scope for PMX's rigid bodies and joints.
Phase 6 authors them with standard `UsdPhysics` where its semantics match and
preserves every other parameter as `mmd:physics:*`, so every rigid body and
joint is recoverable from the stage — and steps no simulation, as
[DESIGN_POLICY.md §8](../design/DESIGN_POLICY.md#8-physics-policy) requires.

## Outcome

```text
Usd.Stage.Open("model.pmx")
    → …                                   the Phase 0–5 stage, unchanged
    → /Asset/physics/rigidBodies/<id>     shapes, mass, damping, groups, mode
    → /Asset/physics/joints/<id>          constraints, limits, springs
```

## What remains

- ⬜ Fix the PMX Euler composition order of rigid-body and joint rotations
  against a reference implementation — PMX-O1, in
  [PMX_CONTRACT.md §16](../design/PMX_CONTRACT.md#16-open-questions); until
  then rotations are carried unconverted and not authored.
- ⬜ Decide the physics prim shapes: which PMX concepts standard `UsdPhysics`
  matches, and the typeless `mmd:physics:*` encoding of the rest — the
  physics half of STAGE-O6, in
  [STAGE_CONTRACT.md §16](../design/STAGE_CONTRACT.md#16-open-questions),
  following the rig half decided in Phase 5.
- ⬜ Carry rigid bodies and joints in `mmdModel`: identity, bone attachment by
  canonical joint, shapes and sizes, positions and rotations through the
  basis conversion, spring constants in source units.
- ⬜ Author `/Asset/physics` and make
  [STAGE_CONTRACT.md §13](../design/STAGE_CONTRACT.md#13-physics--reserved)
  binding, with fixtures for every shape, joint type and repair; an impulse
  morph's `mmd:morph:rigidBodyIndices` then has prims to name.

## Completion criteria

[DESIGN_POLICY.md §14](../design/DESIGN_POLICY.md#14-phases)'s acceptance for
Phase 6: **no simulation; every rigid body and joint recoverable**.

Phase 6's preservation is the last part of what
[DESIGN_POLICY.md §14.1](../design/DESIGN_POLICY.md#141-first-substantial-release--definition-of-done)
wants for the first substantial release; Phases 0–5 are done.
