# Motion contract (MMD-specific boundary)

> Status: **proposed**; nothing is implemented, and this is Phase 7 work. This
> document holds only what is specific to MMD motion — VMD's source facts, its
> text encoding, and where it meets a PMX model. Generic motion concepts
> (canonical poses, clips, retarget, recording, the USD bridge) belong to the
> shared motion architecture and are expected to move to `usd-motion-plugins`;
> this document defers to that contract wherever it exists. Section numbers are
> stable.

---

## 1. Scope

- **In:** VMD motion files — bone and morph keyframes, and the IK-enable and
  visibility track — and how they bind to a PMX model.
- **Recorded, not mapped:** VMD camera, light and self-shadow tracks. They are
  scene motion, not model motion; they are parsed and kept in the source
  representation, and no USD mapping is defined until a consumer needs one.
- **Out:** VMD writing; live motion; motion generation; VRM ↔ MMD retargeting.

## 2. Components and boundaries

```text
VMD bytes ─→ motionVmd (syntax) ─→ motion source representation
          ─→ canonical motion semantics ─→ retarget / bake ─→ UsdSkelAnimation
```

- **`motionVmd`** is a plain library: VMD syntax, CP932 decoding, and the
  source representation. It has **no OpenUSD** dependency, like `mmdPmx`.
- It is **extraction-ready**: it may depend on the shared motion contract and
  nothing else in this repository. Never `usdMmdFileFormat`, never `mmdModel`,
  never `mmdPmx`
  ([WORKSPACE.md §2](../architecture/WORKSPACE.md#2-dependency-directions)).
- **Parsing a VMD never requires a PMX.** Binding the two is a separate,
  explicit operation (§8).
- **`usdVmdFileFormat`** — opening a `.vmd` directly with `UsdStage::Open` — is
  added only once the shared motion contract says what an avatar-independent
  motion stage looks like (MOT-O2). It would be a thin bundle over
  `motionVmd`, re-implementing no parsing.

The source-to-USD basis conversion must be usable by both `mmdModel` and the
motion path without either depending on the other; where those functions live
is decided when `motionVmd` is created (MOT-O1).

## 3. VMD source facts

A summary for orientation; the parser's fixtures are the authority on layout.
All values little-endian; `float` is binary32.

| Section | Count | Record |
| --- | --- | --- |
| header | — | 30-byte signature `Vocaloid Motion Data 0002` (NUL-padded), 20-byte model name. The older `Vocaloid Motion Data file` signature has a 10-byte model name. |
| bone keyframes | `uint32` | bone name (15 bytes), frame (`uint32`), translation (vec3), rotation (quaternion `x, y, z, w`), interpolation (64 bytes) |
| morph keyframes | `uint32` | morph name (15 bytes), frame (`uint32`), weight (`float`) |
| camera keyframes | `uint32` | frame, distance, position (vec3), rotation (vec3), interpolation (24 bytes), view angle (`uint32`), orthographic flag (`uint8`) |
| light keyframes | `uint32` | frame, color (vec3), direction (vec3) |
| self-shadow keyframes | `uint32` | frame, mode (`uint8`), distance (`float`) |
| IK / visibility keyframes | `uint32` | frame, visible (`uint8`), IK count (`uint32`), then per IK: bone name (20 bytes), enabled (`uint8`) |

Older files end after any section; a missing trailing section is an empty
section, not truncation. A section that starts and is cut short is truncation.
The reading rules of [PMX_CONTRACT.md §2](PMX_CONTRACT.md#2-reading-rules) —
bounded reads, counts bounded by remaining bytes, no pointer walking — apply
unchanged.

## 4. Text

VMD names are fixed-length **Shift-JIS (CP932)** fields, NUL-padded, and a
name longer than its field is cut — possibly between the two bytes of one
character.

- `motionVmd` decodes CP932 with a mapping table the project owns — never an
  OS API, `iconv`, or the process locale
  ([TEXT_ENCODING_POLICY.md §9](TEXT_ENCODING_POLICY.md#9-pmd-and-vmd)).
- Padding (the first NUL onward) is removed. A truncated trailing lead byte is
  dropped with `MMD_TEXT_TRUNCATED_CP932` (info).
- The **raw field bytes are kept** beside the decoded name, because binding
  compares bytes (§8.1).

## 5. Time

A VMD frame number is a frame at **30 frames per second**. A USD
representation of VMD time uses `timeCodesPerSecond = 30` and frame numbers as
time codes, so time codes and MMD frames stay equal. Keyframes on one track are
sorted by frame; duplicate frames on one track keep the last record, with
`MMD_MOTION_DUPLICATE_KEYFRAME` (warning).

## 6. Interpolation

Each bone keyframe carries cubic Bézier curves — one each for X, Y and Z
translation and one for rotation — as control points in `0`–`127`; camera
keyframes carry their own set. The canonical motion keeps the curves.
Sampling them onto a fixed time grid is a **bake**, a separate step with an
explicit sample rate, never an implicit side effect of reading.

## 7. Coordinates

VMD values are in the PMX source basis and MMD units. They are converted with
the same functions as the model
([STAGE_CONTRACT.md §6](STAGE_CONTRACT.md#6-coordinate-conversion)):
translations as displacements (`× s`, Z negated), rotations as quaternions
(`(x, y, z, w) → (−x, −y, z, w)`). A bone keyframe's translation is relative to
the bone's rest position and its rotation relative to the bone's world-aligned
rest orientation, which is why the model's joints have identity rest rotations
([STAGE_CONTRACT.md §9.2](STAGE_CONTRACT.md#92-the-skeleton-prim)).

## 8. Binding a VMD to a PMX model

A VMD names bones and morphs; it does not index them. Binding is a separate
operation that takes a canonical motion and a model and produces a
model-specific motion, with its own diagnostics.

### 8.1 Name matching

MMD matches by comparing bytes: a PMX name is encoded to CP932 and cut to the
VMD field width, and compared with the raw field. Binding does the same —
matching on decoded Unicode strings would miss every name longer than the
field. Matching is always on **source names**, never on stable identifiers
([TEXT_ENCODING_POLICY.md §5](TEXT_ENCODING_POLICY.md#5-identity-versus-display)).

- A VMD bone with no matching model bone is dropped with
  `MMD_MOTION_UNMATCHED_BONE` (info), once per name.
- Two model bones whose truncated CP932 names are equal make that VMD name
  ambiguous: the lower bone index wins, with `MMD_MOTION_AMBIGUOUS_NAME`
  (warning).
- A model name that cannot be encoded in CP932 can never match; that is
  reported once per model, not per keyframe.
- Morph keyframes bind to model morphs the same way.

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
The bake therefore lives in a motion or avatar runtime that implements MMD's IK
and append semantics over the `/Asset/rig` contract (Phase 5), not in
`motionVmd` and not in a file-format plugin. A bake that skips that evaluation
must say so in its output and its diagnostics, and must not be presented as
equivalent.

### 8.3 The IK / visibility track

IK-enable keyframes switch an IK chain on or off over time; they are runtime
input to the same evaluator, preserved and bound by bone name like any other
track. Model visibility keyframes are preserved likewise.

## 9. Open questions

| Id | Question | Resolve by |
| --- | --- | --- |
| MOT-O1 | Where the shared basis-conversion functions live, so `mmdModel` and `motionVmd` share them without depending on each other | when `motionVmd` is created |
| MOT-O2 | What a directly opened `.vmd` stage looks like | the shared motion contract (`usd-motion-plugins`) |
| MOT-O3 | Which runtime owns MMD IK and append evaluation for baking | Phase 7, with the motion architecture |
| MOT-O4 | Camera and light tracks: any USD mapping at all | a consumer that needs one |
