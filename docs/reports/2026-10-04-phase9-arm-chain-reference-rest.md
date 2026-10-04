# Phase 9 arm chain's reference rest, as shipped (2026-10-04)

> Later: the shoulder was left out of the aim on 2026-10-04, because its rest slope is the model's shape and aiming it raised a PMX's shoulders, MOT-O14 ([report](2026-10-04-phase9-shoulder-rest.md)).

Dated evidence from real runs; append-only
([contributing/documentation.md](../contributing/documentation.md)).

## Question

[MOTION_CONTRACT.md §12.6](../design/MOTION_CONTRACT.md#126-the-arm-chains-reference-rest)
resolves MOT-O10: `mmdSkeletonAdapter` aims the arm chain alone onto the
shared T-pose directions, and states that rest on both sides, as the
source's `SourceRestPose` and as a PMX target's `TargetRestPose`. It was
decided from a measurement whose rests existed only in a scratch program
([report](2026-09-25-phase9-rest-pose-comparison.md)). Does the adapter, as
shipped against `usd-motion-plugins` v0.5.2, give the same answers?

## What was run

The same corpus and motions as that measurement: the 17 character models
over 1 MB that resolve the required roles, of 18 local PMX files over 1 MB
(the 1.2 MB prop is skipped again), and motions A and C, sampled every 5th
frame. All are distributed files. None is a fixture, and none is committed or
redistributed
([DESIGN_POLICY.md §13](../design/DESIGN_POLICY.md#13-testing-policy)). The
runs were on Windows 11.

The scratch program of 2026-09-25 was rebuilt against this branch's installed
packages and the digest-pinned `motionRetarget` and `motionSource` v0.5.2. It
now takes the rests from `mmd::skeleton::Adapt` alone: `sourceRest` as the
clip's rest, and `targetRest` as `RetargetOptions::targetRest`. Only its
"today" rows still build an identity rest, by clearing the shipped rest's
rotations. Ground truth is MMD's own evaluated segment directions, as before,
and each figure is the median over model pairs of each pair's median angle,
then the largest angle of any sample.

## The shipped aim is the measured one

Over every model, the world reference rest the adapter states for each role
differs from the scratch program's arm-chain construction by **0°**, as a
source and as a target. The target's rest is stated over the PMX skeleton's
own joints. `肩P`, `肩C` and the twist bones are left unset and pass the aim
on, and it composes to the same world rests.

## Results

Motion A. Motion C gives the same figures to 0.01°.

| Segment | Onto a level-arm rig, identity rests | Onto a level-arm rig, shipped source rest | PMX to PMX, identity rests | PMX to PMX, shipped source rest only | PMX to PMX, both shipped rests | Level-arm source to PMX, no target rest | Level-arm source to PMX, shipped target rest |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| shoulder | 17.58° / 30.32° | 0.00° / 0.04° | 7.51° / 22.28° | 17.58° / 30.32° | 0.00° / 0.04° | 17.58° / 30.32° | 0.00° / 0.04° |
| upper arm | 40.16° / 42.42° | 0.00° / 0.04° | 2.45° / 5.26° | 40.16° / 42.42° | 0.00° / 0.04° | 40.16° / 42.42° | 0.00° / 0.04° |
| lower arm | 39.82° / 42.41° | 0.00° / 0.04° | 1.82° / 5.62° | 39.82° / 42.41° | 0.00° / 0.03° | 39.82° / 42.41° | 0.00° / 0.03° |
| hand | 39.74° / 42.38° | 0.00° / 0.04° | 2.68° / 10.04° | 39.50° / 48.10° | 3.55° / 8.39° | 39.50° / 48.10° | 3.55° / 8.39° |

Left side shown. The right side agrees to 0.1°.

- Onto a level-arm rig, the shipped source rest removes the A-pose difference
  entirely, as the scratch rest did.
- PMX to PMX with **both** shipped rests is exact for the shoulder and arms.
  The hand's 3.55° is the hand-to-middle-finger line, which differs between
  models' hands: the hand is not aimed (§12.6), so it carries the lower arm's.
- The rest stated on the source **alone** makes PMX to PMX as wrong as a
  level-arm target is with identity rests. This is §12.6's "both or
  neither", and why the adapter returns the target rest beside the map.
- A level-arm source reaches the PMX 40° low without the target rest, and
  exactly with it.
- The torso and legs are unchanged in every row. The rest stated for them is
  identity, as before: spine, chest and neck 0.00° onto the level-arm rig,
  and PMX to PMX a median 1.7–2.7°, the difference between two models' rests.

## Tests behind it

`mmdSkeletonAdapter_unit` now builds synthetic A-pose rigs, with a known arm
angle, `肩C` and a twist bone between chain joints, and two fingers under each
hand. It asserts the aim of every chain role and that the hand inherits; that
the target slots are set on the chain joints only and compose to the source's
world rests; and that the descriptor keeps identity rests. Then it
retargets with `PoseRetargeter`, at rest and with an arm raised 20°:

- 40° A-pose source onto a level-arm rig;
- 40° onto a 30° A-pose with both rests: under 0.01°;
- the same without the target rest: off by the target's own 30°;
- level-arm source onto the 30° A-pose: level arms.

`mmdMotionAdapter_acceptance` passes the target rest when it retargets onto
the PMX stage skeleton, and none onto the generic level-arm rig.

## Limits

Roll about each bone is still not measured: segment directions cannot see
it, and the shortest rotation adds none. No clip was compared with a real
VRM model's rest, only with the level-arm rig built from each model's own
proportions.
