# Phase 9, then Phase 8, and Phase 10 — shared motion, material and avatar composition, and USDZ packaging

Status: 🚧 Phase 9 in progress — `mmdControl`, both shared-motion adapters,
skeletal end-to-end acceptance, MOT-O5, MOT-O6 and MOT-O9 are done; MOT-O10
and expression interoperability wait on `usd-motion-plugins`; Phase 8
has its `mmdSchema` bundle, and the importer authors stage-contract v2 with
the fallback graphs connected; the UsdImaging adapter is next. Phase 10,
USDZ packaging, has its design and has not started.

Three Phases remain. Phases 9 and 8 run in this order although they are
numbered the other way, and Phase 10 needs only the importer's stage, so it
can run alongside either
([DESIGN_POLICY.md §14](../design/DESIGN_POLICY.md#14-phases)):

- **Phase 9 — shared motion core adoption.** This repository evaluates MMD
  motion over the control rig (`mmdControl`) and hands the result to
  `usd-motion-plugins` as a `MotionClip` (`mmdMotionAdapter`), while
  `mmdSkeletonAdapter` exposes the PMX skeleton for generic retargeting
  ([MOTION_CONTRACT.md §10](../design/MOTION_CONTRACT.md#10-normalizing-into-the-shared-motion-core)).
- **Phase 8 — material and avatar runtime composition.** This repository adds
  `MmdMaterialAPI` and its UsdImaging bridge; `usd-avatar-runtime` composes this
  repository with `usd-vrm-plugins`, `usd-motion-plugins`,
  `usd-physics-plugins`, `motion-connectors`, `hydra-toon` and
  `usd-stage-runner` through
  OpenStrata. Most of it is owned outside this repository: the runtime, not
  this repository, is the avatar execution environment.
- **Phase 10 — USDZ packaging.** `mmd_usdz` packages the imported stage and
  its textures as a standard USDZ that opens without this repository's
  plugins ([PACKAGING_POLICY.md](../design/PACKAGING_POLICY.md)).

What is listed here is only the part this repository owes, or waits for; what
the runtime consumes from here today is [v0.1.0](../releases/v0.1.0.md).

As of 2026-09-21 `usd-motion-plugins` v0.5.0 is published with installable
`motionCore`, `motionRetarget` and `motionUsd`
([DEPENDENCIES.md §6](../architecture/DEPENDENCIES.md#6-usd-motion-plugins)).
All three are consumed by digest-pinned artifacts: `motionCore` and
`motionRetarget` by the adapters, and `motionUsd` by the skeletal acceptance
test. `mmdMotionAdapter` and `mmdSkeletonAdapter` are implemented.

## Outcome

```text
.pmx ─→ usdMmdFileFormat ─→ /Asset stage ─→ MmdMaterialAPI ─→ UsdImaging adapter ─→ hydra-toon
.vmd ─→ motionVmd ─→ mmdMotionBinding ─→ mmdControl ─→ mmdMotionAdapter ─→ MotionClip
.pmx ─→ mmdModel ─→ mmdSkeletonAdapter ─→ SkeletonDescriptor / RetargetMap
                                                                              │
usd-motion-plugins:  retarget to any skeleton, record, author UsdSkelAnimation ◀┘
usd-stage-runner:    orders pose → MMD physics sync → physics step → feedback
usd-avatar-runtime:  composes the above per frame and coordinates rendering
```

## Phase 9 — what remains

- ✅ **`mmdControl`** (2026-09-19): Bézier sampling of a bound motion, bone
  and group morphs, evaluation in MMD's order, appends, IK with the IK-enable
  track, as
  [MOTION_CONTRACT.md §11](../design/MOTION_CONTRACT.md#11-evaluating-the-control-rig)
  says, with its unit, robustness, boundary, sanitizer and installed-consumer
  tests over synthetic rigs with known answers, and checked against 13 local
  models and two distributed motions
  ([report](../reports/2026-09-19-phase9-local-control.md)). MOT-O7 is
  resolved with it.
- ✅ **MOT-O9** (2026-09-19): §11.7 kept. Against three.js r168's
  `CCDIKSolver` on the same frames and inputs, at the same 40 iterations,
  §11.7 leaves a median 0.55 mm on an IK-authored motion where the reference
  leaves 10.5 mm, and both leave at most 29 mm
  ([report](../reports/2026-09-19-phase9-ik-reference.md)). MOT-O11 is
  opened with it.
- ⬜ **MOT-O11**: whether a knee's plane angle starts from its keyed rotation
  rather than from zero — all of the reference's advantage on a motion that
  keys its legs alongside their goals. Needs MMD's output on such a motion;
  the rule does not change without it.
- ✅ **MOT-O5 and MOT-O6** (2026-09-19): the humanoid role table, version 1,
  and how evaluated motion becomes `HumanJoint` rotations and root motion
  ([MOTION_CONTRACT.md §12](../design/MOTION_CONTRACT.md#12-the-humanoid-role-table)),
  measured against the 13 local characters and two distributed motions
  ([report](../reports/2026-09-19-phase9-roles-and-root.md)). No MMD bone is chosen as the root: it is the world
  transform of the joint `hips` maps to. MOT-O10, the rest a clip from MMD
  states, is opened with them.
- ⛔ **MOT-O10**: measured 2026-09-25
  ([report](../reports/2026-09-25-phase9-rest-pose-comparison.md)). Onto a
  level-arm skeleton, today's identity rest leaves every arm segment a median
  40° low. A rest aimed from the arm chain's rest bone directions removes
  that, and whole-body aiming is wrong for MMD. The source rest is correct
  only if the same rest is stated for a PMX target: stated on the source
  alone, PMX to PMX goes from 2.5° to 40°. So it waits for
  `usd-motion-plugins` to take a target rest distinct from the stage's bind
  rest, and to publish the T-pose directions. A level-arm clip already
  reaches a PMX target 40° low today.
- ✅ **MOT-O12** (2026-09-25): role-table version 2. As a target, `上半身2`
  and `上半身3` bind in the model's chain order. As a source, `upperChest` is
  never emitted, because the shared retarget drops a joint a target lacks.
  Arms from models with `上半身3` below `上半身2` went from at most 25° off
  to at most 2.5°
  ([report](../reports/2026-09-25-phase9-upper-chest.md)).
- ✅ **`mmdMotionAdapter` and `mmdSkeletonAdapter`** (2026-09-21): digest-pinned
  `motionCore` and `motionRetarget` v0.5.0 packages; role-table version 1;
  stage-token `SkeletonDescriptor`, source rest and target `RetargetMap`;
  evaluated world rotations normalized into `MotionClip`, root motion and
  namespaced morph channels; unit, boundary and installed-consumer tests.
- ✅ **Acceptance end to end** (2026-09-22): through `motionRetarget` and
  `motionUsd`, a VMD-derived clip poses the PMX stage's
  skeleton with legs driven by IK, and retargets to a non-MMD synthetic
  skeleton through a translation unit whose interface and implementation name
  no MMD type. The deterministic test uses generated source and target rigs;
  a separate local run retargeted a 1,201-sample dance onto a 539-joint PMX
  stage, passed `usdchecker`, and was observed moving in `usdview`
  ([report](../reports/2026-09-22-phase9-motion-acceptance.md)).
- ⛔ **Expression interoperability** waits for the shared core to promote a
  common expression semantic. As of `usd-motion-plugins` v0.5.x none exists,
  and one is promoted only by a revision of its motion contract, never by a
  mapping. Until then, keep every original `mmd:morph:<source name>` channel.
  When a semantic exists, optionally emit only explicit, versioned,
  high-confidence mappings such as blink and basic mouth visemes. Unknown model-specific morphs remain source channels
  and generic motion code contains no MMD name table. Model visibility remains
  independently available under `mmd:model:visibility`
  ([MOTION_CONTRACT.md §10.7](../design/MOTION_CONTRACT.md#107-morphs-as-channels)).

## Phase 8 — what remains

### Material schema and renderer integration

Phase 3 remains complete: contract-v1 stages already preserve every canonical
MMD material value and carry generic `preview` and `mtlx` fallbacks. Phase 8
formalizes that contract for an MMD-aware renderer as stage-contract v2. It
keeps the fallbacks, and connects them to the canonical values instead of
copying those values
([MATERIAL_POLICY.md](../design/MATERIAL_POLICY.md)).

- ✅ **Schema decision and attribute inventory** (2026-09-22):
  `MmdMaterialAPI` is a single-apply API on `UsdShadeMaterial`; existing
  `mmd:material:*` names, types and meanings are retained, neutral fallbacks are
  fixed, provenance stays outside the API, and MAT-O4 is resolved as an
  UsdImaging adapter rather than a third realization graph.
- ✅ **Canonical values as Material interface inputs** (2026-09-25, MAT-O5):
  UsdShade connects only `inputs:`, so a plain `mmd:material:*` attribute
  cannot drive a realization. In Storm the connection is ignored without an
  error. The values become `inputs:mmd:material:*` with the same types. RGBA
  stays `color4f`, because a textured preview connects it to
  `UsdUVTexture.scale` exactly. The morph-modulated values and the texture
  slots are varying. The change is stage-contract v2
  ([report](../reports/2026-09-25-phase8-material-inputs.md)).
- ✅ **`mmdSchema`** (2026-09-25): the `usd-schema` bundle registers
  `MmdMaterialAPI`, the §4.1 inventory as `inputs:mmd:material:*`, with
  C++ accessors (`UsdMmdMaterialAPI`, `UsdMmdTokens`) generated from
  `schema/schema.usda`, and Python through the schema registry. It is also
  installed as a CMake package. Its tests cover registration, the inventory,
  connectability, the accessors and regeneration drift, and `ost plugin test`
  runs its L0–L5 pyramid. It also has CI cells, and a probe in the
  installed-consumer lane. It admits no other MMD schema.
- ✅ **Importer on stage-contract v2** (2026-09-25): `usdMmdFileFormat`
  applies `MmdMaterialAPI` to every material and authors the canonical inputs
  through its accessors. It stamps version 2, and `mmdSchema` is a declared
  and Plug-loaded runtime dependency. A safe texture slot states its sRGB
  encoding on the canonical input. A connected shader reads its colour space
  there, and without it Storm's MaterialX path drew textures lighter. MAT-O6
  is resolved: the untextured `/preview` keeps a static copy
  ([report](../reports/2026-09-25-phase8-importer-contract-v2.md)).
- ✅ **Realizations connected** (2026-09-25): `/mtlx` reads diffuse and the
  texture, and a textured `/preview` reads them too, through their graph
  interface inputs. Over 15 local models, both contexts draw as v1 did, to
  Storm's own render noise. A runtime diffuse override reaches both
  realizations of a textured material, and `/mtlx` of an untextured one. An
  untextured material whose texture path was refused draws in Storm's
  translucent pass, which changes only its edges
  ([report](../reports/2026-09-25-phase8-importer-contract-v2.md)).
- ⬜ Implement a UsdImaging adapter that exposes `MmdMaterialAPI` to Hydra
  without making the importer depend on `hydra-toon`.
- ⬜ Bring up the `hydra-toon` MMD path in this order: diffuse/alpha, toon
  ramp, sphere multiply/add, sub-texture, outline, shadow flags, material morph
  runtime, then advanced UV and vertex-color behavior.
- ⬜ After both `VrmMtoonMaterialAPI` and `MmdMaterialAPI` paths work, evaluate
  renderer-private common code. Do not introduce a USD-level `ToonMaterialAPI`
  until the two concrete implementations demonstrate stable common semantics.

### Avatar runtime composition

- ⬜ Consume the packages from `usd-avatar-runtime` — `usdMmdFileFormat`, and
  `motionVmd`, `mmdMotionBinding`, `mmdControl`, `mmdMotionAdapter` and
  `mmdSkeletonAdapter` for
  motion — and change a contract here only if that consumer shows one is
  wrong ([PACKAGE_CONTRACT.md](../architecture/PACKAGE_CONTRACT.md)).
- ⛔ `usdVmdFileFormat` waits for MOT-O2: `usd-motion-plugins` defines the
  standalone motion stage, and what a model-free VMD can put in it is still
  open ([MOTION_CONTRACT.md §9](../design/MOTION_CONTRACT.md#9-open-questions)).
  When it is answered, the basis functions a model-free `.vmd` stage needs are
  extracted from `mmdModel` with it (MOT-O1).
- ⬜ Verify generic motion targeting PMX with synthetic, BVH-derived and
  VRMA-derived clips through `mmdSkeletonAdapter`; no target path evaluates a
  VMD again.
- ⬜ Verify `motion-connectors → MotionPose → shared retarget → PMX` first from
  deterministic recorded captures. Live devices and network access are demo
  concerns, not CI requirements, and no protocol dependency enters this
  repository.

### Physics runtime integration

Static physics preservation is complete in Phase 6. Phase 8 owns only the
runtime composition described by
[PHYSICS_INTEGRATION.md](../design/PHYSICS_INTEGRATION.md):

- ⬜ verify that `usd-physics-plugins` can consume the existing
  `/Asset/physics` stage contract without backend-specific metadata;
- ⬜ establish the optional MMD coupling-adapter component and installed
  dependency only when that first consumer fixes the required API;
- ⬜ synchronize `followBone` bodies from the evaluated/retargeted pose;
- ⬜ simulate dynamic bodies and constrained pairs;
- ⬜ apply `dynamicWithBone` feedback to the runtime pose outside the importer;
- ⬜ improve limits, springs and damping incrementally, with backend-specific
  tolerances confined to backend tests.

The intended frame order is pose evaluation, bone → body synchronization,
shared physics step, body → bone feedback, then final pose consumption.
`usd-stage-runner` owns that order; `usd-avatar-runtime` owns composition and
playback state. PMX parsing and `Usd.Stage.Open("model.pmx")` remain usable
without any physics runtime.

## Phase 10 — what remains

The design was measured first with OpenUSD 26.08's own packaging over the
local models
([report](../reports/2026-09-25-usdz-packaging-probe.md)). A `.pmx` handed to
OpenUSD's packager as it stands becomes the package's root layer. A
materialized `.usdc` with its textures at their stage-relative paths opens
without the plugins. 31 of the 41 local PMX files name a BMP, which USDZ
cannot hold.

- ⬜ **Step 1 — a minimal package.** `tools/mmdUsdz/` with its manifest, CI
  cell and product membership
  ([WORKSPACE.md §1.2](../architecture/WORKSPACE.md#12-later-only-when-their-responsibility-is-real)).
  Open, discover, convert BMP and TGA to PNG, materialize, write with
  `SdfZipFileWriter`, validate, and move into place
  ([PACKAGING_POLICY.md §3](../design/PACKAGING_POLICY.md#3-pipeline)). The
  `MMD_PKG_` diagnostics. Fixtures with BMP, a `.spa` holding BMP, TGA,
  non-ASCII names, a shared texture and a missing one. A plugin-free open
  test, `usdchecker`, and the `.pmx`-against-`.usdz` comparison.
- ⬜ **Step 2 — robust assets and a deterministic archive.** Name
  collisions, relative-path normalization checked against the importer's,
  and missing-asset reporting. PKG-O1: the same input gives the same bytes in
  any time zone.
- ⬜ **Step 3 — portability.** `--portable-paths` (PKG-O2), and non-ASCII
  input and output paths in CI on every platform.
- ⬜ **Step 4 — a shared packaging layer**, only once `usd-vrm-plugins` has a
  packager too, extracted from the two working tools.

## Completion criteria

[DESIGN_POLICY.md §14](../design/DESIGN_POLICY.md#14-phases)'s acceptance for
Phase 9, for Phase 8 and for Phase 10.
