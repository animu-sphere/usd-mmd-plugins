# Phase 9 A-pose rest against a level-arm rest (2026-09-25)

> Later: MOT-O12 was resolved the same day by role-table version 2 ([report](2026-09-25-phase9-upper-chest.md)).

Dated evidence from real runs; append-only
([contributing/documentation.md](../contributing/documentation.md)).

## Question

MOT-O10: every MMD bone rests at identity rotation, and in that rest a
character's arms hang 37–42° below horizontal
([roles report](2026-09-19-phase9-roles-and-root.md)). A VRM's identity rest
has level arms. Should `mmdSkeletonAdapter` state a `SourceRestPose` measured
from the rest bone directions, and if so, against which reference directions?

## What was run

Seventeen character models from the local corpus: every PMX over 1 MB that
resolves the required roles. Two pairs among them are variants of one
character each, and one 1.2 MB prop is skipped. Two dance
motions were used: A, 1.6 MB, sampled every 5th frame into 563 samples; and C,
11 MB, keys `上半身2`, sampled every 5th frame into 241 samples. All of these
are distributed files. None is a fixture, and none is committed or
redistributed
([DESIGN_POLICY.md §13](../design/DESIGN_POLICY.md#13-testing-policy)). The
runs were on Windows 11. No library of this repository was changed for them.

A scratch program linked the installed `mmdControl`, `mmdSkeletonAdapter` and
`mmdMotionAdapter`, and the digest-pinned `motionRetarget` v0.5.0. For each
model it:

1. bound each motion, evaluated it with `mmdControl`, and built the clip with
   `mmdMotionAdapter::BuildClip`, as the adapters do today;
2. took the **ground truth** from MMD's own evaluation: the world direction
   of 17 segments (spine, chest, neck, and each side's shoulder, upper arm,
   lower arm, hand, upper leg, lower leg and foot). Each direction is the line
   from one mapped joint's evaluated position to the next;
3. built the **stated rest** by the shared core's `t-pose` construction
   (`motionSource`'s, copied into the scratch program only). Each role's world rest is the shortest rotation that turns
   its bone onto the T-pose direction: arms lateral, legs down, spine up, feet
   forward. The construction ran two ways: on the whole body, and on the arm
   chain alone (shoulder, upper arm, lower arm, hand; fingers inherit);
4. retargeted with `PoseRetargeter` onto
   - a **level-arm rig**: the model's own role joints, posed by those aims
     into a T-pose, with identity rest rotations, as a normalized VRM
     rests;
   - **another PMX**: the next model in the list, as its stage states it,
     with identity rests. It was also retargeted with the aims stated as that
     target's rests, which `motionRetarget` accepts although the stage
     does not state them;
   - the second PMX, **from a level-arm source**: the clip the level-arm rig
     received, carried on with identity source rests, as a VRMA clip would
     arrive;
5. measured, per sample and segment, the angle between the retargeted
   direction and the ground truth.

Each figure below is the median over model pairs of each pair's median angle,
then the largest angle of any sample. Arm figures are the same for both
sides to within 0.4°.

## Onto a level-arm rig

The arm chain, motion A (C agrees to 0.01°):

| Segment | Identity rests (today) | Stated arm-chain rest |
| --- | ---: | ---: |
| shoulder | 17.58° / 30.32° | 0.00° / 0.04° |
| upper arm | 40.16° / 42.42° | 0.00° / 0.04° |
| lower arm | 39.82° / 42.41° | 0.00° / 0.04° |
| hand | 39.74° / 42.38° | 0.00° / 0.04° |

Today every arm segment is off by the model's own rest angle, the difference
MOT-O10 names, and it does not vary with the motion. Stating the arm chain's
rest removes it entirely. The torso and legs are 0.00° either way, because the
rig shares the model's geometry there.

## The whole-body construction is not safe on MMD

Applied to the whole body, the same construction fails in two places:

- **The chest flips.** In both models that have `上半身3`, it lies
  **below** `上半身2`: the line from `上半身2` to `上半身3` points 85° and 88°
  downward. The construction aims the chest along that line, so it turns the
  chest 175° and 178°. Retargeted onto another PMX, the arms and neck from those two
  sources are then off by a median 100–170°, up to 179°.
- **The feet are levelled.** The rest line from `足首` to `足先EX` points
  40–66° below horizontal. Aiming it forward changes the rest by that much
  (a median 48° over the pairs). A level-arm humanoid's foot slopes the same
  way in its identity rest, so this change is not part of the A-pose
  difference.

The legs differ from straight down by a median 2.9° (upper) and 4.6° (lower),
at most 6.5°: small beside the arms. The line from `上半身2` to `首` leans
6–18° from vertical in every model. A level-arm humanoid straightens neither,
so the arm chain alone is where the A-pose differs.

## Onto another PMX

| Segment, motion A | Identity rests (today) | Source rest stated only | Stated on both sides |
| --- | ---: | ---: | ---: |
| shoulder | 7.51° / 22.28° | 17.58° / 30.32° | 0.00° / 0.04° |
| upper arm | 2.45° / 5.26° | 40.16° / 42.42° | 0.00° / 0.04° |
| lower arm | 1.82° / 5.62° | 39.82° / 42.41° | 0.00° / 0.03° |
| hand | 2.68° / 10.04° | 39.50° / 48.10° | 3.55° / 8.39° |

| Segment, from a level-arm source, motion A | Identity target rests (today) | Target rest stated |
| --- | ---: | ---: |
| upper arm | 40.16° / 42.42° | 0.00° / 0.04° |
| lower arm | 39.82° / 42.41° | 0.00° / 0.04° |

Today, PMX to PMX is off only by the difference between two models' rests. A
source rest stated **alone** makes it as wrong as a level-arm target is today.
The same 40° already reaches a PMX target from any source with a level-arm
rest. Stating the rest on **both** sides makes both directions exact. The
hand's remaining 3.55° is the hand-to-middle-finger line, which differs
between models' hands. It is 2.7–3.0° with identity rests.

The stage cannot state a target's aim. `/Asset/skel/Skeleton`'s rests are
identity, and the adapter's descriptor must equal them
([report](2026-09-22-phase9-motion-acceptance.md)). The "stated" target above
exists only in the scratch program. Joints the program left unmapped kept
identity. A mapped joint the clip does not drive would take the aim as its
pose.

## `上半身3` is out of the vocabulary's order

In both models that have `上半身3`, the hierarchy is `上半身` → `上半身3` →
`上半身2` → `首`, with `首` and the shoulders' `肩P` under `上半身2`.
[MOTION_CONTRACT.md §12.2](../design/MOTION_CONTRACT.md#122-the-table)
maps `上半身2` to `chest` and `上半身3` to `upperChest`, so the vocabulary's
chest → upperChest runs against the model's own chain. For a motion that
keys `上半身2`, `neck` then carries the chest's rotation a second time. It is
cancelled only on a target that binds `upperChest` too. From those two
sources onto targets without `上半身3`, motion C leaves the arms a median 6–8°
off, 18–19° at the 95th percentile and 26° at most, with **today's** identity
rests. Motion A, which keys only `上半身`, is unaffected. This is independent
of MOT-O10. It is opened as MOT-O12.

## Result and limits

MOT-O10 is answered in part:

- **What to state.** A rest aimed from the rest bone directions of the arm
  chain alone, onto the lateral axis: the shared core's T-pose direction for
  those four roles. It is exact on a level-arm rig. The same construction on
  the whole body is wrong for MMD.
- **What it waits for.** The target half. A stated source rest is correct
  only if a PMX target states the same rest. Without it, PMX to PMX gets
  worse by the full rest angle. A target rest that differs from the bind rest
  is not something `motionRetarget` v0.5.0 can take from a stage. Until it
  can, the source half is not adopted. The T-pose directions are private to
  `motionSource`, so adopting it here would also copy them (WORKSPACE.md
  invariant 9).

Not measured: roll about the bone. The shortest rotation leaves one, and
segment directions cannot see it. No clip was compared with a real VRM
model's rest, only with the level-arm rig built from each model's own
proportions.
