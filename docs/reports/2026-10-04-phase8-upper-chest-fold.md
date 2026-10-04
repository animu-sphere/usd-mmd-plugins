# Phase 8 upper-chest fold through the shared core (2026-10-04)

Dated evidence from real runs; append-only
([contributing/documentation.md](../contributing/documentation.md)).

## Question

MOT-O13: on a PMX without `上半身3`, the shared retarget dropped a generic
clip's `upperChest` motion. The
[generic-clips report](2026-10-04-phase8-generic-clips-onto-pmx.md) of the
same day measured a rest-aware fold, `Qc · Quc · S⁻¹`, written by hand in a
scratch program. `usd-motion-plugins` v0.5.3 then shipped that fold as an
opt-in, `RetargetOptions::foldUnboundIntermediateRotations`
([its RETARGETING_POLICY.md §4.2](https://github.com/animu-sphere/usd-motion-plugins/blob/v0.5.3/docs/design/RETARGETING_POLICY.md#42-opt-in-folding-of-unbound-intermediate-rotations)).
Does the shipped option, set as
[MOTION_CONTRACT.md §10.9](../design/MOTION_CONTRACT.md#109-a-generic-clip-onto-a-pmx)
says, give the figures the hand-written fold gave?

## What was run

The same scratch program and inputs as the generic-clips report, rebuilt
against this branch's install tree and the digest-pinned
`usd-motion-plugins` v0.5.3 packages. It ran three times over every pair:

- **default**: the retarget as before, without the fold;
- **hand-written**: the earlier experiment, which folds the clip's samples
  before the retarget;
- **shipped**: the clip unchanged, with
  `foldUnboundIntermediateRotations = true`.

The inputs are the seven VRMA clips through `usdVrmaFileFormat`, the mocopi
BVH from `usd-motion-plugins`' corpus through `motion_convert`, and the 17
local characters. The 1.2 MB prop is run but not counted, as before. None of
these files is a fixture, and none is committed or redistributed
([DESIGN_POLICY.md §13](../design/DESIGN_POLICY.md#13-testing-policy)). The
figures are the earlier report's: per segment, the median over pairs of each
pair's median angle, the largest angle, and the spread. The runs were on
Windows 11 with OpenUSD 26.08.

## Results

The default run reproduces the earlier report's "Without" column exactly, on
both producers. Over all 2,228 segment rows, the shipped run differs from
the hand-written one by at most 0.005°. Left side, on the 15 models without
`上半身3`:

| Segment | VRMA default: median / max / spread | VRMA shipped | mocopi default | mocopi shipped |
| --- | ---: | ---: | ---: | ---: |
| shoulder | 8.10° / 19.88° / 2.91° | 8.10° / 18.99° / 0.00° | 19.94° / 28.11° / 12.95° | 19.58° / 19.59° / 0.00° |
| upper arm | 0.00° / 5.40° / 4.21° | 0.00° / 2.96° / 0.04° | 3.40° / 9.83° / 9.54° | 0.29° / 0.30° / 0.00° |
| lower arm | 0.10° / 7.99° / 3.94° | 0.10° / 6.91° / 0.01° | 3.30° / 10.09° / 9.79° | 0.29° / 0.29° / 0.00° |
| hand | 6.43° / 11.19° / 3.85° | 6.43° / 10.36° / 0.00° | — | — |
| neck | 3.92° / 15.41° / 3.94° | 3.29° / 12.14° / 0.00° | 2.99° / 17.25° / 13.47° | 1.48° / 7.00° / 0.00° |
| upper leg | 2.46° / 5.00° / 0.00° | unchanged | 2.11° / 3.81° / 0.00° | unchanged |
| lower leg | 2.58° / 4.08° / 0.00° | unchanged | 4.53° / 6.48° / 0.00° | unchanged |

On the two models with `上半身3`, all three runs are identical: every
joint the clips drive is bound, so there is nothing to fold.

## Findings

**The shipped option is the measured fold.** Every arm, hand and neck spread
on the 15 models goes to at most 0.04°, the figure on the two models with
`上半身3`. What is left is the two skeletons' rest shapes, as before. The
legs do not move, because nothing below the chest changes.

**It is inert where nothing is missing.** With `上半身3`, the three runs
agree to the last digit. So a consumer can set the option for every PMX
without testing for `上半身3`, as §10.9 says.

**The diagnostics keep the fact.** `MOTION_RETARGET_UNBOUND_DRIVEN_BONE`
still names `upperChest` once per clip, on the same 128 pairs as without the
fold. Only its detail grows: "its rest-relative rotation was folded into
chest". No other diagnostic changes.

## Tests behind it

`mmdSkeletonAdapter_generic_clips` holds the path in CI without any of these
files. On its generated stages, which have identity rests and rotated rests,
and its generated PMX humanoids:

- With `上半身3`, every segment is within 0.01° at rest and in motion,
  with the fold and without it, and with no diagnostics.
- Without `上半身3` and without the fold, the test still pins the default:
  the segments above the chest miss the upper chest's 15°, and `upperChest`
  is the only unbound driven joint.
- Without `上半身3` and with the fold, every segment is within 0.01° in
  motion, and `upperChest` is still the only unbound driven joint.

## Limits

Those of the generic-clips report: roll about each bone and root
translation were not measured. The fold turns the whole subtree of
`上半身2` by the upper chest's rotation. That is exact for the directions
measured here. It does not promise segment positions where the two chains
differ in length
([its §4.2](https://github.com/animu-sphere/usd-motion-plugins/blob/v0.5.3/docs/design/RETARGETING_POLICY.md#42-opt-in-folding-of-unbound-intermediate-rotations)).
