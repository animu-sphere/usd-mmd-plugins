# Phase 8 generic clips onto a PMX (2026-10-04)

Dated evidence from real runs; append-only
([contributing/documentation.md](../contributing/documentation.md)).

## Question

The roadmap's Phase 8 asks for generic motion, not only VMD, to reach a PMX
through `mmdSkeletonAdapter`, with no VMD evaluated on the way
([current.md](../roadmap/current.md#avatar-runtime-composition)). Two
producers write such motion today, both as a semantic motion stage:
`usd-vrm-plugins`' `usdVrmaFileFormat` from a `.vrma`, and
`usd-motion-plugins`' `motion_convert` from a BVH and a producer profile.
Does a clip from either, read by `motionUsd` and retargeted with the adapter's
map and target rest, pose a PMX as its source moves? Where it does not, why?

## What was run

The recipe a consumer follows, in a scratch program against this branch's
build and the digest-pinned `usd-motion-plugins` v0.5.2 packages:

1. `motionUsd::ReadMotionStage` reads the stage into a `MotionClip`, and its
   skeleton's tokens and rest transforms.
2. `BuildSkeletonDescriptor` and `BuildSourceRestPose` take the clip's rest
   from that skeleton.
3. `mmd::skeleton::Adapt` gives the PMX target's descriptor, map, required
   joints and target rest, and `PoseRetargeter` retargets with all of them.

Ground truth is the source posed on its own skeleton by its own clip. Each
figure is the angle between a source segment and the same segment of the
PMX, every 5th sample; the median over pairs of each pair's median, and the
largest angle of any sample. **Spread** is the largest difference, in any one
pair, between that pair's largest and smallest angle. A spread of zero says
the target turns exactly as the source does, and the angle left is the
difference between the two skeletons' rests.

The inputs are local files. None is a fixture, and none is committed or
redistributed
([DESIGN_POLICY.md §13](../design/DESIGN_POLICY.md#13-testing-policy)):

- **VRMA**: the seven clips of a distributed VRMA 1.0 motion pack, opened
  through the `usdVrmaFileFormat` bundle of `usd-vrm-plugins` v0.10.0 and
  written out as `.usda`. Each is standard UsdSkel over the shared joint
  tokens, with no `customData.motion`. One clip's skeleton states rotated
  rests: its hips turned 120° about (1, 1, 1), its upper chest and neck
  rolled 35° and 47°.
- **BVH**: the redistributable mocopi recording in `usd-motion-plugins`'
  `motionBvh` corpus (853 frames at 50 Hz), converted by `motion_convert`
  v0.5.2 with the `mocopi-mobile-bvh-default-v1` profile. A local copy of the
  same recording converted identically.
- **PMX**: the 17 character models over 1 MB of the earlier reports; the
  1.2 MB prop is skipped again, since it has no hips. Two of the 17 have
  `上半身3`, so `upperChest` binds on them; 15 do not.

The runs were on Windows 11 with OpenUSD 26.08.

## Results

Left side shown; the right side agrees to within 1.4°. "Folded" is an
experiment described below, not shipped behavior.

VRMA, 7 clips:

| Segment | With `上半身3` (2 models): median / max / spread | Without (15): median / max / spread | Without, folded | No target rest: median / max |
| --- | ---: | ---: | ---: | ---: |
| shoulder | 8.10° / 18.99° / 0.00° | 8.10° / 19.88° / 2.91° | 8.10° / 18.99° / 0.00° | 9.60° / 22.26° |
| upper arm | 0.00° / 2.96° / 0.03° | 0.00° / 5.40° / 4.21° | 0.00° / 2.96° / 0.04° | 40.16° / 45.22° |
| lower arm | 0.10° / 6.91° / 0.01° | 0.10° / 7.99° / 3.94° | 0.10° / 6.91° / 0.01° | 40.39° / 46.70° |
| hand | 4.78° / 8.04° / 0.00° | 6.43° / 11.19° / 3.85° | 6.43° / 10.36° / 0.00° | 43.22° / 51.11° |
| neck | 1.73° / 6.69° / 0.00° | 3.92° / 15.41° / 3.94° | 3.29° / 12.14° / 0.00° | 3.36° / 15.41° |
| upper leg | 2.46° / 3.39° / 0.00° | 2.46° / 5.00° / 0.00° | 2.46° / 5.00° / 0.00° | 2.46° / 5.00° |
| lower leg | 3.28° / 4.08° / 0.00° | 2.58° / 4.08° / 0.00° | 2.58° / 4.08° / 0.00° | 2.58° / 4.08° |

mocopi BVH, one clip (it has no fingers, so no hand segment):

| Segment | With `上半身3` | Without | Without, folded | No target rest |
| --- | ---: | ---: | ---: | ---: |
| shoulder | 19.58° / 19.59° / 0.00° | 19.94° / 28.11° / 12.95° | 19.58° / 19.59° / 0.00° | 33.92° / 49.31° |
| upper arm | 0.29° / 0.29° / 0.00° | 3.40° / 9.83° / 9.54° | 0.29° / 0.30° / 0.00° | 40.11° / 47.70° |
| lower arm | 0.29° / 0.29° / 0.00° | 3.30° / 10.09° / 9.79° | 0.29° / 0.30° / 0.00° | 39.10° / 48.26° |
| neck | 1.70° / 1.84° / 0.00° | 2.99° / 17.25° / 13.47° | 1.48° / 7.00° / 0.00° | 2.99° / 17.25° |
| upper leg | 1.70° / 2.11° / 0.00° | 2.11° / 3.81° / 0.00° | 2.11° / 3.81° / 0.00° | 2.11° / 3.81° |
| lower leg | 4.48° / 5.91° / 0.00° | 4.53° / 6.48° / 0.00° | 4.53° / 6.48° / 0.00° | 4.53° / 6.48° |

## Findings

**A generic clip reaches a PMX exactly where the PMX binds every joint the
clip drives.** On the two models with `上半身3`, every segment's spread is at
most 0.03°, for every clip and for the clip with rotated rests. What is left
is constant and is the two skeletons' shapes: the arms 0.00–0.29° in the
median, the legs 1.7–4.5° as between any two models, and the shoulder 8.1°
from VRMA and 19.6° from mocopi. The adapter aims the PMX shoulder onto the
shared T-pose direction
([MOTION_CONTRACT.md §12.6](../design/MOTION_CONTRACT.md#126-the-arm-chains-reference-rest)),
while those sources' collarbones rest that far above it. The arms below are
unaffected. Retarget diagnostics on these pairs are empty.

**Both or neither holds for generic sources.** Without the target rest,
every arm segment is about 40° low, the model's own arm angle, as §12.6 says
of any level-arm source. Both producers' rests are level-armed.

**A PMX without `上半身3` loses the upper chest's motion.** Both producers
drive `upperChest`. Where the PMX binds none, the shared retarget reports it
as an unbound driven joint and drops its rotation. It does not fold the
rotation into a neighbour
([its RETARGETING_POLICY.md §4.1](https://github.com/animu-sphere/usd-motion-plugins/blob/main/docs/design/RETARGETING_POLICY.md#41-partial-skeletons-carried-from-usd-vrm-plugins-v090),
case 6). Everything above the chest then misses it: spreads of up to 4.2° on
the VRMA arms and 9.8° on the mocopi arms, and 13.5° on the mocopi neck.
Clips 1–4 of the pack keep the upper chest still and lose nothing; clips 5–7
move it. That is 15 of the 17 local characters.

**Folding the upper chest into the chest removes it, if the fold respects
the rests.** In the experiment, each sample's `chest` rotation became
`Qc · Quc · S⁻¹`, where `S` is the upper chest's local rest, and
`upperChest` was cleared. The chest then carries the upper chest's world
rotation away from its rest, and every joint above lands as on a model with
`上半身3`: every spread is at most 0.04°. The naive `Qc · Quc` is right only
where the upper chest's rest rotation is identity. It made the neck 38° off
on the clip with rotated rests.

The fold is a generic retarget rule: which joint a target lacks, and how its
motion carries into the joint that replaces it, say nothing about MMD. It
belongs to the shared retarget, which states that such a fold "would be a
contract change with its own parity evidence". This report is that evidence
from this side. Until it lands, this repository adds no private fold
([WORKSPACE.md §7](../architecture/WORKSPACE.md#7-invariants), invariant 9),
and the question is MOT-O13
([MOTION_CONTRACT.md §9](../design/MOTION_CONTRACT.md#9-open-questions)).

## Tests behind it

`mmdSkeletonAdapter_generic_clips` holds the recipe in CI without any of
these files. It authors a semantic motion stage the way the two producers
do: `/Animation/HumanoidSkeleton` over the shared tokens, one bound
`UsdSkelAnimation`, no `customData.motion`. It does so once with identity
rests and once with rotated rests that leave every joint in place. The clip
yaws the hips, rolls the upper chest, nods the neck and raises an arm.
`motionUsd` reads it, and it is retargeted onto two generated PMX humanoids
with 40° A-pose arms. The truth is the stage's own animation values,
composed with Gf alone.

- With `上半身3`: every arm, neck and leg segment within 0.01° at rest and in
  motion, for both rests, with no diagnostics.
- Without `上半身3`: exact at rest. In motion, `upperChest` is the only
  unbound driven joint, and the segments above it miss up to the upper
  chest's 15°. When the shared core folds the joint, this test changes with
  the contract.

The test links `mmdSkeletonAdapter` and `motionUsd` only. Nothing that
evaluates a VMD is on its link line.

## Limits

Roll about each bone is not measured, as before: segment directions cannot
see it. Root translation was not compared. Both producers' hips start at
their own height, and `RootMotionOptions` scales the delta, which is the
consumer's choice. Expressions were not carried: VRMA's expression tracks
are `usd-vrm-plugins`' to read, and the shared core has no common expression
semantic yet
([MOTION_CONTRACT.md §10.7](../design/MOTION_CONTRACT.md#107-morphs-as-channels)).
Only one BVH producer has a profile. Live capture through
`motion-connectors` is a separate roadmap item.
