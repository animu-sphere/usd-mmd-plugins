# Phase 9 upper chest across skeletons (2026-09-25)

Dated evidence from real runs; append-only
([contributing/documentation.md](../contributing/documentation.md)).

## Question

MOT-O12. Two local models place `上半身3` between `上半身` and `上半身2`.
Role-table version 1 maps `上半身2` to `chest` and `上半身3` to `upperChest`,
so it puts `上半身3` above `上半身2`. On a motion that keys `上半身2`, arms
from those two models land up to 25° off on targets without `上半身3`
([rest-pose report](2026-09-25-phase9-rest-pose-comparison.md)). Which
mapping removes that error?

## What was run

The models, motions and scratch program were those of the
[rest-pose report](2026-09-25-phase9-rest-pose-comparison.md). The scratch
program measured the segment direction against MMD's own evaluation, with
identity rests, PMX onto PMX. Three mappings were compared:

- **A.** Version 1.
- **B.** The two bones in the model's chain order, ancestor as `chest`, on
  both sides.
- **C.** B as a target. As a source, the bone the neck hangs from is `chest`,
  and `upperChest` is never emitted.

Four models were run first, paired so that every combination of source and
target with and without `上半身3` occurs once. C, adopted as version 2, was
then built into `mmdSkeletonAdapter` and run over all 17 models.

## Why ordering alone does not help

B is no better than A onto a target without `上半身3`:

| Pair, motion C (upper arm, median / max) | A | B | C |
| --- | ---: | ---: | ---: |
| with `上半身3` → without | 8.62° / 27.08° | 9.05° / 25.21° | 3.78° / 3.78° |
| with → with | 2.77° / 2.77° | 2.77° / 2.77° | 2.77° / 2.77° |
| without → with | 1.74° / 1.74° | 1.74° / 1.74° | 1.74° / 1.74° |

The shared retarget drops a joint the target does not bind. It does not fold
that joint's rotation into its child (`usd-motion-plugins`'
RETARGETING_POLICY §4.1, case 6, which states that folding is not promised).
So whichever of the two bones is `upperChest` loses its local rotation on
such a target. Under A, the neck's rotation relative to `上半身3` then carries
`上半身2`'s key a second time. Under B, `上半身2`'s key is dropped outright.
Either way the neck and arms are off by that key. C emits nothing that can
be dropped. The neck's bone carries its full world rotation as `chest`, so
the neck and arms are placed correctly on every target.

## Version 2 over the corpus

All 17 models, PMX onto PMX, identity rests, motion C. Each figure is the
median over pairs of each pair's median, then the largest 95th percentile and
the largest angle of any pair:

| Segment | Version 1 | Version 2 |
| --- | ---: | ---: |
| neck | 2.73° / 19.37° / 22.37° | 2.66° / 11.89° / 11.89° |
| upper arm | 2.63° / 18.49° / 25.20° | 2.45° / 5.26° / 5.26° |
| lower arm | 2.04° / 18.94° / 25.68° | 1.82° / 5.62° / 5.62° |
| hand | 3.00° / 18.26° / 22.91° | 2.68° / 10.04° / 10.04° |

Under version 2, motion C's figures are motion A's: the only arm error left
is the difference between two models' rests. Motion A does not key `上半身2`,
and none of its neck, arm or hand figures changed.

On a target that has `上半身3`, the `chest` segment's angle changes by a
constant: 2.2° to 6.7° on one pair, 0.2° to 2.2° on another. That target's
`chest` is now `上半身3` rather than `上半身2`, so a different pair of joints is
measured. The neck, arms and hands of those pairs did not change.

## Limits

- No local model chains `上半身3` above `上半身2`. The target rule for that
  order (`上半身2` as `chest`) is covered only by a synthetic unit test.
- As a source, the split of a bend between `上半身3` and `上半身2` is not
  carried. A target with both takes the whole bend in `chest` and leaves
  `upperChest` at rest.
