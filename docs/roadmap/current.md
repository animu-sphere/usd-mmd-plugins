# Phase 7 — VMD

Status: ⬜ not started.

Phase 7 reads VMD motion. `libs/motionVmd` is a plain, extraction-ready
library — VMD syntax, CP932 decoding, the motion source representation, no
OpenUSD and no dependency on the model libraries — integrated with the shared
motion architecture, as
[MOTION_CONTRACT.md](../design/MOTION_CONTRACT.md) describes. A `.vmd` opens
directly as a stage only once that contract says what such a stage is.

## Outcome

```text
motionVmd::Read("motion.vmd")
    → bone, morph, camera, light, self-shadow and IK/visibility tracks,
      names decoded from CP932 with their raw bytes kept
vmd_inspect motion.vmd
    → what the file holds
```

## What remains

- ⬜ Decide where the shared basis-conversion functions live, so `mmdModel`
  and `motionVmd` share them without depending on each other — MOT-O1, in
  [MOTION_CONTRACT.md §9](../design/MOTION_CONTRACT.md#9-open-questions).
- ⬜ Decide which runtime owns MMD IK and append evaluation for baking —
  MOT-O3, in the same section.
- ⬜ Create `libs/motionVmd`: every VMD section under the PMX reading rules,
  CP932 through a table the project owns, `MMD_TEXT_TRUNCATED_CP932` and
  `MMD_MOTION_DUPLICATE_KEYFRAME`, with synthetic fixtures and a fuzzing lane
  like the parser's.
- ⬜ Create `tools/vmdInspect`.
- ⬜ Binding a VMD to a PMX model by name, as a separate operation
  ([MOTION_CONTRACT.md §8](../design/MOTION_CONTRACT.md#8-binding-a-vmd-to-a-pmx-model)).
- ⬜ `usdVmdFileFormat` waits for MOT-O2, the shared motion contract's answer
  to what a directly opened `.vmd` stage looks like.

## Completion criteria

[DESIGN_POLICY.md §14](../design/DESIGN_POLICY.md#14-phases)'s acceptance for
Phase 7: per [MOTION_CONTRACT.md](../design/MOTION_CONTRACT.md).

Phases 0–6 are done, so every Phase
[DESIGN_POLICY.md §14.1](../design/DESIGN_POLICY.md#141-first-substantial-release--definition-of-done)
names for the first substantial release has landed; Phase 7 is not part of
it.
