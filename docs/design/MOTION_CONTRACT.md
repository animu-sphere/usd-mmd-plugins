# Motion contract (MMD-specific boundary)

> Status: **binding** since Phase 7 for §2–§8.1 and §8.3: `motionVmd` reads
> VMD as §3–§6 say, `vmd_inspect` reports on it, and `mmdMotionBinding` binds
> a motion to a model as §7 and §8.1 say, each with fixtures. §8.2 is a
> boundary, not an implementation: nothing here bakes. This document holds
> only what is specific to MMD motion — VMD's source facts, its text encoding,
> and where it meets a PMX model. Generic motion concepts (canonical poses,
> clips, retarget, recording, the USD bridge) belong to the shared motion
> architecture and are expected to move to `usd-motion-plugins`; this
> document defers to that contract wherever it exists. Section numbers are
> stable.

---

## 1. Scope

- **In:** VMD motion files — bone and morph keyframes, and the IK-enable and
  visibility track — and how they bind to a PMX model.
- **Recorded, not mapped:** VMD camera, light and self-shadow tracks. They are
  scene motion, not model motion; they are parsed and kept in the source
  representation, and no USD mapping is defined until a consumer needs one.
- **Out:** VMD writing; live motion; motion generation; VRM ↔ MMD retargeting;
  baking (§8.2).

## 2. Components and boundaries

```text
VMD bytes ─Read─→ motionVmd::Document ─BuildMotion─→ motionVmd::Motion
                                                          │
                     mmd::CanonicalDocument ──────────────┤
                                                          ▼
                                   mmdMotionBinding::Bind ─→ BoundMotion
                                                          │
                     (a motion or avatar runtime: IK, append, bake) ─→ UsdSkelAnimation
```

- **`motionVmd`** is a plain library: VMD syntax, CP932 decoding, and the
  source representation. It has **no dependency at all** — no OpenUSD, and
  nothing in this repository.
- It is **extraction-ready**: it may depend on the shared motion contract and
  nothing else. Never `usdMmdFileFormat`, never `mmdModel`, never `mmdPmx`
  ([WORKSPACE.md §2](../architecture/WORKSPACE.md#2-dependency-directions)).
  No installable shared motion contract exists yet, so today it depends on
  nothing, and carries its own diagnostic record and `Result<T>` rather than
  reach `mmdPmx`'s
  ([WORKSPACE.md §7](../architecture/WORKSPACE.md#7-invariants)).
- **Parsing a VMD never requires a PMX.** `vmd_inspect` reports on a VMD with
  `motionVmd` alone.
- **`mmdMotionBinding`** is where a motion and a model meet: it depends on
  `mmdModel` and `motionVmd`, so neither of them depends on the other (§8).
- **`usdVmdFileFormat`** — opening a `.vmd` directly with `UsdStage::Open` — is
  added only once the shared motion contract says what an avatar-independent
  motion stage looks like (MOT-O2). It would be a thin bundle over
  `motionVmd`, re-implementing no parsing.

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
requires evaluating MMD control semantics**, which is runtime work under the
static importer boundary
([DESIGN_POLICY.md §2.2](DESIGN_POLICY.md#22-the-static-importer-boundary)).

**Who evaluates (MOT-O3, resolved in Phase 7): not this repository.** The
bake lives in the motion or avatar runtime that composes this repository
with the shared motion architecture — Phase 8, under `usd-avatar-runtime`
([DESIGN_POLICY.md §14](DESIGN_POLICY.md#14-phases)) — implementing MMD's IK
and append semantics over the `/Asset/rig` contract (Phase 5) and consuming a
`BoundMotion`. Neither `motionVmd`, `mmdMotionBinding` nor a file-format
plugin solves IK or evaluates an append, and nothing here offers a bake that
skips that evaluation. A runtime that does skip it must say so in its output
and its diagnostics, and must not present the result as equivalent.

### 8.3 The IK / visibility track

IK-enable keyframes switch an IK chain on or off over time; they are runtime
input to the same evaluator, preserved and bound by bone name like any other
track. Model visibility keyframes are preserved likewise, bound to the model
as a whole.

## 9. Open questions

| Id | Question | Resolve by |
| --- | --- | --- |
| MOT-O2 | What a directly opened `.vmd` stage looks like | the shared motion contract (`usd-motion-plugins`) |
| MOT-O4 | Camera and light tracks: any USD mapping at all | a consumer that needs one |

Resolved:

| Id | Question | Decision | Resolved |
| --- | --- | --- | --- |
| MOT-O1 | Where the shared basis-conversion functions live, so `mmdModel` and `motionVmd` share them without depending on each other | in `mmdModel`, applied by `mmdMotionBinding`; `motionVmd` converts nothing (§2) | Phase 7, 2026-09-17 |
| MOT-O3 | Which runtime owns MMD IK and append evaluation for baking | the Phase 8 motion or avatar runtime, over `/Asset/rig` and a `BoundMotion`; nothing in this repository bakes (§8.2) | Phase 7, 2026-09-17 |
