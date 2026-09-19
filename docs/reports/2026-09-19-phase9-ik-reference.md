# Phase 9 IK against an independent implementation (2026-09-19)

Dated evidence from real runs; append-only
([contributing/documentation.md](../contributing/documentation.md)).

## What was run

The 12 characters and two dance motions of the
[Phase 9 control report](2026-09-19-phase9-local-control.md) — distributed
files, none a fixture, none committed or redistributed
([DESIGN_POLICY.md §13](../design/DESIGN_POLICY.md#13-testing-policy)) — on
Windows 11. No library of this repository was changed for them.

1. **This repository.** A scratch program linking the installed
   `mmdControl` evaluated every frame of each motion on each character and
   wrote, for both leg chains (`左足ＩＫ`, `右足ＩＫ`), the world positions of
   the goal, the effector (`足首`), the knee (`ひざ`) and the hip (`足`), and
   whether the chain was enabled.
2. **The reference.** three.js r168's `MMDLoader` built each character's
   bones, IK chains and appends from its PMX, and its `MMDAnimationHelper`,
   with `pmxAnimation` on and physics off, evaluated the same frames in Node
   through its `CCDIKSolver` — an implementation written independently of
   this one, at each chain's stored loop count (40 for every leg here). The
   same four positions were written, converted at 0.08 m per unit.
3. **Two variants of §11.7**, built in scratch from this repository's
   `Evaluator.cpp` with one change each: one never stops early, the other
   starts a plane link's angle from the link's keyed rotation about its axis
   instead of from zero.

Distances are those of the control report: a leg frame is *within reach*
when its goal is no farther from the hip than 98% of the leg's rest length,
and the residual is the distance from the effector to the goal.

## The inputs agree

Two differences had to be removed before the solvers could be compared:

- **Curves.** three.js reads a bone key's Z and rotation `x1` from bytes 2
  and 3; `motionVmd` reads them from row 1, because later MMD versions write
  a physics toggle there ([MOTION_CONTRACT.md §6](../design/MOTION_CONTRACT.md#6-interpolation)).
  Motion C holds that toggle, and as three.js read it the `足ＩＫ` goals moved
  up to 1.08 m away from `motionVmd`'s on 502 of 2402 leg frames. The
  reference was given row 1's bytes.
- **The last frame.** three.js's mixer loops, so the last frame of motion A
  evaluated as frame 0; the reference was made to clamp.

After both, every goal and every hip agreed to 0.02 mm on every frame of
both motions: what differs below is the solve alone.

## Residuals

Motion A (IK-authored: `足` and `ひざ` keyed on 41–303 frames of 2796), 37 271
leg frames within reach of 67 440; motion C (`足` and `ひざ` keyed on every
frame, alongside their IK goals), 24 122 of 28 824.

| Solver | A: p50 / p99 / max | A: under 1 mm | C: p50 / p99 / max | C: under 1 mm |
| --- | --- | ---: | --- | ---: |
| §11.7 (`mmdControl`) | 0.55 / 16.0 / 29.1 mm | 60% | 2.38 / 23.8 / 28.9 mm | 41% |
| three.js r168 | 10.5 / 29.0 / 29.2 mm | 23% | 0.06 / 11.4 / 29.1 mm | 85% |
| §11.7, plane angle from the keyed rotation | 0.59 / 18.4 / 29.1 mm | 58% | 0.05 / 10.2 / 28.5 mm | 87% |

Per character (bones, chains):

| Model | A: §11.7 | A: three.js | C: §11.7 | C: three.js |
| --- | --- | --- | --- | --- |
| 280, 4 | 0.08 / 9.2 / 20.1 | 4.6 / 22.3 / 22.3 | 0.07 / 13.8 / 21.1 | 0.01 / 7.5 / 22.2 |
| 516, 4 | 1.1 / 18.8 / 26.9 | 15.7 / 27.5 / 27.5 | 7.1 / 25.8 / 27.5 | 0.14 / 12.5 / 27.5 |
| 490, 4 | 0.31 / 10.9 / 16.9 | 5.3 / 16.9 / 16.9 | 1.1 / 13.6 / 16.9 | 0.03 / 4.8 / 15.7 |
| 319, 12 | 0.94 / 14.1 / 19.5 | 10.3 / 21.3 / 21.3 | 4.4 / 17.7 / 20.7 | 0.15 / 7.9 / 21.1 |
| 319, 11 | 0.94 / 14.1 / 19.5 | 10.3 / 21.3 / 21.3 | 4.4 / 17.7 / 20.7 | 0.15 / 7.9 / 21.1 |
| 566, 4 | 1.1 / 19.0 / 26.3 | 15.5 / 26.7 / 26.8 | 5.2 / 21.7 / 26.7 | 0.04 / 16.0 / 26.7 |
| 444, 4 | 0.05 / 6.6 / 16.3 | 2.9 / 19.6 / 19.7 | 0.02 / 11.6 / 17.9 | 0.01 / 7.5 / 19.5 |
| 633, 4 | 0.74 / 18.2 / 29.1 | 14.9 / 29.2 / 29.2 | 2.7 / 23.9 / 28.8 | 0.07 / 16.2 / 29.0 |
| 366, 4 | 0.74 / 14.3 / 20.7 | 9.9 / 21.3 / 21.4 | 3.8 / 17.5 / 20.9 | 0.09 / 14.0 / 21.3 |
| 461, 16 | 0.45 / 13.7 / 20.5 | 9.8 / 24.5 / 24.5 | 2.9 / 15.6 / 20.5 | 0.05 / 12.3 / 24.5 |
| 692, 5 | 0.75 / 18.2 / 29.1 | 14.9 / 29.2 / 29.2 | 2.8 / 23.9 / 28.9 | 0.10 / 16.7 / 29.1 |
| 692, 5 | 0.75 / 18.2 / 29.1 | 14.9 / 29.2 / 29.2 | 2.8 / 23.9 / 28.9 | 0.10 / 16.7 / 29.1 |

p50 / p99 / max in millimetres. §11.7's rows are the control report's,
recomputed: the medians and maxima are its own, and p99 is within 0.3 mm of
it, taken by a different percentile rule.

- On motion A, §11.7 leaves the effector closer than the reference on 27 268
  frames and farther on 1 504 (by more than 0.5 mm either way); on motion C,
  closer on 586 and farther on 14 717.
- **The largest residual is the same in both**, 16–29 mm per character. At
  40 iterations neither implementation finishes the hardest poses; the
  control report showed §11.7's falling under 0.1 mm at 256.
- **Stopping early changes nothing here.** The variant that never stops gave
  the same bits as §11.7 on every frame of both motions: no iteration failed
  to improve on these legs.
- **Motion C's difference is the knee's starting angle.** §11.7 starts every
  enabled chain's plane angle at zero, so a knee is solved from straight,
  whatever the motion keys; three.js starts from the keyed rotation. Given
  the keyed angle as its start, §11.7 leaves a median 0.05 mm on C — as
  close as the reference — and closer than it on 1 901 frames, farther on
  1 434, and its knees move a median 4.6 mm and at most 66 mm. On A, where
  the knees are keyed on few frames, the same change moves the median from
  0.55 to 0.59 mm and p99 from 16.0 to 18.4 mm, and the knees at most 35 mm.

## The poses differ

Residuals say how close each leg got, not whether the two agree. The knee's
world position, §11.7 against the reference, over every enabled leg frame:

| | p50 | p99 | max |
| --- | ---: | ---: | ---: |
| A: §11.7 | 19.6 mm | 67.1 mm | 360 mm |
| A: §11.7, keyed start | 17.9 mm | 66.6 mm | 360 mm |
| C: §11.7 | 4.9 mm | 54.0 mm | 68.4 mm |
| C: §11.7, keyed start | 0.5 mm | 27.7 mm | 68.4 mm |

Two implementations of cyclic coordinate descent at the same loop count
reach the same goals with knees centimetres apart, and on 31 of motion A's
67 440 leg frames more than 100 mm apart, on every character. The limits are
held differently too: three.js rotates a knee about any axis and then clamps
its Euler angles, where §11.7's plane links rotate about their one axis only.

## What this decided

- **MOT-O9, kept**: §11.7 is not changed. An independent implementation at
  the same stored loop count leaves no less distance overall — a larger
  median on the IK-authored motion, the same largest on both — and where it
  does leave less, the difference is where the knee starts, not the
  descent. The 29 mm that remains is 40 iterations of cyclic coordinate
  descent, in either implementation.
- **Opened, MOT-O11**: whether a plane link starts from its keyed rotation.
  §11.7 discards a knee's keyed rotation whenever its chain is enabled; a
  motion that keys its legs as well as their goals, like C, is solved from a
  straight knee, and leaves a median 2.4 mm where a start from the keys
  leaves 0.05 mm. Which of the two MMD does is told by MMD's output alone;
  three.js is one implementation's reading, not MMD's.
- **Unverified, still**: IK matching MMD's own playback. This run compared
  against a second implementation, and its knees disagree with §11.7's by
  centimetres; neither is MMD.
