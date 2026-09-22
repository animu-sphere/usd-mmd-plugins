# Motion contract (MMD-specific boundary)

> Status: **binding** since Phase 7 for §2–§8.1 and §8.3: `motionVmd` reads
> VMD as §3–§6 say, `vmd_inspect` reports on it, and `mmdMotionBinding` binds
> a motion to a model as §7 and §8.1 say, each with fixtures. §11 is
> **binding** since Phase 9's `mmdControl`: it evaluates a bound motion as §11
> says, with a synthetic rig behind each rule. §10 and §12 are **binding**
> since `mmdMotionAdapter` and `mmdSkeletonAdapter` adopted the released
> `usd-motion-plugins` v0.5.0 packages, with synthetic adapter tests and an
> installed consumer. This document holds
> only what is specific to MMD motion — VMD's source facts, its text encoding,
> where it meets a PMX model, how MMD's control rig is evaluated, and how the
> result enters the shared motion core. Generic motion concepts (`MotionPose`,
> `MotionClip`, humanoid joint semantics, sampling, retarget, recording, the
> `UsdSkelAnimation` bridge) are owned by `usd-motion-plugins` and consumed
> from it; VMD itself stays here, because it is MMD's format (the
> `usd-motion-plugins` design policy, §26). Section numbers are stable.
>
> Revised 2026-09-17 to that policy: MOT-O3 is superseded (§8.2), `motionVmd`
> is no longer described as leaving this repository (§2), and §10 is new.
> Revised 2026-09-19: §11 is new, and MOT-O7 is resolved by it. Revised again
> the same day: §10 follows what `usd-motion-plugins` has merged — the rig
> types are `motionRetarget`'s, not `motionCore`'s, and a map binds a *target*
> rig — §12 is new, MOT-O5 and MOT-O6 are resolved by it, and MOT-O10 is
> opened. §12 was accepted there and became binding with the adapters.
> Revised again the same day: MOT-O9 is resolved against an independent
> implementation with §11.7 unchanged, and MOT-O11 is opened.
> Revised 2026-09-21: the shared-motion edge is split by responsibility:
> `mmdMotionAdapter` emits evaluated motion, while `mmdSkeletonAdapter` exposes
> the PMX skeleton, rest pose and versioned humanoid mapping.
> Revised again on 2026-09-21: `usd-motion-plugins` v0.5.0 shipped installable
> `motionCore` and `motionRetarget`, and both adapters implemented §10 and §12.

---

## 1. Scope

- **In:** VMD motion files — bone and morph keyframes, and the IK-enable and
  visibility track — and how they bind to a PMX model.
- **Recorded, not mapped:** VMD camera, light and self-shadow tracks. They are
  scene motion, not model motion; they are parsed and kept in the source
  representation, and no USD mapping is defined until a consumer needs one.
- **In, from Phase 9:** evaluating a bound motion over the model's control
  rig, and handing the result to the shared motion core (§8.2, §10).
- **Out:** VMD writing; live motion; motion generation; retargeting itself —
  VRM ↔ MMD or any other — which is `usd-motion-plugins`' (§10.6); recording.

## 2. Components and boundaries

```text
VMD bytes ─Read─→ motionVmd::Document ─BuildMotion─→ motionVmd::Motion
                                                          │
                     mmd::CanonicalDocument ──────────────┤
                                                          ▼
                                   mmdMotionBinding::Bind ─→ BoundMotion
                                                          │
                            (Phase 9, §10)  mmdControl ───┤  IK, append, bone morphs
                                                          ▼
                             mmdMotionAdapter ─→ MotionClip (usd-motion-plugins)
                                                           │
                             retarget, recording, UsdSkelAnimation — usd-motion-plugins

CanonicalDocument ─→ mmdSkeletonAdapter ─→ SkeletonDescriptor / RetargetMap
```

- **`motionVmd`** is a plain library: VMD syntax, CP932 decoding, and the
  source representation. It has **no dependency at all** — no OpenUSD, and
  nothing in this repository.
- It is **model-independent**: never `usdMmdFileFormat`, never `mmdModel`,
  never `mmdPmx`
  ([WORKSPACE.md §2](../architecture/WORKSPACE.md#2-dependency-directions)),
  and never `usd-motion-plugins` either — the source representation is
  MMD's, and only the two adapter components speak the shared core's types
  (§10.1).
  It carries its own diagnostic record and `Result<T>` rather than reach
  `mmdPmx`'s ([WORKSPACE.md §7](../architecture/WORKSPACE.md#7-invariants)).
- **Parsing a VMD never requires a PMX.** `vmd_inspect` reports on a VMD with
  `motionVmd` alone.
- **`mmdMotionBinding`** is where a motion and a model meet: it depends on
  `mmdModel` and `motionVmd`, so neither of them depends on the other (§8).
- **`usdVmdFileFormat`** — opening a `.vmd` directly with `UsdStage::Open` — is
  added only once MOT-O2 is resolved against the standalone motion stage
  `usd-motion-plugins` defines (`/Animation`, with the body as a
  `UsdSkelAnimation`). It would be a thin bundle over `motionVmd`,
  re-implementing no parsing.

**The basis conversion (MOT-O1, resolved in Phase 7).** `motionVmd` converts
nothing: its values are in the source basis and units, as the file states
them, the way `usd-vrm-plugins`' BVH reader leaves a file's convention to the
layer that knows it. The conversion is applied by `mmdMotionBinding`, which
already depends on `mmdModel`, with `mmdModel`'s own functions
([mmdModel/Basis.h](../../libs/mmdModel/include/mmdModel/Basis.h)) — so a
motion and the model it drives are converted by the same code, and no
function is copied. No shared basis library exists: the second consumer that
would need one without a model is a directly opened `.vmd` stage, and
extracting the functions is decided with MOT-O2.

## 3. VMD source facts

The parser's fixtures are the authority on layout. All values little-endian;
`float` is binary32. The record sizes below are fixed except the IK record's.

| Section | Count | Record | Bytes |
| --- | --- | --- | ---: |
| header | — | 30-byte signature `Vocaloid Motion Data 0002` (NUL-padded), 20-byte model name. The older `Vocaloid Motion Data file` signature has a 10-byte model name. | 50 / 40 |
| bone keyframes | `uint32` | bone name (15 bytes), frame (`uint32`), translation (vec3), rotation (quaternion `x, y, z, w`), interpolation (64 bytes) | 111 |
| morph keyframes | `uint32` | morph name (15 bytes), frame (`uint32`), weight (`float`) | 23 |
| camera keyframes | `uint32` | frame, distance, position (vec3), rotation (vec3), interpolation (24 bytes), view angle (`uint32`), orthographic flag (`uint8`) | 61 |
| light keyframes | `uint32` | frame, color (vec3), direction (vec3) | 28 |
| self-shadow keyframes | `uint32` | frame, mode (`uint8`), distance (`float`) | 9 |
| IK / visibility keyframes | `uint32` | frame, visible (`uint8`), IK count (`uint32`), then per IK: bone name (20 bytes), enabled (`uint8`) | 9 + 21 per IK |

The signature is compared up to its first NUL; what follows it in the 30
bytes is ignored.

Older files end after any section; a missing trailing section is an empty
section, not truncation, and the document records how many sections the file
held. Of three distributed motions, one ends after the self-shadow section
and one after the light section
([report](../reports/2026-09-17-phase7-local-motions.md)). A section that
starts and is cut short is truncation. The reading rules of
[PMX_CONTRACT.md §2](PMX_CONTRACT.md#2-reading-rules) — bounded reads, counts
bounded by remaining bytes, no pointer walking — apply unchanged.

| Event | Code | Severity |
| --- | --- | --- |
| neither signature | `MMD_MOTION_BAD_SIGNATURE` | fatal |
| the file ends inside the header, a count, or a record | `MMD_MOTION_TRUNCATED_BUFFER` | fatal |
| a count more records than the remaining bytes can hold, at each record's minimum size | `MMD_MOTION_COUNT_EXCEEDS_BUFFER` | fatal |
| bytes after the IK section | `MMD_MOTION_TRAILING_BYTES` | warning |
| a file that cannot be opened or read in full (`ReadFile`) | `MMD_MOTION_FILE_UNREADABLE` | fatal |

Values are kept as stored: a quaternion is not normalized, a non-finite
float is not replaced, and the camera's orthographic byte and every
interpolation byte are kept whole.

## 4. Text

VMD names are fixed-length **Shift-JIS (CP932)** fields, NUL-padded, and a
name longer than its field is cut — possibly between the two bytes of one
character.

- `motionVmd` decodes CP932 with a mapping table the project owns — never an
  OS API, `iconv`, or the process locale
  ([TEXT_ENCODING_POLICY.md §9](TEXT_ENCODING_POLICY.md#9-pmd-and-vmd)). The
  table,
  [libs/motionVmd/src/Cp932Table.inc](../../libs/motionVmd/src/Cp932Table.inc),
  is generated from Microsoft's CP932 as Python's `cp932` codec implements
  it, by a committed script whose `--check` is a test; it decodes every
  single byte and 9604 two-byte characters, and encodes where several
  sequences decode to one character as Microsoft's encoder does.
- Padding — the first NUL onward — is removed. Some writers fill it with
  leftover memory, so nothing after the NUL is kept.
- A trailing lead byte whose second byte the field cut off is dropped with
  `MMD_TEXT_TRUNCATED_CP932` (info).
- A sequence CP932 does not map leaves the decoded name empty, with
  `MMD_TEXT_INVALID_CP932` (error) and the byte offset where it starts —
  rejected, never repaired, as PMX text is
  ([TEXT_ENCODING_POLICY.md §3](TEXT_ENCODING_POLICY.md#3-decoding-pmx-text)).
- The **field's bytes before its NUL are kept** beside the decoded name,
  because binding compares bytes (§8.1). An undecodable name still binds.

## 5. Time

A VMD frame number is a frame at **30 frames per second**. A USD
representation of VMD time uses `timeCodesPerSecond = 30` and frame numbers as
time codes, so time codes and MMD frames stay equal.

`motionVmd::Read` keeps records in file order. `BuildMotion` groups them into
tracks — one per bone, morph and IK name, by the name's bytes, in the order
each name first appears; one each for visibility, camera, light and
self-shadow — and sorts every track by frame. Duplicate frames on one track
keep the last record in file order, with `MMD_MOTION_DUPLICATE_KEYFRAME`
(warning), once per track.

## 6. Interpolation

Each bone keyframe carries cubic Bézier curves — one each for X, Y and Z
translation and one for rotation — as control points `(x1, y1)` and
`(x2, y2)` on a 0–127 grid, from `(0, 0)` to `(127, 127)`; camera keyframes
carry six. The source representation keeps the control points as stored.
Sampling them onto a fixed time grid is a **bake**, a separate step with an
explicit sample rate, never an implicit side effect of reading.

**Bone keyframes.** The 64 bytes are four rows of 16, each row the one
before shifted left by one byte. In row 0, for channel `c` = X, Y, Z,
rotation: `x1 = b[c]`, `y1 = b[4 + c]`, `x2 = b[8 + c]`, `y2 = b[12 + c]`.
Later MMD versions overwrite `b[2]` and `b[3]` with a physics toggle — in one
distributed motion, 99 683 of 101 652 bone keyframes hold `(99, 15)` there
while row 1 still holds the curves, and in another every row agrees with
row 0 ([report](../reports/2026-09-17-phase7-local-motions.md)) — so the
`x1` of Z and of rotation are read from row 1, `b[17]` and `b[18]`. What the
toggle means is not interpreted; the 64 bytes are kept.

**Camera keyframes.** For channel `c` = X, Y, Z, rotation, distance, view
angle: `x1 = b[4c]`, `x2 = b[4c + 1]`, `y1 = b[4c + 2]`, `y2 = b[4c + 3]`.

## 7. Coordinates

VMD values are in the PMX source basis and MMD units. Binding converts them
with the same functions as the model
([STAGE_CONTRACT.md §6](STAGE_CONTRACT.md#6-coordinate-conversion), MOT-O1):
translations as displacements (`× s`, Z negated), rotations as quaternions
(`(x, y, z, w) → (−x, −y, z, w)`). A bone keyframe's translation is relative to
the bone's rest position and its rotation relative to the bone's world-aligned
rest orientation, which is why the model's joints have identity rest rotations
([STAGE_CONTRACT.md §9.2](STAGE_CONTRACT.md#92-the-skeleton-prim)). The curves
are unchanged: a mirror negates a value, never the progress a curve
describes. Camera and light values are not converted (§1).

## 8. Binding a VMD to a PMX model

A VMD names bones and morphs; it does not index them. Binding is a separate
operation — `mmd::binding::Bind(const motionVmd::Motion&, const
mmd::CanonicalDocument&)` in `mmdMotionBinding` — that takes a motion and a
model and produces a model-specific motion, with its own diagnostics: bone
tracks addressed to canonical joints, morph tracks to canonical morphs, IK
tracks to the joints they name, and the visibility track, every value in the
USD basis. It never fails.

### 8.1 Name matching

MMD matches by comparing bytes: a PMX name is encoded to CP932 and cut to the
VMD field width — 15 bytes for a bone or morph, 20 for an IK state — and
compared with the field's bytes before their NUL. Binding does the same —
matching on decoded Unicode strings would miss every name longer than the
field. Matching is always on **source names**, never on stable identifiers
([TEXT_ENCODING_POLICY.md §5](TEXT_ENCODING_POLICY.md#5-identity-versus-display)).

- A VMD bone or IK name with no matching model bone is dropped with
  `MMD_MOTION_UNMATCHED_BONE` (info), once per name and section.
- A VMD morph name with no matching model morph is dropped with
  `MMD_MOTION_UNMATCHED_MORPH` (info), once per name.
- Two model elements whose truncated CP932 names are equal make that VMD name
  ambiguous: the lower source index wins, with `MMD_MOTION_AMBIGUOUS_NAME`
  (warning).
- A model name that cannot be encoded in CP932 can never match; that is
  reported once per table, with `MMD_MOTION_UNENCODABLE_NAME` (info), not per
  keyframe. Models authored with Simplified Chinese bone names are the
  ordinary case: of the local models, several carry between 2 and 151 such
  names ([report](../reports/2026-09-17-phase7-local-motions.md)).
- A model element whose source name is empty — what an undecodable PMX name
  becomes — matches nothing.

### 8.2 A bake is not a data conversion

MMD motion is authored against MMD's **control rig**. Leg motion lives on the
IK bones (`足ＩＫ`), and the leg bones that deform the mesh get their rotation
from the IK solve; many models drive shoulders, twist bones and eyes through
append (inherit) transforms. So a VMD bound to a model and written straight
into a `UsdSkelAnimation`, without evaluating IK and append transforms, moves
the IK targets and leaves the legs in their rest pose.

The consequence is structural: **baking VMD to a correct `UsdSkelAnimation`
requires evaluating MMD control semantics**, which is never done at import,
under the static importer boundary
([DESIGN_POLICY.md §2.2](DESIGN_POLICY.md#22-the-static-importer-boundary)).
The same holds for a `MotionClip`: a raw bone track is control-rig motion,
not body motion, so it does not enter the shared core unevaluated (§10.2).

**Who evaluates (MOT-O3, superseded 2026-09-17): this repository, in
`mmdControl`, a plain library outside the importer.** Phase 7 had given the
evaluation to the Phase 8 motion or avatar runtime. The `usd-motion-plugins`
design policy then placed everything that depends on MMD IK conventions,
bone flags or morph semantics in this repository (its §3.2 and §38), and
expects body motion to leave it already normalized (its §19.2) — which, for
MMD, cannot happen before IK and append transforms are evaluated. So the
evaluator is an MMD semantic, owned here; a runtime **schedules** it — per
frame, in a thread, as an OpenExec node it wraps — and never re-implements it
([DESIGN_POLICY.md §20](DESIGN_POLICY.md#20-alignment-with-the-usd-motion-plugins-design-policy)).

What does not change: `motionVmd`, `mmdMotionBinding` and every file-format
plugin still solve nothing, opening a file still evaluates nothing, and
nothing offers a bake that skips the evaluation. A consumer that does skip it
must say so in its output and its diagnostics, and must not present the
result as equivalent.

### 8.3 The IK / visibility track

IK-enable keyframes switch an IK chain on or off over time; they are input to
the same evaluator, `mmdControl` (§10.3), preserved and bound by bone name
like any other track. Model visibility keyframes are preserved likewise, bound to the model
as a whole.

## 9. Open questions

| Id | Question | Resolve by |
| --- | --- | --- |
| MOT-O2 | What a directly opened `.vmd` stage looks like. `usd-motion-plugins` fixes the frame (`/Animation`, the body as `UsdSkelAnimation`, `customData.motion`); what remains is that a VMD without a model has only control-rig tracks, which no evaluator can turn into body motion (§10.2) — so either the stage carries source tracks outside `Body`, or no such stage exists | `usd-motion-plugins`' `motionUsd` contract, then a consumer |
| MOT-O4 | Camera and light tracks: any USD mapping at all | a consumer that needs one |
| MOT-O8 | Whether `mmdControl` must also evaluate from a stage alone — `/Asset/rig` and `/Asset/morph` — for a runtime that holds no `CanonicalDocument` (§10.3) | a consumer that holds only the stage |
| MOT-O10 | The rest a clip from MMD states. Every MMD bone rests at identity rotation, so §12.3's rotations are relative to the model's modelled pose, in which every local character's upper arms point 37–42° below horizontal ([report](../reports/2026-09-19-phase9-roles-and-root.md)) — not the level arms a VRM's identity rest describes. Whether `mmdSkeletonAdapter` states a `SourceRestPose` measured from the rest bone directions (as the shared core's BVH profiles state `rest-offsets`), and against which reference directions, or leaves the difference to a retarget option | a measured A-pose-to-level-arm retarget comparison; the first generic skeletal acceptance used identity rest rotations and could not answer it ([report](../reports/2026-09-22-phase9-motion-acceptance.md)) |
| MOT-O11 | Whether a plane link starts from its keyed rotation. §11.7 starts an enabled chain's plane angles at zero, so a knee's keyed rotation never reaches the pose and the knee is solved from straight; three.js r168 starts from the keyed rotation. On a motion that keys its legs' rotations alongside their goals, that start alone leaves a median 0.05 mm where §11.7 leaves 2.4 mm, and moves the knees a median 4.6 mm and at most 66 mm; on an IK-authored motion it changes little ([report](../reports/2026-09-19-phase9-ik-reference.md)). Which MMD does | MMD's output on a motion that keys its IK links |

Resolved:

| Id | Question | Decision | Resolved |
| --- | --- | --- | --- |
| MOT-O1 | Where the shared basis-conversion functions live, so `mmdModel` and `motionVmd` share them without depending on each other | in `mmdModel`, applied by `mmdMotionBinding`; `motionVmd` converts nothing (§2) | Phase 7, 2026-09-17 |
| MOT-O3 | Which runtime owns MMD IK and append evaluation for baking | ~~the Phase 8 motion or avatar runtime~~ — **superseded 2026-09-17:** this repository, in the plain library `mmdControl`, scheduled by a runtime but never re-implemented by one (§8.2) | Phase 7, 2026-09-17; superseded the same day |
| MOT-O7 | Which morph types `mmdControl` evaluates into the pose, and which reach `MotionChannelSet` as source weights | bone morphs, directly or as members of group morphs, are evaluated into the pose; every other bound morph track — group morphs included, bone morphs not — is a channel with its sampled weight, unexpanded (§11.3) | Phase 9, 2026-09-19 |
| MOT-O5 | Which MMD bones (`全ての親`, `センター`, `グルーブ`) become `RootMotion` and which stay hips-local motion | none is chosen: the root is the evaluated world transform of the joint `hips` maps to, so every ancestor's motion reaches it, and nothing of it stays in a local rotation below (§12.3) | Phase 9, 2026-09-19 |
| MOT-O6 | Which MMD bone names map to which `HumanJoint`, how English names and variants are matched, and how the table is versioned | table version 1, §12.2: exact source names, deforming (`D`) bones first; English names and spelling variants never match (§12.1); the version is recorded with every clip and bumped with any entry (§12.5) | Phase 9, 2026-09-19 |
| MOT-O9 | Whether MMD's own IK reaches a reachable goal closer than §11.7 does at a model's stored loop count | kept: §11.7 is unchanged. Against an independent implementation (three.js r168's `CCDIKSolver`) at the same 40 iterations, on the same frames and inputs, §11.7 leaves a median 0.55 mm on an IK-authored motion where the reference leaves 10.5 mm, and both leave at most 29 mm — the residual of 40 iterations of cyclic coordinate descent. Where the reference leaves less, the difference is the knee's start (MOT-O11). Matching MMD's own playback stays unverified ([report](../reports/2026-09-19-phase9-ik-reference.md)) | Phase 9, 2026-09-19 |

## 10. Normalizing into the shared motion core

Binding: this section is Phase 9
([DESIGN_POLICY.md §14](DESIGN_POLICY.md#14-phases)). `mmdControl` evaluates
as §11 says; `mmdMotionAdapter` and `mmdSkeletonAdapter` consume the two
packages released by `usd-motion-plugins` v0.5.0: `motionCore` (`HumanJoint`,
`MotionPose`, `RootMotion`, `MotionChannelSet`, `MotionClip`) and
`motionRetarget` (`SkeletonDescriptor`, `RetargetMap`, `SourceRestPose`)
([DEPENDENCIES.md §6](../architecture/DEPENDENCIES.md#6-usd-motion-plugins)).
The type names are that repository's, and where its published contract
differs from this section, the published contract wins and this section is
revised.

### 10.1 Components

| Component | Input | Output | Depends on |
| --- | --- | --- | --- |
| `mmdControl` | a `BoundMotion`, the `CanonicalDocument` it was bound to, a time | the local transform of every deformation joint at that time, in the USD basis and meters, plus the morph weights §10.7 leaves as channels (§11) | `mmdMotionBinding`, `mmdModel` |
| `mmdSkeletonAdapter` | a `CanonicalDocument` | a `SkeletonDescriptor`, versioned humanoid `RetargetMap` and `SourceRestPose` (§10.4, §12) | `mmdModel`, `usd-motion-plugins` `motionRetarget` |
| `mmdMotionAdapter` | `mmdControl` evaluated over a time range at an explicit rate; the source mapping/rest from `mmdSkeletonAdapter` | a `MotionClip` of evaluated humanoid motion and preserved generic channels | `mmdControl`, `mmdModel`, `mmdSkeletonAdapter`, `usd-motion-plugins` `motionCore` |

None links more of OpenUSD than those packages' foundation types, and none
authors a stage. The edges are
[WORKSPACE.md §2](../architecture/WORKSPACE.md#2-dependency-directions)'s;
the two adapters are the only components that cross into `usd-motion-plugins`
([WORKSPACE.md §2.4](../architecture/WORKSPACE.md#24-edges-out-of-this-repository)).

### 10.2 The pipeline

```text
VMD bytes → motionVmd → mmdMotionBinding ─→ BoundMotion           MMD source tracks
                              (+ mmdModel)        │
                                                  ▼
                        mmdControl, per sample time t:
                          1. sample every bound track's Bézier curve at t (§6)
                          2. apply bone morphs (§11.3)
                          3. evaluate every bone in MMD's evaluation order —
                             transform layer, then after-physics flag, never
                             the canonical joint order — applying appends and
                             solving IK (with the IK-enable track) as the
                             control semantics of STAGE_CONTRACT.md §12 say
                                                  │
                                                  ▼
                        deformation-joint local transforms at t
                                                  │
                        mmdSkeletonAdapter → roles + SourceRestPose
                        mmdMotionAdapter          ▼
                          evaluated pose → humanoid world rotations
                          MotionPose per t → MotionClip
                                                  │
                                                  ▼
                        usd-motion-plugins: sampling, retarget, recording,
                        UsdSkelAnimation authoring
```

A `MotionClip` from MMD is always **evaluated** motion: every pose holds
deformation-joint rotations after IK and append transforms, never the raw
rotation of an IK target. A clip built by skipping step 3 is not a lesser
clip; it is a wrong one, and no API here produces it.

Evaluation is **deterministic**: the same bound motion, model, time and rate
produce the same bits, as the importer's same-bytes rule requires
([DESIGN_POLICY.md §2.5](DESIGN_POLICY.md#25-determinism-over-cleverness)).

### 10.3 Evaluation

- **Where the semantics come from.** `mmdControl` reads the control
  semantics `mmdModel` canonicalizes — the same facts the importer authors
  under `/Asset/rig` in Phase 5 — so the stage and the evaluator cannot
  disagree about a chain. Evaluating from the stage alone is MOT-O8. The
  rules themselves are §11.
- **Physics.** Bones MMD drives by rigid bodies are not simulated. Their
  pose is what keyframes, appends and IK give them; bones flagged to deform
  after physics are evaluated in that position of the order with no
  simulation before them, and the clip's metadata says physics was not run.
  Physics stays a runtime's
  ([DESIGN_POLICY.md §8](DESIGN_POLICY.md#8-physics-policy)).
- **External parents** name another model in the scene; with one model there
  is nothing to resolve, so the relation is ignored and reported once per
  bone.
- **Sampling rate.** Evaluation happens at explicit times. The adapter's
  default is the VMD frame grid, 30 samples per second (§5); any other rate
  is the caller's explicit choice. Between keyframes the MMD Bézier curve is
  authoritative; the shared core's linear and shortest-path interpolation
  (its §8) applies only between the evaluated samples.
- **State.** An evaluator holds precomputed chain and order data built once
  per model, so a realtime caller reuses it across frames (the motion
  policy's §31). It owns no thread and no clock.

### 10.4 Skeleton and humanoid map

A PMX model meets the shared core in two directions. `mmdSkeletonAdapter`
owns the role table (§12), skeleton description and source rest; the motion
adapter consumes those results rather than duplicating the mapping.

- **As a source**, `mmdSkeletonAdapter` supplies the role mapping and
  `SourceRestPose`; a VMD bound to the model becomes a `MotionClip` of
  `HumanJoint` rotations and root motion, built as §12.3 says, with the
  `SourceRestPose` those rotations are relative to. No MMD joint reaches the
  clip; a retarget onto any skeleton reads the clip alone.
- **As a target**, `mmdSkeletonAdapter` exposes the PMX stage's skeleton
  through a `SkeletonDescriptor` and a `RetargetMap`, so a clip from elsewhere
  can drive it:
  - The **`SkeletonDescriptor`** is built with the shared core's
    `BuildSkeletonDescriptor` from the stage's joint tokens and the model's
    rest transforms, so its tokens are exactly `/Asset/skel/Skeleton`'s
    `joints` — hierarchical paths of stable identifiers
    ([STAGE_CONTRACT.md §9](STAGE_CONTRACT.md#9-skeleton-and-skinning)) — and
    a retargeted pose can be authored onto that skeleton as it is. The
    shared core asks for names that match `UsdSkelSkeleton.joints`, and the
    stage's tokens are those; source names, Japanese kept, stay on the
    canonical model and the stage's display names
    ([TEXT_ENCODING_POLICY.md §5](TEXT_ENCODING_POLICY.md#5-identity-versus-display)),
    which is where §12's table reads them. Every rest rotation is identity.
  - The **`RetargetMap`** binds each `HumanJoint` the table resolves to that
    joint's index, with the table's required set as `requiredBones`
    (§12.4). It is a heuristic, never a humanoid claim about the model. An
    explicitly authored map always wins over the table.
- The table maps into **retarget data**, not into USD identifiers, so it does
  not become the stage ABI that
  [TEXT_ENCODING_POLICY.md §6.3](TEXT_ENCODING_POLICY.md#63-what-contract-v1-deliberately-does-not-do)
  refuses to freeze.

### 10.5 Time, coordinates and root motion

| Shared core | From MMD |
| --- | --- |
| time in seconds | `timestamp = frame / 30`; `nominalFrameRate = 30` is descriptive, the timestamps are authoritative |
| Y-up, meters, right-handed | already true after binding (§7); nothing is converted again |
| local joint rotations | each `HumanJoint`'s rotation relative to its nearest mapped humanoid ancestor, from the evaluated world rotations (§12.3), relative to identity rest rotations ([STAGE_CONTRACT.md §9.2](STAGE_CONTRACT.md#92-the-skeleton-prim)); what that rest looks like is MOT-O10 |
| root motion separate from hips | `RootMotion` is the evaluated world position and orientation of the joint `hips` maps to (§12.3; MOT-O5, resolved) |
| provenance | `SourceMetadata` and clip metadata name the format (`vmd`) and the VMD's model name; they never change behavior |

### 10.6 What this repository does not do with the result

Retargeting, clip sampling, blending, recording and `UsdSkelAnimation`
authoring are `usd-motion-plugins`'. VRM ↔ MMD is not a pair this repository
knows: a `MotionClip` from a VMD reaches a VRM, and one from a VRMA reaches a
PMX, through the shared retarget and the other format's own descriptors. No
component here keeps a private copy of any of it
([WORKSPACE.md §7](../architecture/WORKSPACE.md#7-invariants), invariant 9).

### 10.7 Morphs as channels

Morph tracks that are not evaluated into the pose (§11.3) always reach
`MotionChannelSet` under the namespaced semantic `mmd:morph:<source name>`, with
the bound weight as a scalar. That source-preserving channel is retained even
when an optional semantic expression is emitted beside it.

The shared core owns common expression vocabulary; this repository owns any
mapping from an MMD source name to that vocabulary. Such a mapping is explicit,
versioned and limited to high-confidence conventions such as blink and basic
mouth visemes. It preserves the original channel, diagnoses ambiguity and
never turns an unknown model-specific morph into a guess. Generic motion code
contains no MMD morph-name table.

The evaluated model-visibility step track is carried on every sample as
`mmd:model:visibility`, with `1` for visible and `0` for hidden. The separate
`mmd:morph:` and `mmd:model:` sub-namespaces ensure that arbitrary
source-authored morph text cannot collide with this reserved adapter semantic.

### 10.8 Diagnostics

MMD-side events keep this repository's `MMD_MOTION_*` family
([reference/DIAGNOSTICS.md](../reference/DIAGNOSTICS.md)); codes are added to
its catalog with the code that raises them. Diagnostics the shared core
raises (`MOTION-E####`, `MOTION-W####`, `MOTION-I####`) are passed through
unchanged, never re-coded.

`mmdMotionAdapter` rejects a non-finite, reversed or non-positive-rate sample
request with `MMD_MOTION_INVALID_SAMPLE_RANGE`. It reports each required
source role absent from the versioned table once per clip as
`MMD_MOTION_MISSING_REQUIRED_JOINT`; the partial clip remains valid (§12.4).
An evaluated rotation, root position or channel that cannot be represented as
a finite shared value rejects the clip with `MMD_MOTION_NON_FINITE_SAMPLE`;
the adapter never hides it by substituting identity or zero.

## 11. Evaluating the control rig

Binding since Phase 9: `mmdControl` evaluates a bound motion over a model as
this section says, and each rule has a synthetic rig with a known answer in
its unit tests. The rules are MMD's playback as the open MMD runtimes
reproduce it; where a reading had to be chosen, the choice is named here.
Every quantity is in the USD basis and meters, as binding and
canonicalization leave it (§7), and every rotation of a canonical joint is
relative to an identity rest rotation
([STAGE_CONTRACT.md §9.2](STAGE_CONTRACT.md#92-the-skeleton-prim)). Rotations
are unit quaternions acting on column vectors: `a · b` applies `b` first. The
Z mirror of §7 preserves products (`S·(a·b)·S = (S·a·S)·(S·b·S)`), so MMD's
composition rules hold unchanged in the USD basis.

### 11.1 The evaluator

- **Prepared once per model.** `Prepare` reads the canonical model and keeps
  what evaluation needs — each joint's parent and rest translation, its
  control semantics, the IK chains and the evaluation order (§11.5) — and
  does not refer to the model afterwards. Its diagnostics (§11.8) are raised
  there, once, never per evaluation.
- **Evaluated at an explicit time.** `Evaluate` takes a motion bound to that
  model and a time in MMD frames (§5), fractional allowed, and returns a
  **pose**: for every canonical joint, its local translation — the rest
  translation from its parent plus what the motion adds — and its local
  rotation; the channel weights of §11.3; and the model's visibility.
- **Stateless.** Every evaluation starts from the rest pose. Nothing carries
  over from an earlier call — no IK solution warms the next — so a time gives
  the same pose whatever was evaluated before it, and the same inputs give
  the same bits (§10.2). A caller may pass its own pose to be refilled.
- A track whose index names no joint or morph of the prepared model is
  ignored: it was bound to another model.

### 11.2 Sampling the tracks

At frame `f`, every track is sampled on its own:

- **Before its first key or after its last**, a track holds that key's
  value; at a key's frame it has that key's value.
- **Bone tracks, between keys** `k0` and `k1`: `u = (f − f0) / (f1 − f0)`.
  Each channel — X, Y and Z translation, and rotation — takes its progress
  from **`k1`'s** curve for that channel: the curve stored on a key describes
  the transition into it. The curve's control points are divided by 127; the
  parameter `s` with `x(s) = u` is found by 32 bisections of `[0, 1]`, and the
  progress is `y(s)`. Translation interpolates each axis linearly by its own
  progress; rotation is the shortest-path spherical interpolation by the
  rotation's progress, normalized.
- **Morph tracks** interpolate linearly between keys: they have no curves.
- **IK and visibility tracks** are steps: the last key at or before `f`, or
  the first key before it. A joint with no IK track has its chain enabled,
  and a motion with no visibility keys leaves the model visible.

A bone with no track has no motion: translation zero, rotation identity. Key
values are used as stored, except that a key rotation is normalized (a zero
one is identity); a non-finite value gives non-finite output for that joint
and what depends on it, never a failure.

### 11.3 Morphs

Morphs are applied after the keys. Each morph's **effective weight** is its
track's sampled weight (0 without one) plus, for every group morph that lists
it, that group's effective weight times the member's weight. Groups may nest
— canonicalization has already dropped any member that would close a cycle —
and a morph that two groups reach takes both. Effective weights are
accumulated groups-first in one pass, so the work is linear in the morph
table however the groups nest.

**Bone morphs** are evaluated into the pose, in ascending morph index, each
with its effective weight `w` (a zero weight does nothing): for each of its
offsets `(t, q)`, in order, the joint's motion translation gains `w · t`, and
its motion rotation `ρ` becomes `slerp(1, q, w) · ρ` — a rotation about `q`'s
axis by `w` times `q`'s angle, applied after `ρ`. Of what a group reaches,
only its bone morphs change the pose.

No other morph type changes a joint: vertex, UV and material morphs are a
renderer's, and flip and impulse morphs are not evaluated. They reach the
pose's **channels** instead — one per bound morph track whose morph is
**not** a bone morph, with its sampled weight, never expanded — so a group
morph is a channel too (MOT-O7, §10.7). A consumer that expands a group
channel skips its bone morphs: they are already in the pose.

### 11.4 A joint's local transform

For joint `j`, with `T_j` its rest translation from its parent and `a_j`,
`ρ_j` its motion translation and rotation (§11.2, §11.3):

```text
translation_j = T_j + a_j + (appendT_j   if j appends translation)
rotation_j    = ik_j · ρ_j · (appendR_j  if j appends rotation)
world_j       = world_parent(j) · [translation_j, rotation_j]
```

`ik_j` is identity unless an IK chain rotates `j` (§11.7). A fixed axis, local
axes and the rotatable, translatable, visible and operable flags constrain
editing in MMD, not playback, and are not applied.

### 11.5 Evaluation order

Joints are evaluated in ascending **(after-physics flag, transform layer,
source index)** — never the canonical joint order
([STAGE_CONTRACT.md §12.1](STAGE_CONTRACT.md#121-per-joint-control-semantics)).
Before the pass every joint holds its motion, identity `ik` and no append.
In order, for each joint: if it appends, its append is taken from its
source's state at that moment (§11.6); then, if it is the IK bone of an
enabled chain, the chain is solved (§11.7). World transforms are always those
of the current state. Physics is not simulated: after-physics joints are
evaluated last with no simulation before them (§10.3).

### 11.6 Appends

For joint `j` appending from source `s` with ratio `r`:

- **Rotation.** `appendR_j` is a rotation about the axis of
  `A = ik_s · ρ_s · (appendR_s if s appends rotation)` by `r` times its angle,
  the angle taken in `[0, π]`, so a negative ratio turns the other way. The
  source's whole rotation is taken — its keys and morphs, its IK rotation and
  its own append — so a chain of appends composes, and a bone that follows an
  IK link (`足D` from `足`) follows the solved leg.
- **Translation.** `appendT_j = r · (a_s + (appendT_s if s appends translation))`.
- **Local appends** (PMX flag `0x0080`) are evaluated by the same rule, with
  `MMD_MOTION_LOCAL_APPEND_APPROXIMATED` (§11.8). None of 25 local models
  holds one ([report](../reports/2026-09-19-phase9-local-control.md)).

A source later in the order than `j` has not had its own append or IK applied
when `j` reads it; no local model has one.

### 11.7 IK

A chain has an IK bone `g` (the goal), an effector `e`, links `L1 … Ln` in
source order, a loop count `N` and an angle limit `α` in radians. A disabled
chain is not solved, and its links keep identity `ik`. An enabled one is
solved by cyclic coordinate descent:

1. Every link's `ik` is set to identity, and its plane angle and previous
   Euler angles (below) to zero. `best = +∞`.
2. For each iteration `0 … N − 1`, for each link `L` in order that is not the
   effector:
   - The goal's and the effector's world positions are taken into `L`'s frame
     and normalized; a vector shorter than `10⁻¹²` skips the link. The angle
     between them is clamped to `α`; below `10⁻³` degrees the link is skipped.
   - **Plane links** — limited, with exactly one axis whose lower or upper
     limit is non-zero — rotate about that axis only. Of the effector vector
     rotated by `+angle` and by `−angle` about it, the one with the larger dot
     product with the goal vector gives the sign (`−` on a tie), and the
     signed angle is added to the link's plane angle. In iteration 0 only, a
     plane angle outside the limits is negated if its negation is inside
     them, or if its negation is nearer the limits' midpoint. The plane angle
     is then clamped to the limits, and `ik_L = axisAngle(axis, plane angle) · ρ_L⁻¹`.
   - **Other links** rotate about the normalized cross product of the
     effector and goal vectors (one shorter than `10⁻¹²` skips the link):
     `R = ik_L · ρ_L · axisAngle(axis, angle)`. A limited link then takes
     `R`'s Euler angles `(x, y, z)`, `R = Rx(x) · Ry(y) · Rz(z)` — of
     `(x, y, z)` and the eight triples `(x ± π, ±π − y, z ± π)`, the one
     nearest its previous Euler angles by the sum of absolute differences,
     each wrapped into `(−π, π]`; when `|cos y|` is below `10⁻⁶`, `x` is the
     previous one — clamps each to its limits and then to within `α` of the
     previous angle, keeps the result as its previous Euler angles, and makes
     `R = Rx · Ry · Rz` of it. Then `ik_L = R · ρ_L⁻¹`.
   - After all links, `d` is the distance from the effector to the goal. If
     `d < best`, `best = d` and every link's `ik` is saved; otherwise the
     saved values are restored and the solve stops.

`N` is used as stored when it is between 0 and 256; outside, it is clamped
into that range with `MMD_MOTION_IK_LOOP_CLAMPED` (§11.8). The largest of the
25 local models' chains is 40
([report](../reports/2026-09-19-phase9-local-control.md)): the bound only
keeps a malformed model from turning one evaluation into billions of
iterations.

A plane link's angle starts at zero, not at its keyed rotation: while its
chain is enabled, a knee's keyed rotation never reaches the pose, and a
motion that keys both a leg and its goal is solved from a straight knee.
Whether MMD starts there too is MOT-O11; an independent implementation does
not ([report](../reports/2026-09-19-phase9-ik-reference.md)).

### 11.8 Diagnostics

Raised by `Prepare`, once per element, never by `Evaluate`:

| Event | Code | Severity |
| --- | --- | --- |
| a joint with an external parent: with one model there is nothing to resolve, so the relation is ignored (§10.3) | `MMD_MOTION_EXTERNAL_PARENT_IGNORED` | info |
| a local append, evaluated as a global one (§11.6) | `MMD_MOTION_LOCAL_APPEND_APPROXIMATED` | info |
| a chain's loop count outside `[0, 256]`, clamped (§11.7) | `MMD_MOTION_IK_LOOP_CLAMPED` | warning |

## 12. The humanoid role table

Binding, with §10: `mmdSkeletonAdapter` implements it, and
`mmdMotionAdapter` consumes it for source clips. It
resolves MOT-O5 and MOT-O6, against the 13 local characters and two
distributed motions of the
[2026-09-19 report](../reports/2026-09-19-phase9-roles-and-root.md).

The shared core never matches joint names: a `HumanJoint` drives a joint only
through an explicit entry, and building entries heuristically is the format
repository's (its RETARGETING_POLICY §3). This section is that heuristic for
MMD. It names roles; it never claims a model is a humanoid.

### 12.1 Matching

- A role is matched on a bone's **source name**, exactly: the decoded
  Japanese name as the model states it, compared as a whole string. It is
  the same identity a VMD track binds by (§8.1).
- **English names never match.** Every one the report looked at was empty
  or not the bone's name — `左腕` carries `Bip001 R Finger2` in one model — so
  a match on them would bind the wrong joint silently.
- **No spelling is folded.** Width (`１`/`1`), `ひざ`/`膝` and the like are not
  normalized: no local character spells a table name differently, and a fold
  that no model exercises is a guess the table would then be versioned by.
  A variant is added as its own entry, in a new table version, when a model
  shows it.
- Each role lists its candidates in order, and the **first bone the model
  has** is the one bound. A candidate the model lacks is skipped; a role
  none of whose candidates it has is unmapped.
- An explicitly authored map always wins over the table (§10.4).

### 12.2 The table, version 1

Left side shown; the right side is the same with `右` for `左`. Candidates in
order, deforming (`D`) bones first: in a model that has them they are the
bones the skin follows, and the non-`D` bone is the one the IK solves and
the `D` bone appends.

| `HumanJoint` | Candidates |
| --- | --- |
| `hips` | as a source: `下半身`. As a target: the nearest joint that is an ancestor of the `spine` joint and of both `upperLeg` joints (§12.3) |
| `spine` | `上半身` |
| `chest` | `上半身2` |
| `upperChest` | `上半身3` |
| `neck` | `首` |
| `head` | `頭` |
| `leftEye` | `左目` |
| `jaw` | — (no conventional bone) |
| `leftUpperLeg` | `左足D`, `左足` |
| `leftLowerLeg` | `左ひざD`, `左ひざ` |
| `leftFoot` | `左足首D`, `左足首` |
| `leftToes` | `左足先EX` |
| `leftShoulder` | `左肩` |
| `leftUpperArm` | `左腕` |
| `leftLowerArm` | `左ひじ` |
| `leftHand` | `左手首` |
| `leftThumbMetacarpal`, `…Proximal`, `…Distal` | `左親指０`, `左親指１`, `左親指２` |
| `leftIndexProximal`, `…Intermediate`, `…Distal` | `左人指１`, `左人指２`, `左人指３` |
| `leftMiddle…` | `左中指１`, `左中指２`, `左中指３` |
| `leftRing…` | `左薬指１`, `左薬指２`, `左薬指３` |
| `leftLittle…` | `左小指１`, `左小指２`, `左小指３` |

Never mapped: `全ての親`, `センター`, `グルーブ`, `腰` and `両目` as roles of
their own; IK bones (`左足ＩＫ`, `左つま先ＩＫ`) and IK tips (`左つま先`);
twist bones (`左腕捩`, `左手捩`), `左肩P`/`左肩C`, `腰キャンセル左`, and every
bone the table does not name. A model without `左親指０` maps `左親指１` to
`…Proximal` all the same: an entry never shifts to fill a missing one.

### 12.3 Evaluated motion as `HumanJoint` rotations and root motion

MMD's hierarchy is not the humanoid one: `上半身` and `下半身` are siblings
under `腰`, and between two table joints sit bones no role names —
`腰キャンセル`, `肩P`, `肩C`, the twist bones. So a clip is built from
**world** rotations, never by copying a local one:

- For each sample, `mmdControl`'s local transforms are composed down the
  canonical hierarchy into each joint's world transform.
- A mapped `HumanJoint`'s local rotation is the world rotation of its
  **nearest mapped humanoid ancestor** — the nearest ancestor in the
  vocabulary's hierarchy that the table maps, the shared core's
  `NearestPresentAncestor`, never an ancestor in MMD's — inverted, times its
  own world rotation. `spine` is relative to `hips` although `上半身` is not a child of
  `下半身`, and whatever the unnamed bones between two table joints do — the
  twist bones' rotation, `腰キャンセル`'s cancellation — reaches the child's
  rotation instead of being dropped. For a chain with no unnamed bone this is
  the local rotation itself, and it agrees with the shared core's path rule
  for recorded files (its MOTION_CONTRACT §11).
- **Root motion (MOT-O5).** `RootMotion::worldPosition` and
  `worldOrientation` are the world position and rotation of the joint `hips`
  maps to, both flagged present — the shared core's record for a rig rooted
  at its hips (its MOTION_CONTRACT §5.3), and `hips`' local rotation is that
  same world rotation. **No MMD bone is chosen as the root.** `全ての親`,
  `センター`, `グルーブ` and `腰` are all ancestors of `下半身`, so whatever each
  of them does reaches the root, and nothing of it is left in a local
  rotation below. In both distributed motions `センター` carries every
  translation — up to 16 model units of travel and 3 of height — and
  `全ての親`, `グルーブ` and `腰` hold at most one key, at zero.
- The `SourceRestPose` states the table joints' rests, with each bone's
  semantic parent from the same nearest-mapped-ancestor rule. Every rest
  rotation is identity; what that rest is, as a pose, is MOT-O10.

**As a target**, the same table binds joints, but a retarget writes each
bound joint's rotation relative to *its own* parent on the PMX side (the
shared core's RETARGETING_POLICY §4.1, case 6), and UsdSkel evaluates no
append. Binding `hips` to `下半身` there would turn the legs and leave
`上半身` behind; so `hips` binds to the nearest common ancestor of the `spine`
and `upperLeg` joints — `腰` in each of the 12 local characters with legs —
whose rotation moves both halves, as a humanoid's hips do. A model without
the three joints has no such ancestor, and `hips` is left unmapped (§12.4).
`下半身` stays at rest under it, and an unevaluated `腰キャンセル` is at rest
too, so the legs are relative to `hips` as the clip means.

### 12.4 Required joints

The shared core leaves the required set to the caller (its
RETARGETING_POLICY §4). The table's is `hips`, `spine`, `head`, and on both
sides `upperLeg`, `lowerLeg`, `foot`, `upperArm`, `lowerArm`, `hand` —
fifteen joints, the set VRM 1.0 requires, so a PMX target and a VRM target
report alike. Twelve of the 13 local characters resolve all fifteen; the
thirteenth, a 68-bone partial without legs, resolves the upper body.

- **As a target**, the set is `RetargetOptions::requiredBones`, and a joint
  left unmapped is the shared core's `MOTION_RETARGET_MISSING_REQUIRED_BONE`
  from `DiagnoseRig`, passed through unchanged (§10.8).
- **As a source**, a required joint the table cannot resolve is absent from
  every pose of the clip, and reported once under an `MMD_MOTION_*` code
  added with the adapter. The clip is still built: a partial clip is valid
  in the shared core.

### 12.5 Version

The table is **version 1**. Any change to an entry — a candidate added,
removed or reordered, a role mapped differently — changes which joint a
clip drives, so it is a new version, recorded in the changelog. The version
is a constant of `mmdSkeletonAdapter`, reported with every map it builds and
recorded in a clip's provenance where the shared core gives it a place. It
is independent of the shared core's `HumanJointVocabularyVersion` (1), which
the table is written against: a vocabulary bump is a table review.
