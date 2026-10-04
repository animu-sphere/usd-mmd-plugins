# Phase 8 recorded connector captures onto a PMX (2026-10-04)

Dated evidence from real runs; append-only
([contributing/documentation.md](../contributing/documentation.md)).

## Question

The roadmap's Phase 8 asks for `motion-connectors → MotionPose → shared
retarget → PMX` to be verified first from deterministic recorded captures,
with no live device, no network and no protocol dependency in this repository
([current.md](../roadmap/current.md#avatar-runtime-composition)).
`motion-connectors` v0.1.0 was released on 2026-10-04. Its connectors replay a
packet capture through the same `PushDatagram` → `Poll` path a live socket
feeds. Does the `MotionPose` stream such a replay delivers pose a PMX through
`mmdSkeletonAdapter`, with the recipe of
[MOTION_CONTRACT.md §10.9](../design/MOTION_CONTRACT.md#109-a-generic-clip-onto-a-pmx)?

## What was run

A scratch program, outside this repository, linked against this repository's
install tree at `df40556`, the digest-pinned `usd-motion-plugins` v0.5.3
packages, and the `motionConnectorCore`, `motionConnectorMocopi` and
`motionConnectorTransport` v0.1.0 release artifacts for Windows. For each
capture it:

1. reads the capture and pushes every datagram, at its recorded receive time,
   into a `MocopiConnector` through `IMotionConnector`, and polls every
   `MotionFrame`. Each frame's actor pose becomes one `MotionClip` sample,
   as the connector delivered it;
2. decodes the session's first skeleton packet beside the connector, with
   `MakeSkeletonMap`, for the device's rest. The connector does not hand its
   rest out, and the library states it as values for a consumer to compose
   into a `SourceRestPose`. The program builds the source skeleton from those
   values on the shared joint paths, then calls `BuildSkeletonDescriptor` and
   `BuildSourceRestPose`;
3. retargets onto each PMX with §10.9's step 3: `requiredJoints`,
   `targetRest`, and `foldUnboundIntermediateRotations`. It also runs without
   the fold, and without the target rest.

Ground truth and figures are those of the
[generic-clips report](2026-10-04-phase8-generic-clips-onto-pmx.md): the
source posed on its own skeleton by its own clip, and per bone segment, every
5th sample, the median over pairs of each pair's median angle, the largest
angle, and the spread.

The inputs:

- **Recorded**: the five mocopi device sessions of 2026-08-15 that
  `motion-connectors`' `recorded/manifest.json` describes. Each file's SHA-256
  matched its manifest row. The bytes are a real person's motion, are not in
  either repository, and are not redistributed. Together they are 7,690
  frames: a still stand, a left-arm raise, a head turn with an arm raise and a
  walk, a walk, and a session the application restarted mid-recording.
- **Generated**: the nine captures of `motion-connectors`' committed mocopi
  corpus, which are synthetic and redistributable.
- **PMX**: the 17 local characters of the earlier reports. As before, the
  1.2 MB prop is run but not counted, since it has no hips. Two of the 17 have
  `上半身3` and 15 do not.

The runs were on Windows 11 with OpenUSD 26.08. A second run of two sessions
onto three models printed byte-identical output.

## Results

**The connector delivers what its manifest recorded.** Frames per session
equal each row's `framesDelivered`: 2,190, 1,757, 1,056, 1,194 and 1,493. The
connector's diagnostics are the rows' `expectedDiagnostics`:
`MOCOPI_SOURCE_RESTARTED` and `MOCOPI_FRAME_INCOMPLETE` on the restarted
session, `MOCOPI_FRAME_INCOMPLETE` on the still stand, and none on the
others. Every pose carries the rig's 22 shared joints, `upperChest` among
them. The device's rest states identity rotations, with the whole rest in the
offsets: a level-armed T-pose.

Recorded sessions, left side. In the recipe columns the right side is the
same to the hundredth; without the fold its largest angles differ.

| Segment | With `上半身3` (2 models): median / max / spread | Without (15): recipe | Without, no fold | Without, no target rest: median / max |
| --- | ---: | ---: | ---: | ---: |
| shoulder | 19.58° / 19.58° / 0.00° | 19.58° / 19.58° / 0.00° | 20.20° / 27.61° / 13.06° | 33.93° / 45.20° |
| upper arm | 0.29° / 0.30° / 0.00° | 0.29° / 0.30° / 0.00° | 2.37° / 17.86° / 17.23° | 40.46° / 42.53° |
| lower arm | 0.29° / 0.30° / 0.00° | 0.29° / 0.30° / 0.00° | 2.28° / 17.81° / 17.26° | 40.00° / 42.52° |
| neck | 1.70° / 1.84° / 0.00° | 1.48° / 7.00° / 0.00° | 3.53° / 24.96° / 19.51° | 1.48° / 7.00° |
| upper leg | 1.70° / 2.11° / 0.00° | 2.11° / 3.81° / 0.00° | unchanged | unchanged |
| lower leg | 4.48° / 5.91° / 0.00° | 4.53° / 6.48° / 0.00° | unchanged | unchanged |

With `上半身3`, the run without the fold is the recipe's, to the last digit.
Without the target rest, its arms are about 39° low.

Generated corpus: five captures carry a rig, 20 frames in all. Every arm and
shoulder segment is within 0.02° on all 17 models, and every spread is 0.00°.
The other four deliver no pose, by design: one has no skeleton packet, one
declares an 11-bone rig, one refuses its only skeleton packet, and one holds no
walkable container. The probe therefore had no rest to build for them.

Retarget diagnostics on the 17 characters: `MOTION_RETARGET_UNBOUND_DRIVEN_BONE`
names `upperChest` once per pair on the 15 without `上半身3`, with the fold as
its receiver, and nothing else is reported.

## Findings

**A recorded live stream reaches a PMX exactly.** With §10.9's recipe,
every segment's spread is 0.00° on all 17 characters. That holds for every
recorded session, including the restarted one, and for every generated
capture with a rig. A connector stream needs nothing from this repository
that a semantic motion stage does not. The source skeleton comes from the
producer's stated rest, and the adapter's map, required joints and target
rest do the rest.

**The live path lands where the file path landed.** The constant residuals
match those the
[generic-clips report](2026-10-04-phase8-generic-clips-onto-pmx.md) measured
from the mocopi BVH in `usd-motion-plugins`' corpus, within 0.01°, on both
groups of models. Those residuals are the shoulder 19.58°,
the arms 0.29°, the folded neck 1.48° / 7.00°, and the legs. The application's
UDP rig and its BVH export rest the same, as `motion-connectors` measured,
and the shared retarget turns them onto a PMX the same way.

**The fold matters more live.** Without it, the 15 models without `上半身3`
miss the upper chest's motion. That is up to 17.9° on the arms and 25.0° on
the neck, against 10.1° and 17.3° from the BVH clip. Both the fold and the target rest are what §10.9 already
asks of a consumer onto a PMX. Nothing here changes it.

**A restart is the connector's to report, and it stays there.** The
restarted session's clip has one backwards timestamp, where the new session
begins. The connector reported it as `MOCOPI_SOURCE_RESTARTED`. The retarget
is per sample, so every sample on both sides of the restart lands as the
others do. Splitting or re-timing the stream is the stream layer's job, not
this repository's.

## What stays outside this repository

Nothing here links `motion-connectors`, and nothing was added. The edge stays
forbidden ([WORKSPACE.md §2.2](../architecture/WORKSPACE.md#22-forbidden-edges)):
the consumer composes the connector and the adapter. No CI test was added.
`mmdSkeletonAdapter_generic_clips` already holds the retarget half on
generated rests, and the connector half is `motion-connectors`' corpus
replay. The recorded sessions cannot be fixtures
([DESIGN_POLICY.md §13](../design/DESIGN_POLICY.md#13-testing-policy)).

## Limits

- **mocopi only.** No VMC sender has been recorded, so VMC rests on
  generated captures in `motion-connectors`. VRChat OSC delivers tracker
  observations, not a body pose, until an operator's assignment drives the
  tracking solve. Neither was run.
- Roll about each bone and root translation were not measured, as in the
  earlier reports. The connector carries the hips position as root motion.
  Where it lands on a PMX is `RootMotionOptions`, the consumer's choice.
- The connector's rest was decoded beside it, not asked of it. A consumer
  that wants the rest from the connector needs `motion-connectors` to expose
  it. That is its decision, not this repository's.
- Two-segment directions are not reported: shoulder to shoulder, and spine
  to neck. They sum segments of different lengths, so they vary with the pose
  on every model, with `上半身3` or without it, as the fold report's limits
  say.
