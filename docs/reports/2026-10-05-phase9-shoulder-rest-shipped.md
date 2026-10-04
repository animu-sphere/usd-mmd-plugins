# Phase 9 the unaimed shoulder, as shipped (2026-10-05)

Dated evidence from real runs; append-only
([contributing/documentation.md](../contributing/documentation.md)).

## Question

MOT-O14 left the shoulder out of
[MOTION_CONTRACT.md §12.6](../design/MOTION_CONTRACT.md#126-the-arm-chains-reference-rest)'s
aim. The [measurement behind it](2026-10-04-phase9-shoulder-rest.md) kept the
shipped upper-arm world rest and only unset the shoulder's slot. The
adapter now aims the upper arm from the model's own shoulder rest instead.
Does the shipped adapter give the measured shoulder, and do the arms stay
where they were?

## What was run

The scratch programs of the earlier reports, relinked against this branch's
install tree and the digest-pinned `usd-motion-plugins` v0.5.3 packages:

- **Shoulder elevation**: the mocopi device session with a still stand,
  replayed through `motion-connectors` v0.1.0's `MocopiConnector`. The
  elevation of `肩` → `腕` was measured at sample 300, onto the 17 local
  characters, as before.
- **Connector captures**: the five recorded mocopi sessions and the five
  generated captures with a rig, onto the 17 characters. The figures are
  those of the
  [generic-clips report](2026-10-04-phase8-generic-clips-onto-pmx.md): the
  median, the largest angle, and the spread, every 5th sample.
- **Generic clips**: the seven VRMA clips and the mocopi BVH of the
  [fold report](2026-10-04-phase8-upper-chest-fold.md), with the fold set,
  compared row by row with that report's shipped run.

The runs were on Windows 11 with OpenUSD 26.08. None of the inputs is
committed or redistributed
([DESIGN_POLICY.md §13](../design/DESIGN_POLICY.md#13-testing-policy)).

## Results

**The standing shoulder is where the model rests it.** Above the model's own
slope, the shoulder sits a median 0.5° on the left, from 0.0° to 1.5°. On the
right it sits −3.1°, from −3.7° to −1.9°. These are the measurement's figures
to the tenth of a degree, and they are the source clavicle's own departure
from its rest: +1.6° and −1.9°. As shipped before, the medians were 17.4°
and 13.7°.

**Every segment still turns as the source's does.** On the connector
captures, every segment's spread is 0.00° on all 17 characters. That
includes the shoulder, whose angle to the source is now constant: the two
rigs' rest slopes. The upper and lower arm figures did not change in the
hundredths.

| Segment | mocopi captures, without `上半身3`: median / max / spread | VRMA, without `上半身3` | VRMA, with |
| --- | ---: | ---: | ---: |
| shoulder | 33.93° / 45.20° / 0.00° | 10.31° / 22.26° / 0.00° | 9.84° / 17.81° / 0.00° |
| upper arm | 0.29° / 0.30° / 0.00° | 0.00° / 2.96° / 0.03° | 0.00° / 2.96° / 0.04° |
| lower arm | 0.29° / 0.30° / 0.00° | 0.10° / 6.91° / 0.01° | 0.10° / 6.91° / 0.01° |

Against the fold report's shipped run, over every clip and character:

- the upper arm moved by at most 0.006° and the lower arm by 0.004°;
- the hand, measured to its middle finger, moved by at most 0.09°;
- the neck and legs did not move.

The shoulder's angle to the source went from 8.10° to 10.31° in the median
from VRMA, and from 19.58° to 33.93° from the mocopi BVH. Neither was an
error before, and neither is now: each is how far the PMX's shoulder slope
lies from the source's.

The connector diagnostics, frame counts and retarget diagnostics are those
of the earlier runs.

## Findings

**The shipped rule is the measured one.** The shoulder rests as the model
does, and a source clavicle near its rest leaves it there. The arms keep
their aim.

**Only roll moved, and only a little.** The upper arm is now aimed from the
model's own shoulder rest, not from an aimed one. The shortest rotation then
reaches the same direction with a different roll. The arm segments cannot
see that, and the hand's line to its middle finger moves by at most 0.09°.

**The target rest no longer touches the shoulder.** So, for the shoulder,
"no target rest" and the recipe agree, and a PMX target's shoulder answers
to its own rest whatever the source.

## Tests behind it

`mmdSkeletonAdapter_unit` now pins the shoulder unaimed. As a source, its
local and world rests are identity. As a target, its slot is unset and its
world rest is identity, like `肩C`'s. In the both-ways retarget, each
target's shoulders keep their own rest: level on the level rig, and the
second PMX's 5° slope, where the source slopes 10°. Every arm segment stays
within 0.01° of the source.

`mmdSkeletonAdapter_generic_clips` expects the PMX's 10° shoulder to keep
its 10° from the source's level shoulder, at rest and in motion. Every other
segment is unchanged.

## Limits

- Roll is not measured, as before.
- A PMX as the source onto a non-MMD rig is held only by the synthetic
  tests. No VMD-derived clip onto a level-clavicle rig was rerun.
