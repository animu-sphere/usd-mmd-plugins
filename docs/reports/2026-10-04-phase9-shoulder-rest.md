# Phase 9 the shoulder's reference rest (2026-10-04)

Dated evidence from real runs; append-only
([contributing/documentation.md](../contributing/documentation.md)).

## Question

[MOTION_CONTRACT.md §12.6](../design/MOTION_CONTRACT.md#126-the-arm-chains-reference-rest)
aims the whole arm chain onto the shared core's T-pose directions: the
shoulder, the upper arm, the lower arm and the hand. A mocopi capture
retargeted onto a PMX through the shipped adapter was viewed in `usdview`,
and the character's shoulders sat visibly raised, although the arms were
right. Is the shoulder's aim what raises them, and is the raise a pose or
the model's shape?

## What was run

A scratch program, outside this repository, against its install tree at
`df40556` and `usd-motion-plugins` v0.5.3. The source was a local mocopi
device session of 2026-08-15, one of those `motion-connectors`'
`recorded/manifest.json` describes, which starts with a still stand. It was
replayed through `motion-connectors` v0.1.0's `MocopiConnector` and
retargeted with §10.9's recipe onto the 17 local characters of the earlier
reports.

It measured the shoulder segment, from `肩` to `腕`, as an elevation above
horizontal. It did so in each model's own rest, and at the source's sample
300, 5 s into the take, at the end of its still stand. It ran twice:

- **as shipped**: the adapter's `targetRest`, with the shoulder aimed;
- **shoulder unaimed**: the same `targetRest`, except that each shoulder's
  slot is unset, so the shoulder rests as the model does. Each upper arm's
  slot is restated under it so that the upper arm keeps the shipped aimed
  world rest.

The runs were on Windows 11 with OpenUSD 26.08. None of the inputs is
committed or redistributed
([DESIGN_POLICY.md §13](../design/DESIGN_POLICY.md#13-testing-policy)).

## Results

The source's own shoulder segment rises 13.7° in its rest. At sample 300 it
rises 15.3° on the left and 11.8° on the right. Its clavicles are therefore
within 2° of their rest.

| | Left | Right |
| --- | ---: | ---: |
| The model's own shoulder slope: median, range | −17.4°, −30.2° to −5.6° | −17.4°, −30.2° to −5.1° |
| As shipped, standing: median elevation | 0.1° | −3.6° |
| As shipped, above the model's own slope: median, max | 17.4°, 30.3° | 13.7°, 26.6° |
| Shoulder unaimed, standing: median elevation | −16.8° | −20.4° |
| Shoulder unaimed, above the model's own slope: median, range | 0.5°, 0.0° to 1.5° | −3.1°, −3.7° to −1.9° |

The upper and lower arm's directions are the same in both runs on every
model. On the model first viewed, the standing upper arm is at −84.26°
against the source's −84.45°.

## Findings

**The shoulder's aim raises it by the model's own slope.** Every local
character's `肩` slopes down, from 5° to 30°. The aim calls the level
direction the shoulder's rest. So a source clavicle that is near its own rest
puts the PMX shoulder near level: 17.4° above where the model rests it, in
the median, and 30° above at worst. That is the raised look in the viewer.

**The slope is the model's shape, not a pose.** The arms are different. A
PMX's arms hang 37–42° in its rest while a level-arm rig's are level, and
that is two different poses: MOT-O10's reason for the aim. A shoulder at
rest is neutral in both rigs. MMD's own neutral is the shoulder at its
identity rotation, which is the slope. The source's clavicle, which rises
13.7° in its rest, is neutral too. Aiming both onto one direction turns a
difference of shape into a difference of pose.

**The earlier measure counted the slope as an error.** Against a level-arm
rig, the [rest-pose comparison](2026-09-25-phase9-rest-pose-comparison.md)
found the PMX shoulder off by a median 17.58° and at most 30.32° before the
aim, and 0.00° with it. Those are the slopes above, to within 0.2°,
because that rig's clavicles are level. A segment direction that matches
another rig's is the right goal for an arm, whose rest differs by pose. It
is the wrong goal for a shoulder, whose rest differs by shape.

**Leaving the shoulder unaimed puts it where the model rests it.** The
standing shoulder then sits within the source clavicle's own 2° of the
model's slope. The arms do not change: their aim is the upper arm's, and it
stays.

## Limits

- The experiment kept the shipped upper-arm world rest. The revised rule
  aims the upper arm from the model's own shoulder rest instead. That turns
  the upper arm onto the same direction, so the figures above hold. Its roll
  can differ, and roll was not measured. The implementation reruns these
  figures with the adapter's own rests.
- Only a mocopi source onto a PMX was measured. The other directions follow
  from the same rule, but they were not measured: a level-clavicle VRMA onto
  a PMX, and a PMX as the source onto a non-MMD rig. The second was expected
  to drop that rig's clavicles by the PMX's slope.
