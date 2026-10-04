# Phase 8 finishing a retargeted pose, as shipped (2026-10-05)

Dated evidence from real runs; append-only
([contributing/documentation.md](../contributing/documentation.md)).

## Question

The [measurement behind MOT-O15](2026-10-05-phase8-pmx-pose-finish.md)
finished each pose in a scratch program. It used a hand-written roll split,
and `mmdControl::Evaluator::Evaluate` over a motion keyed at the pose. The
finish now ships as
[MOTION_CONTRACT.md §10.10](../design/MOTION_CONTRACT.md#1010-finishing-a-retargeted-pose-on-a-pmx)
says:

- `mmdSkeletonAdapter`'s `CarryArmRoll`, with its `armTwists` and
  `heldJoints`;
- `mmdControl`'s `Evaluator::Complete` (§11.9).

Does the shipped finish give what was measured, and is it exact where §10.10
says it is?

## What was run

The scratch program of the earlier report, relinked against this branch's
install tree. For every sample, it retargeted with §10.9's recipe, then
called `CarryArmRoll`, then `Complete` with the adapter's `heldJoints`. The
source was the mocopi device session with a walk, replayed through
`motion-connectors` v0.1.0's `MocopiConnector`. The targets were the 17 local
characters. It measured three things:

- **Below the twist bones**: how far each held joint below a twist bone
  moved, and turned, between the retarget's pose and the carried one. A
  joint whose own roll was carried, such as `ひじ`, was checked for
  position only, since its rotation changes by design.
- **Held joints**: whether `Complete` returned every held joint's given
  transform unchanged.
- **IK**: each chain's effector-to-goal distance before and after `Complete`.

It ran on Windows 11 with OpenUSD 26.08. None of the inputs is committed or
redistributed
([DESIGN_POLICY.md §13](../design/DESIGN_POLICY.md#13-testing-policy)).

## Results

**Every character has all four twist bones.** `armTwists` found `腕捩` and
`手捩` on both sides of each of the 17.

**The carry is exact.** Below the twist bones, no held joint moved by more
than 0.0003 mm or turned by more than 0.00002°, which is float rounding.
The first build composed the roll onto the twist bone alone. On models whose
`腕捩` sits off the arm's bone line, that moved the joints below by up to
8.3 mm. Turning the twist bone's offset with the roll removed it. §10.10
step 1 now says so, and a synthetic test holds a twist bone 2 cm off the
line.

**Held joints are returned as given.** On all 17, `Complete` changed no held
joint, compared bit for bit, in any sample.

**IK is as measured.** On the three characters with arm IK helpers, every
such chain went from 9.7–580 mm to within 1.3 mm, and all but one to within
0.002 mm. That matches the earlier report's figures. No other chain
changed.

**It looks as measured.** Rendered at three frames, the two Miku models'
sleeves follow their arms as in the experiment. A character without arm
helpers looks as it did before the finish.

## Tests behind it

- `mmdControl_unit`:
  - a held joint returns its given transform and takes no append, where
    unheld it would take one;
  - a chain is solved clear of held joints, and is off over a held link and
    over a link that feeds a held joint's append (`足` under a held `足D`);
  - with nothing held, `Complete` agrees with `Evaluate` of a motion keyed at
    the pose.
- `mmdSkeletonAdapter_unit`:
  - `armTwists` finds `腕捩` between `腕` and `ひじ` with the bone's direction;
  - `heldJoints` holds the bound joints, the twist bones and their
    ancestors;
  - the carry splits a swing and a roll exactly and leaves `ひじ` and `手首`
    in place;
  - with the twist bone off the line, `腕捩` and everything below keep their
    world transforms.

## Limits

Those of the earlier report: twist bones are found by exact name, physics is
not simulated, and a chain touching a held joint stays off.
