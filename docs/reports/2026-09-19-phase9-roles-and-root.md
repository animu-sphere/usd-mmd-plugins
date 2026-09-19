# Phase 9 humanoid roles and root motion against distributed models and motions (2026-09-19)

Dated evidence from real runs; append-only
([contributing/documentation.md](../contributing/documentation.md)).

## What was run

Four runs over the 25 PMX models and the two dance motions of the
[Phase 7 report](2026-09-17-phase7-local-motions.md) — distributed files,
none a fixture, none committed or redistributed
([DESIGN_POLICY.md §13](../design/DESIGN_POLICY.md#13-testing-policy)) — on
Windows 11. No library of this repository was changed for them.

1. **Bone names.** `mmd_inspect --json` over every model; a Python script
   counted, over the 13 models of more than 60 bones (12 characters and one
   68-bone partial), which conventional MMD bone names each has, under exact
   comparison and under NFKC, which English names those bones carry, and
   each one's parent.
2. **The table.** The same script resolved
   [MOTION_CONTRACT.md §12.2](../design/MOTION_CONTRACT.md#122-the-table-version-1)'s
   table, version 1, against each of the 13, and found each one's nearest
   common ancestor of `上半身` and both upper legs.
3. **Rest arms.** A second Python reader, of the PMX bone table alone,
   measured over the same 13 the angle below horizontal of the line from
   `左腕` to `左ひじ`, and from `右腕` to `右ひじ`, in the rest pose.
4. **Root bones in motion.** A Python reader of the VMD bone section,
   independent of `motionVmd`, measured for motions A (dance, 1.6 MB) and C
   (dance, 11 MB) each root-side bone's key count, translation range and
   largest rotation.

## Bone names

Every table name that a model has, it spells exactly as the table does. NFKC
changed no match: the only names that differ from a conventional spelling
under it are the leg IK bones, `左足ＩＫ` and `左つま先ＩＫ` in full-width
letters, in all 12 characters — IK bones, which the table never maps.

| Names | Characters that have them (of 13) |
| --- | ---: |
| `全ての親`, `センター`, `グルーブ`, `上半身`, `上半身2`, `首`, `頭`, `下半身` | 13 |
| `腰` | 12 |
| `上半身3` | 2 |
| `左目` / `右目`, `両目` | 12 |
| `左足` `左ひざ` `左足首` and their `D` bones, `左足先EX`, `左つま先` (each side) | 12 |
| `左肩` `左腕` `左腕捩` `左ひじ` `左手捩` `左手首` (each side) | 13 |
| `左肩P`, `左肩C` (each side) | 12 |
| thumb `０`–`２`, index and middle `１`–`３` (each side) | 13 |
| ring and little `１`–`３` (each side) | 12 |
| index to little `０` (each side) | 1 |

The thirteenth model without legs, `腰` or eyes is the 68-bone partial.

**English names.** Of the table bones, most carry none. Where one is set it
is often not the bone's name: `左腕` carries `Bip001 R Finger2`, `左ひじ`
`WeaponR`, `頭` `+HemB L C21` and `左足` `x` in one model each, and `左足先EX`
carries `toe2_L` in 7. No English name was the evidence for any role.

**Between table joints.** The table joints are not parent and child in MMD's
hierarchy:

| Bone | Parent (characters) |
| --- | --- |
| `上半身`, `下半身` | `腰` (12), `グルーブ` (1) — siblings in all 13 |
| `左足`, `左足D` | `腰キャンセル左` (10), `下半身` (2) |
| `左肩` | `左肩P` (12), `上半身2` (1) |
| `左腕` | `左肩C` (12), `左肩` (1) |
| `左ひじ` | `左腕捩` (13) |
| `左手首` | `左手捩` (13) |
| `左足先EX` | `左足首D` (12) |

## The table

Of the table's 54 roles (the vocabulary's 55 but `jaw`, which MMD has no
bone for), 10 characters resolve 53 — all but `upperChest` — and two resolve
all 54. All 12 resolve the 15 required joints, and every leg through its `D`
bone. The partial resolves 31 roles, missing the six leg joints the required
set names.

The nearest common ancestor of `上半身` and both upper-leg joints is `腰` in
each of the 12 characters; the partial has no legs, so it has none.

## Rest arms

Every one of the 13 rests with its upper arms 37.3°–42.4° below horizontal,
the two sides equal to the tenth of a degree in each: an A pose, at identity
rotation on every bone.

## Root bones in motion

Translations in model units (about 8 cm each), rotations the largest over the
track's keys.

| Bone | Motion A | Motion C |
| --- | --- | --- |
| `全ての親` | no track | 1 key, zero |
| `センター` | 393 keys; x −5.45 to 3.70, y −2.40 to 1.53, z −5.90 to 8.90; rotation to 180° | 128 keys; x −10.32 to 5.48, y −2.92 to 0.40, z −8.07 to 10.87; no rotation |
| `グルーブ` | no track | 1 key, zero |
| `腰` | no track | 1 key, zero |
| `下半身` | 318 keys, rotation only, to 76.5° | 69 keys, rotation only, to 176.0° |
| `上半身` | 352 keys, rotation only, to 105.7° | 69 keys, rotation only, to 175.2° |
| `左足ＩＫ` | 292 keys, translation only | 82 keys, translation and rotation |

Every body translation is on `センター` in both motions, and in A the body's
turn is too. The leg IK bones translate independently of it, and IK folds
them into the legs' rotations before a clip exists
([MOTION_CONTRACT.md §11.7](../design/MOTION_CONTRACT.md#117-ik)).

## What this decided

- **MOT-O6**: table version 1 as
  [§12.2](../design/MOTION_CONTRACT.md#122-the-table-version-1) states it —
  exact source names, `D` bones first, no English name and no folded
  spelling, and a required set every character here resolves.
- **MOT-O5**: no MMD bone is chosen as the root. `センター` carries the motion
  here, but a motion keyed on `全ての親` or `グルーブ` would move the body just
  as well, and all of them are ancestors of `下半身`; the root is `下半身`'s
  evaluated world transform
  ([§12.3](../design/MOTION_CONTRACT.md#123-evaluated-motion-as-humanjoint-rotations-and-root-motion)).
- **Target `hips`**: `腰`, found as a common ancestor rather than named,
  because `上半身` and `下半身` are siblings in every model here.
- **Opened, MOT-O10**: every bone rests at identity rotation while every
  character here is modelled with its arms about 40° down; a clip relative to
  that rest, retargeted onto a rig whose identity rest has level arms, would
  raise every arm by as much. What rest the clip states is decided against a
  retarget onto a non-MMD skeleton, which this run did not make.
