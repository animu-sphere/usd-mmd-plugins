# Phase 7 against distributed motions (2026-09-17)

Dated evidence from real runs; append-only
([contributing/documentation.md](../contributing/documentation.md)).

## What was run

Three runs over the VMD files held locally — distributed MMD motions, none of
them a fixture, none committed or redistributed
([DESIGN_POLICY.md §13](../design/DESIGN_POLICY.md#13-testing-policy)) — and
the PMX models of the Phase 6 report, on Windows 11, MSVC.

1. **The reader.** `vmd_inspect --json` read each motion. A Python script
   independent of `motionVmd` read the same bytes with Python's own `cp932`
   codec and compared, per motion: the sections present, the records in every
   section, and every bone, morph and IK track — its name bytes, its decoded
   name (a cut trailing lead byte dropped), its key count and its first and
   last frame.
2. **The interpolation bytes.** The same script counted, over every bone
   keyframe, what bytes 2 and 3 hold and whether rows 1–3 are row 0 shifted
   by one byte.
3. **Binding.** A scratch program linking `mmdPmx`, `mmdModel`, `motionVmd`
   and `mmdMotionBinding` bound each of the two motions with bone tracks to
   each of the 25 local PMX models.

## The reader

| Motion | Size | Sections | Bone keyframes | Morph keyframes | Camera | IK records | Tracks (bone / morph / IK) | Diagnostics | `vmd_inspect` |
| --- | ---: | :---: | ---: | ---: | ---: | ---: | --- | --- | ---: |
| A (dance) | 1.6 MB | 5 of 6 | 14 160 | 1 279 | 0 | — | 140 / 31 / — | none | 16 ms |
| B (camera) | 4 KB | 4 of 6 | 0 | 0 | 70 | — | 0 / 0 / — | none | 7 ms |
| C (dance) | 11 MB | 6 of 6 | 101 652 | 262 | 0 | 1 | 159 / 83 / 17 | `MMD_TEXT_TRUNCATED_CP932` ×1 | 32 ms |

The independent reader agreed with the tool on every count and every track of
all three. A and B end early — after the self-shadow and the light section — which
is why a missing trailing section is an empty one
([MOTION_CONTRACT.md §3](../design/MOTION_CONTRACT.md#3-vmd-source-facts)).
C's one diagnostic is a morph name cut inside its last character. No motion
holds a duplicate frame on a track, trailing bytes, or a name CP932 does not
map.

## The interpolation bytes

| Motion | Bone keyframes | Bytes 2, 3 | Rows 1–3 are row 0 shifted | Row 1 carries Z and rotation `x1` |
| --- | ---: | --- | ---: | ---: |
| A | 14 160 | the curves' own values (`(20, 20)` in 14 143) | 14 160 | 14 160 |
| C | 101 652 | `(99, 15)` in 99 683, `(0, 0)` in 1 969 | 0 | 101 652 |

In C, bytes 2 and 3 of row 0 are not curve values: MMD writes a physics toggle
there. Everything else in the rows is consistent in both files — row 1's
bytes are row 0's from byte 1 on, except where row 0 holds the toggle — so
the `x1` of Z and of rotation are read from row 1
([MOTION_CONTRACT.md §6](../design/MOTION_CONTRACT.md#6-interpolation)).

## Binding

Of the 25 models, 12 are complete characters (over 250 bones) and one a
68-bone partial; the rest are props of 1–46 bones, which bind a bone or none.
No model makes any name ambiguous. Tracks bound, of A's 140 bone and 31 morph
tracks and C's 159 bone, 83 morph and 17 IK tracks:

| Model | Bones | Morphs | A: bone / morph | C: bone / morph / IK | Names not in CP932 |
| --- | ---: | ---: | --- | --- | ---: |
| A | 280 | 63 | 76 / 21 | 68 / 28 / 4 | 2 |
| B | 516 | 61 | 77 / 21 | 70 / 27 / 4 | 151 |
| C | 490 | 68 | 77 / 21 | 70 / 28 / 4 | 2 |
| D | 319 | 131 | 72 / 28 | 70 / 32 / 4 | 0 |
| E | 319 | 131 | 74 / 28 | 70 / 32 / 4 | 0 |
| F | 566 | 62 | 77 / 21 | 70 / 27 / 4 | 2 |
| G | 444 | 65 | 76 / 22 | 70 / 28 / 4 | 12 |
| H | 633 | 70 | 77 / 21 | 70 / 28 / 4 | 2 |
| I | 366 | 62 | 74 / 21 | 70 / 27 / 4 | 16 |
| J | 461 | 106 | 70 / 26 | 70 / 32 / 4 | 0 |
| K | 692 | 65 | 77 / 21 | 70 / 28 / 4 | 0 |
| L | 692 | 64 | 77 / 21 | 70 / 28 / 4 | 0 |
| M | 68 | 0 | 33 / 0 | 40 / 0 / 0 | 0 |

Each binding took under a millisecond. The unbound tracks are bones and
morphs the models do not have — motion C drives sleeve and hair bones of its
own model — and each is one `MMD_MOTION_UNMATCHED_BONE` or
`MMD_MOTION_UNMATCHED_MORPH`, bounded to 16 per section. The names CP932
cannot encode are bone names in Simplified Chinese (`齿下`, `左腕饰`, …) on
models authored in Chinese: MMD cannot match them either, and each model
reports them once, with `MMD_MOTION_UNENCODABLE_NAME`
([MOTION_CONTRACT.md §8.1](../design/MOTION_CONTRACT.md#81-name-matching)).
