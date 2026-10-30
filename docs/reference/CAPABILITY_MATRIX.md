# Capability matrix

What the current code supports, feature by feature. This page states **facts
about the tree**, not plans; a status here changes only in the change that adds
the fixture proving it.

This is the only current feature-support summary. Contracts own semantics,
[current.md](../roadmap/current.md) owns task status, and dated reports own
measurements. The PMX import table's Intended column states a contract target,
not an execution claim.

The fixtures behind the current claims are in
[plugins/usdMmdFileFormat/tests/fixtures/](../../plugins/usdMmdFileFormat/tests/fixtures/);
`fixtures.json` there says what each must do, and what its stage must hold.

## Status vocabulary

| Status | Meaning |
| --- | --- |
| **supported** | implemented and covered by deterministic tests |
| **approximated** | imported with documented loss or a generic fallback |
| **preserved** | the source semantic is retained on the stage but not executed or rendered |
| **unsupported** | intentionally rejected or omitted, with a diagnostic |
| **experimental** | scope qualifier for opt-in local probes; not shipped runtime support |
| **unverified** | an implementation may exist, but the project makes no support claim |
| **—** | no implementation exists |

Never "supported" merely because code parses the bytes.

## PMX parsing (`mmdPmx`, `mmd_inspect`)

What the parser reads into a `pmx::Document` — source facts, not a stage.
Each claim is backed by the generated fixtures and the parser's unit tests.

| Capability | Current | Contract |
| --- | :---: | --- |
| PMX 2.0: every table | supported | [PMX §3–§13](../design/PMX_CONTRACT.md#3-header-and-globals) |
| PMX 2.1: every table, QDEF, flip and impulse morphs, soft bodies | supported | [PMX §5](../design/PMX_CONTRACT.md#5-vertices-and-deform), [§10](../design/PMX_CONTRACT.md#10-morphs), [§12](../design/PMX_CONTRACT.md#12-soft-bodies-21) |
| UTF-8 and UTF-16LE text, Japanese names, validated; malformed text read as empty | supported | [TEXT §3](../design/TEXT_ENCODING_POLICY.md#3-decoding-pmx-text) |
| Index widths 1 / 2 / 4, per index kind | supported | [PMX §4](../design/PMX_CONTRACT.md#4-indices) |
| Additional vec4 count 0–4 | supported | [PMX §5](../design/PMX_CONTRACT.md#5-vertices-and-deform) |
| Out-of-range indices repaired to none, with a diagnostic | supported | [PMX §4](../design/PMX_CONTRACT.md#4-indices) |
| Malformed-input rejection: truncation, counts, layout bytes, face structure | supported | [PMX §15](../design/PMX_CONTRACT.md#15-fatal-versus-recoverable) |
| Fuzzing under ASan and UBSan | supported | [DESIGN_POLICY §13](../design/DESIGN_POLICY.md#13-testing-policy) |
| Reading a file by a non-ASCII path (`ReadFile`, `mmd_inspect`) | supported | [TEXT §4](../design/TEXT_ENCODING_POLICY.md#4-no-locale-anywhere) |

## VMD reading (`motionVmd`, `vmd_inspect`)

What the reader holds in a `motionVmd::Document` and groups into a
`motionVmd::Motion` — source facts in the source basis, not a stage. Each
claim is backed by the reader's unit and robustness tests and the VMD
fixtures `tests/fixtures/generate_vmd_fixtures.py` writes.

| Capability | Current | Contract |
| --- | :---: | --- |
| Both signatures (`… 0002`, 20-byte model name; `… file`, 10-byte) | supported | [MOTION §3](../design/MOTION_CONTRACT.md#3-vmd-source-facts) |
| Bone, morph, camera, light, self-shadow and IK / visibility keyframes | supported | [MOTION §3](../design/MOTION_CONTRACT.md#3-vmd-source-facts) |
| A file that ends after any section | supported | [MOTION §3](../design/MOTION_CONTRACT.md#3-vmd-source-facts) |
| CP932 names through the project's table; the bytes kept; a cut character dropped, an unmapped sequence refused | supported | [MOTION §4](../design/MOTION_CONTRACT.md#4-text) |
| Tracks per name, sorted by frame; duplicate frames resolved to the last | supported | [MOTION §5](../design/MOTION_CONTRACT.md#5-time) |
| Bézier curves of bone and camera keyframes, including files whose physics toggle overwrites bytes 2 and 3 | supported | [MOTION §6](../design/MOTION_CONTRACT.md#6-interpolation) |
| Malformed-input rejection: signature, truncation, counts | supported | [MOTION §3](../design/MOTION_CONTRACT.md#3-vmd-source-facts) |
| Reading a file by a non-ASCII path (`ReadFile`, `vmd_inspect`) | supported | [TEXT §4](../design/TEXT_ENCODING_POLICY.md#4-no-locale-anywhere) |
| Camera, light and self-shadow tracks | preserved (parsed, not mapped) | [MOTION §1](../design/MOTION_CONTRACT.md#1-scope) |

## Motion binding (`mmdMotionBinding`)

| Capability | Current | Contract |
| --- | :---: | --- |
| Bone, morph and IK tracks bound by source name, as CP932 bytes cut to the field | supported | [MOTION §8.1](../design/MOTION_CONTRACT.md#81-name-matching) |
| Unmatched, ambiguous and unencodable names | supported | [MOTION §8.1](../design/MOTION_CONTRACT.md#81-name-matching) |
| Bone keys converted to the USD basis and meters with the model's conversion | supported | [MOTION §7](../design/MOTION_CONTRACT.md#7-coordinates) |
| The visibility track | preserved | [MOTION §8.3](../design/MOTION_CONTRACT.md#83-the-ik--visibility-track) |

## Control evaluation (`mmdControl`)

What evaluating a bound motion at one time gives: a pose of every
deformation joint, the morph channels, and the visibility. Each claim is
backed by a synthetic rig with a known answer in `mmdControl_unit`; core pose
and scalar group evaluation also have generated `mmdControl_robustness` checks.

| Capability | Current | Contract |
| --- | :---: | --- |
| Bone tracks sampled on their Bézier curves; morph tracks linearly; IK and visibility as steps | supported | [MOTION §11.2](../design/MOTION_CONTRACT.md#112-sampling-the-tracks) |
| Bone morphs, directly and through nested group morphs, evaluated into the pose | supported | [MOTION §11.3](../design/MOTION_CONTRACT.md#113-morphs) |
| Every other bound morph as a channel with its sampled weight | supported | [MOTION §11.3](../design/MOTION_CONTRACT.md#113-morphs) |
| Owner-side non-bone group weight resolution (`ResolveMorphChannels`), preserving raw channels and excluding bone effects | supported by generated unit/robustness and installed-consumer checks; generated runtime/live vertex-group publication validated in the [consumer report](../reports/2026-10-24-phase8-live-resolved-morph.md); scalar resolution alone does not realize leaf effects | [MOTION §11.3](../design/MOTION_CONTRACT.md#113-morphs) |
| Owner-side primary/additional UV deltas (`EvaluateUvMorphs`), including nested groups and primary-V conversion | generated numeric unit and installed-consumer checks; primary snapshot/live USD publication in the [live report](../reports/2026-10-25-phase8-live-primary-uv.md), explicit Toon UV transport in the [direct-consumer report](../reports/2026-10-26-phase8-toon-primary-uv.md), automatic renderer mapping in the [resident UV report](../reports/2026-10-26-phase8-resident-uv-mapping.md) and tangent regeneration in the [tangent report](../reports/2026-10-26-phase8-uv-tangents.md). The [composed UV report](../reports/2026-10-26-phase8-composed-resident-uv.md) adds generated UV-only MMD owner/runtime snapshots through automatic bindings with USD/Hydra stream and textured GPU parity. The [composed avatar report](../reports/2026-10-26-phase8-composed-avatar-uv.md) combines final joints, vertex morphs, Jolt feedback and UVs through the same resident paths with stream/pixel parity across resets. The [private-input review](../reports/2026-10-27-phase8-hydra2-visual-acceptance.md) adds manually accepted original PMX/VMD playback through composed USD and Hydra2. Private-avatar direct-snapshot parity, additional runtime UV output and representative acceptance remain open | [MOTION §11.3](../design/MOTION_CONTRACT.md#113-morphs) |
| Owner-side material modulation (`EvaluateMaterialMorphs`), including nested groups, multiply/add and all-material targets | generated numeric owner unit and installed-consumer checks; explicit typed runtime snapshot publication has generated 28-component, reset, lifetime and refusal checks in the [consumer report](../reports/2026-10-27-phase8-material-snapshots.md). Automatic material binding, live USD appearance and renderer sampled-contribution integration remain open | [MATERIAL §11](../design/MATERIAL_POLICY.md#11-material-morphs) |
| MMD's evaluation order: after-physics flag, transform layer, source index | supported | [MOTION §11.5](../design/MOTION_CONTRACT.md#115-evaluation-order) |
| Rotation and translation appends, negative ratios, chains of appends, appends from IK links | supported | [MOTION §11.6](../design/MOTION_CONTRACT.md#116-appends) |
| Local appends | approximated (evaluated as global, reported) | [MOTION §11.6](../design/MOTION_CONTRACT.md#116-appends) |
| IK by cyclic coordinate descent: angle limit, plane links, Euler-limited links, the IK-enable track | supported | [MOTION §11.7](../design/MOTION_CONTRACT.md#117-ik) |
| IK matching MMD's own playback within a distance | unverified: no MMD output to compare against; an independent implementation leaves no less distance at the same loop count (MOT-O9), and whether a knee starts from its keyed rotation is MOT-O11 | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) |
| External parents | unsupported (ignored, reported) | [MOTION §11.8](../design/MOTION_CONTRACT.md#118-diagnostics) |
| Physics before after-physics bones | unsupported by design (nothing is simulated) | [MOTION §10.3](../design/MOTION_CONTRACT.md#103-evaluation) |
| The same inputs give the same bits, whatever was evaluated before | supported | [MOTION §11.1](../design/MOTION_CONTRACT.md#111-the-evaluator) |
| Appends and IK over a given pose, such as a retarget's, with held joints returned unchanged and chains over them or feeding them off (`Complete`) | supported | [MOTION §11.9](../design/MOTION_CONTRACT.md#119-evaluating-from-a-given-pose) |
| Owned pre/post-physics control continuation (`BeginPhysics` / `ResumePhysics`) with sparse joint-local feedback and retained sampled VMD state | supported by generated control tests; no world step or runtime integration in this evaluator | [MOTION §11.10](../design/MOTION_CONTRACT.md#1110-intended-physics-continuation-boundary) |

## Shared-motion adapters (`mmdSkeletonAdapter`, `mmdMotionAdapter`)

Each claim is covered by synthetic adapter tests and by the installed-consumer
lane against digest-pinned `usd-motion-plugins` v0.5.3 packages.

| Capability | Current | Contract |
| --- | :---: | --- |
| Stage-token `SkeletonDescriptor` with canonical rest translations | supported | [MOTION §10.4](../design/MOTION_CONTRACT.md#104-skeleton-and-humanoid-map) |
| Role-table version 2, exact Japanese source names, deforming `D` candidates first, English names ignored; `上半身2`/`上半身3` bound in chain order as a target, no source `upperChest` | supported | [MOTION §12.1](../design/MOTION_CONTRACT.md#121-matching), [§12.2](../design/MOTION_CONTRACT.md#122-the-table) |
| Source role mapping and `SourceRestPose`; target `RetargetMap` with hips at the common spine/leg ancestor | supported | [MOTION §12.3](../design/MOTION_CONTRACT.md#123-evaluated-motion-as-humanjoint-rotations-and-root-motion) |
| Evaluated world rotations made local to the nearest mapped humanoid ancestor | supported | [MOTION §12.3](../design/MOTION_CONTRACT.md#123-evaluated-motion-as-humanjoint-rotations-and-root-motion) |
| Root motion from the evaluated world transform of the source hips role | supported | [MOTION §10.5](../design/MOTION_CONTRACT.md#105-time-coordinates-and-root-motion) |
| Explicit time range and rate, endpoint sampling, seconds and 30 VMD frames per second | supported | [MOTION §10.3](../design/MOTION_CONTRACT.md#103-evaluation), [§10.5](../design/MOTION_CONTRACT.md#105-time-coordinates-and-root-motion) |
| Non-bone morph weights under `mmd:morph:<source name>` channels | supported | [MOTION §10.7](../design/MOTION_CONTRACT.md#107-morphs-as-channels) |
| Evaluated model visibility under the reserved `mmd:model:visibility` channel | supported | [MOTION §10.7](../design/MOTION_CONTRACT.md#107-morphs-as-channels) |
| Cross-format common expression mapping | —; source morph channels retained, no common expression mapping emitted | [MOTION §10.7](../design/MOTION_CONTRACT.md#107-morphs-as-channels) |
| Non-finite evaluated shared values | rejected, never replaced | [MOTION §10.8](../design/MOTION_CONTRACT.md#108-diagnostics) |
| VMD-derived, IK-evaluated legs through a `motionUsd` round trip and `motionRetarget`, onto a PMX-derived stage skeleton and a non-MMD synthetic skeleton | supported | [Phase 9 acceptance report](../reports/2026-09-22-phase9-motion-acceptance.md) |
| The arm chain's reference rest, aimed onto the shared T-pose directions, as `SourceRestPose` and as a PMX target's `TargetRestPose`; stage rests unchanged (MOT-O10) | supported | [MOTION §12.6](../design/MOTION_CONTRACT.md#126-the-arm-chains-reference-rest), [report](../reports/2026-10-04-phase9-arm-chain-reference-rest.md) |
| A generic clip (a `.vrma` stage, a `motion_convert` BVH clip, any semantic motion stage) read by `motionUsd` and retargeted onto a PMX with no VMD evaluated, the source rest taken from its stage, identity or not | supported; on a PMX without `上半身3`, the clip's `upperChest` motion reaches `上半身2` when the consumer sets `foldUnboundIntermediateRotations` (MOT-O13), and is dropped otherwise | [MOTION §10.9](../design/MOTION_CONTRACT.md#109-a-generic-clip-onto-a-pmx), [report](../reports/2026-10-04-phase8-generic-clips-onto-pmx.md), [fold report](../reports/2026-10-04-phase8-upper-chest-fold.md) |
| Finishing a pose retargeted onto a PMX: arm roll carried onto `腕捩`/`手捩` with the twist bone's offset (`CarryArmRoll`), and the held joints (`heldJoints`) for `mmdControl`'s `Complete` | supported; twist bones found by exact name | [MOTION §10.10](../design/MOTION_CONTRACT.md#1010-finishing-a-retargeted-pose-on-a-pmx), [report](../reports/2026-10-05-phase8-pmx-pose-finish-shipped.md) |

## PMX model import

| Capability | Current | Intended | Phase | Contract |
| --- | :---: | --- | :---: | --- |
| `.pmx` registration, `Usd.Stage.Open` | supported | supported | 0 | [STAGE §1](../design/STAGE_CONTRACT.md#1-scope) |
| Non-ASCII file paths (read through `Ar`) | supported | supported | 0 | [TEXT §4](../design/TEXT_ENCODING_POLICY.md#4-no-locale-anywhere) |
| PMX 2.0 and 2.1 open; a malformed file fails with its fatal code | supported | supported | 0, 1 | [PMX §15](../design/PMX_CONTRACT.md#15-fatal-versus-recoverable) |
| Recoverable parser diagnostics recorded on the stage | supported | supported | 1 | [DIAGNOSTICS §4](DIAGNOSTICS.md#4-surfacing) |
| Stage metadata (`/Asset`, Y-up, meters, contract version, model names and comments) | supported | supported | 0, 2 | [STAGE §2](../design/STAGE_CONTRACT.md#2-contract-version), [§4](../design/STAGE_CONTRACT.md#4-prim-hierarchy), [§5](../design/STAGE_CONTRACT.md#5-model-metadata-and-provenance) |
| Coordinate conversion (right-handed, facing +Z, 0.08 m per MMD unit) | supported | supported | 2 | [STAGE §6](../design/STAGE_CONTRACT.md#6-coordinate-conversion) |
| Mesh: points, faces, normals | supported | supported | 2 | [STAGE §8](../design/STAGE_CONTRACT.md#8-geometry) |
| Primary UV (`primvars:st`) | supported | supported | 2 | [STAGE §8.3](../design/STAGE_CONTRACT.md#83-uvs) |
| Additional UV 1–4 | preserved | preserved | 2 | [STAGE §8.3](../design/STAGE_CONTRACT.md#83-uvs) |
| Per-vertex edge scale | preserved | preserved | 2 | [STAGE §8.4](../design/STAGE_CONTRACT.md#84-other-per-vertex-data) |
| Material face ranges (`GeomSubset`) | supported | supported | 2 | [STAGE §8.1](../design/STAGE_CONTRACT.md#81-one-mesh-material-subsets) |
| Material prims: identity, provenance, texture slots | supported | supported | 2 | [STAGE §10](../design/STAGE_CONTRACT.md#10-materials), [MATERIAL §4](../design/MATERIAL_POLICY.md#4-canonical-material-semantics) |
| Per-material culling | approximated + preserved | approximated + preserved | 2 | [STAGE §8.5](../design/STAGE_CONTRACT.md#85-double-sidedness) |
| Skeleton (`UsdSkelSkeleton`) | supported | supported | 2 | [STAGE §9](../design/STAGE_CONTRACT.md#9-skeleton-and-skinning) |
| Canonical joint order: reordering, invalid parents, parent cycles | supported | supported | 2 | [STAGE §9.1](../design/STAGE_CONTRACT.md#91-canonical-joint-order) |
| BDEF1 / BDEF2 / BDEF4 | supported | supported | 2 | [STAGE §9.4](../design/STAGE_CONTRACT.md#94-skinning) |
| Weight normalization, zero-weight vertices | supported | supported | 2 | [STAGE §9.5](../design/STAGE_CONTRACT.md#95-weight-normalization) |
| SDEF | approximated + preserved | approximated + preserved | 2 | [STAGE §9.4](../design/STAGE_CONTRACT.md#94-skinning) |
| QDEF | unverified | unverified | 2 | [PMX §5](../design/PMX_CONTRACT.md#5-vertices-and-deform) |
| A model without bones (`Xform` root, unskinned mesh) | supported | supported | 2 | [STAGE §4.1](../design/STAGE_CONTRACT.md#41-why-asset-is-the-skelroot) |
| Japanese names preserved (model, material, bone) | supported | supported | 2 | [TEXT §5](../design/TEXT_ENCODING_POLICY.md#5-identity-versus-display) |
| Stable ASCII identifiers | supported | supported | 2 | [TEXT §6](../design/TEXT_ENCODING_POLICY.md#6-stable-identifiers) |
| Japanese texture filenames | supported | supported | 2 | [TEXT §7](../design/TEXT_ENCODING_POLICY.md#7-texture-paths) |
| Unsafe texture paths | unsupported (refused, preserved) | unsupported (refused, preserved) | 2 | [TEXT §7.2](../design/TEXT_ENCODING_POLICY.md#72-unsafe-paths) |
| `UsdPreviewSurface` | approximated | approximated | 3 | [MATERIAL §5](../design/MATERIAL_POLICY.md#5-usdpreviewsurface-realization) |
| MaterialX `gltf_pbr` | approximated | approximated | 3 | [MATERIAL §6](../design/MATERIAL_POLICY.md#6-materialx-gltf_pbr-realization) |
| Native MMD material semantics | preserved | preserved | 3 | [MATERIAL §4](../design/MATERIAL_POLICY.md#4-canonical-material-semantics) |
| `MmdMaterialAPI` application | preserved (stage-contract v2) | preserved (stage-contract v2) | 8 | [MATERIAL §4.3](../design/MATERIAL_POLICY.md#43-mmdmaterialapi), [§14](../design/MATERIAL_POLICY.md#14-migration-and-implementation-order) |
| Sphere textures | preserved | preserved | 3 | [MATERIAL §8](../design/MATERIAL_POLICY.md#8-sphere-textures) |
| Toon ramps (individual and shared) | preserved | preserved | 3 | [MATERIAL §7](../design/MATERIAL_POLICY.md#7-toon-ramps) |
| Edge / outline | preserved | preserved | 3 | [MATERIAL §9](../design/MATERIAL_POLICY.md#9-edges) |
| Vertex morph (`UsdSkelBlendShape`) | supported | supported | 4 | [STAGE §11](../design/STAGE_CONTRACT.md#11-morphs) |
| Group, flip morph | preserved | preserved | 4 | [STAGE §11](../design/STAGE_CONTRACT.md#11-morphs) |
| Bone morph | preserved | preserved | 4 | [STAGE §11](../design/STAGE_CONTRACT.md#11-morphs) |
| UV and additional-UV morph | preserved | preserved | 4 | [STAGE §11](../design/STAGE_CONTRACT.md#11-morphs) |
| Material morph | preserved | preserved | 4 | [MATERIAL §11](../design/MATERIAL_POLICY.md#11-material-morphs) |
| Impulse morph | preserved | preserved | 4 | [STAGE §11](../design/STAGE_CONTRACT.md#11-morphs) |
| A vertex morph nothing can drive: no bones, or no mesh (preserved, no blend shape) | supported | supported | 4 | [STAGE §11.1](../design/STAGE_CONTRACT.md#111-vertex-morphs) |
| Group and flip morph cycles | supported | supported | 4 | [PMX §10](../design/PMX_CONTRACT.md#10-morphs) |
| Japanese morph names preserved | supported | supported | 4 | [TEXT §5](../design/TEXT_ENCODING_POLICY.md#5-identity-versus-display) |
| IK chains (effector, links, loop count, limit angle, link limits) | preserved | preserved | 5 | [STAGE §12.2](../design/STAGE_CONTRACT.md#122-ik-chains) |
| Append rotation and translation, local append | preserved | preserved | 5 | [STAGE §12.1](../design/STAGE_CONTRACT.md#121-per-joint-control-semantics) |
| Fixed axis, local axes, external parent key | preserved | preserved | 5 | [STAGE §12.1](../design/STAGE_CONTRACT.md#121-per-joint-control-semantics) |
| Transform layer, deform after physics, tail, rotatable / translatable / visible / operable | preserved | preserved | 5 | [STAGE §12.1](../design/STAGE_CONTRACT.md#121-per-joint-control-semantics) |
| A control relation that names no bone (dropped, the rest kept) | supported | supported | 5 | [STAGE §12.3](../design/STAGE_CONTRACT.md#123-repairs) |
| Display frames | — (parsed; no contract version authors them) | unsupported (parsed, not authored) | 1 | [PMX §11](../design/PMX_CONTRACT.md#11-display-frames) |
| Rigid bodies: shape, size, rest frame, mass, kinematic mode (`UsdPhysics`) | supported | supported | 6 | [STAGE §13.1](../design/STAGE_CONTRACT.md#131-rigid-bodies) |
| Rigid bodies: bone, collision group and mask, damping, restitution, friction, physics mode | preserved | preserved | 6 | [STAGE §13.1](../design/STAGE_CONTRACT.md#131-rigid-bodies) |
| Spring 6-DOF and 6-DOF joints: bodies, frames, limits (`UsdPhysics`) | supported | supported | 6 | [STAGE §13.2](../design/STAGE_CONTRACT.md#132-joints) |
| Joint springs, free axes, every joint value | preserved | preserved | 6 | [STAGE §13.2](../design/STAGE_CONTRACT.md#132-joints) |
| PMX 2.1 point-to-point, cone-twist, slider, hinge joints (no `UsdPhysics` joint) | preserved | preserved | 6 | [STAGE §13.2](../design/STAGE_CONTRACT.md#132-joints) |
| Undefined shape, mode or joint type; a joint whose body is missing | supported | supported | 6 | [STAGE §13.3](../design/STAGE_CONTRACT.md#133-repairs) |
| Soft bodies (2.1) | unsupported (parsed, not authored) | unsupported (parsed, not authored) | 1 | [PMX §12](../design/PMX_CONTRACT.md#12-soft-bodies-21) |

## Export (`mmd_export`)

What the exporter writes from the imported stage, in a process that loads the
importer through Plug. Each claim is backed by the `packaging/` fixtures, the
tool's unit and fixture tests, and the
[local-model report](../reports/2026-09-26-phase10-mmd-export-local-models.md).

| Capability | Current | Contract |
| --- | :---: | --- |
| `.pmx` → a standard USDZ that opens with no MMD plugin and passes `usdchecker` | supported | [PACKAGING §1–§9](../design/PACKAGING_POLICY.md#1-scope) |
| The package's stage is the importer's, spec for spec, texture renames aside | supported | [PACKAGING §4](../design/PACKAGING_POLICY.md#4-materialization) |
| Textures at the paths the stage names them by, UTF-8 names kept, each once | supported | [PACKAGING §5](../design/PACKAGING_POLICY.md#5-package-layout) |
| `--portable-paths`: ASCII root and texture names, deterministic numbering, provenance and image content kept | supported | [PACKAGING §5.1](../design/PACKAGING_POLICY.md#51-portable-archive-names) |
| BMP (under any extension) and TGA converted to PNG losslessly; PNG or JPEG under another extension renamed | supported | [PACKAGING §7](../design/PACKAGING_POLICY.md#7-texture-formats) |
| A missing or unsupported texture: nothing written; a colliding texture also stops the default layout | supported | [PACKAGING §6](../design/PACKAGING_POLICY.md#6-discovery-and-localization), [§7](../design/PACKAGING_POLICY.md#7-texture-formats) |
| Validation before the output is replaced, the `usdchecker` validators included | supported | [PACKAGING §9](../design/PACKAGING_POLICY.md#9-validation) |
| Non-ASCII input and output paths | supported | [PACKAGING §10](../design/PACKAGING_POLICY.md#10-command-line) |
| A byte-deterministic archive across runs, source mtimes and time zones, for the same input bytes, output stem and tool/OpenUSD build | supported | [PACKAGING §13](../design/PACKAGING_POLICY.md#13-determinism) |
| `.usdc` / `.usda` output | — (when `usdcat` proves insufficient) | [PACKAGING §16](../design/PACKAGING_POLICY.md#16-later) |

## Physics preservation and execution

Preservation refers to the imported static stage. Execution below covers the
isolated [consumer probe](../../tests/physics_consumer/CMakeLists.txt) and the
scoped [runtime coupling consumer](../reports/2026-10-21-phase8-runtime-consumer.md)
against locally rebuilt installed packages. These experimental results do not
establish published runtime dependencies or avatar L3 acceptance.
[Installed-package compatibility](../reports/2026-10-18-phase8-package-compatibility.md)
is checked at configure time; packages without compatibility metadata reject.
[PHYSICS_INTEGRATION §7](../design/PHYSICS_INTEGRATION.md#7-collision-filtering-and-joints)
owns conversion rules; [§12](../design/PHYSICS_INTEGRATION.md#12-fidelity-and-acceptance-policy)
owns adoption criteria.

The [scene-resource consumer report](../reports/2026-10-21-phase8-scene-resources.md)
extends generated lifecycle evidence to reusable host-owned body/joint resource
sets, construction rollback and constrained selective unload. It does not
establish automatic application/world binding, cross-avatar constraint policy,
resident rendering or representative L3 acceptance.

| Feature | Source / static preservation | Execution claim and limits | Contract / evidence |
| --- | --- | --- | --- |
| Body primitives, local poses, kinematic flags and mass | Supported stage authoring | Experimental reader; shared primitive mass inference is not PMX zero-mass equivalence | [Stage §13](../design/STAGE_CONTRACT.md#13-physics), [mass reader](../reports/2026-10-18-phase8-mass-reader.md) |
| Standard 6-DOF endpoints, frames and limits | Supported stage authoring | Approximated experimental six-DOF stepping, opt-in; backend angular limits differ from source conventions | [Execution report](../reports/2026-10-18-phase8-six-dof-execution.md) |
| Collision groups/masks | Preserved | Experimental neutral filter mapping; mixed-format category allocation has no runtime contract | [§7](../design/PHYSICS_INTEGRATION.md#7-collision-filtering-and-joints), [filter report](../reports/2026-10-18-phase8-collision-filter.md) |
| Friction/restitution and pair mixing | Preserved | Experimental coefficient execution and selected product policy; no trajectory-equivalence claim | [§7.1](../design/PHYSICS_INTEGRATION.md#71-contact-coefficients), [mixing report](../reports/2026-10-18-phase8-contact-mixing.md) |
| Linear/angular damping | Preserved | Approximated finite-rate execution for accepted coefficients; live/probe admission still refuses d=1. The source resolver and neutral clearing backend are separate prerequisites, not enabled live endpoint support | [§7.2](../design/PHYSICS_INTEGRATION.md#72-body-damping), [finite-rate report](../reports/2026-10-18-phase8-body-damping.md), [backend checkpoint](../reports/2026-10-31-phase8-damping-backend-boundary.md) |
| Joint spring metadata audit | Preserved source vectors | Supported local audit only; source-gap counts remain even for approximated execution | [§7.3](../design/PHYSICS_INTEGRATION.md#73-joint-spring-metadata-audit), [audit report](../reports/2026-10-18-phase8-joint-spring-audit.md) |
| Positive translation springs on free/limited standard spring6Dof axes | Preserved | Approximated experimental Hookean execution, independently default OFF; numerical damping and timestep dependence | [§7.4](../design/PHYSICS_INTEGRATION.md#74-experimental-translation-spring-approximation), [response report](../reports/2026-10-18-phase8-translation-springs.md) |
| Positive rotation springs on free/limited standard spring6Dof axes | Preserved | Approximated experimental small-angle execution, independently default OFF; finite-angle/multi-axis equivalence unverified | [§7.5](../design/PHYSICS_INTEGRATION.md#75-experimental-rotation-spring-approximation), [response report](../reports/2026-10-18-phase8-rotation-springs.md) |
| Negative/locked-axis spring coefficients; format-only PMX 2.1 joints | Preserved | Unsupported execution in this probe; audit reports source meaning | [§7.3–§7.5](../design/PHYSICS_INTEGRATION.md#73-joint-spring-metadata-audit) |
| Representative generated PMX reader/stability regression | Existing stage-contract tests verify preservation | Automated installed-importer probe; finite exposed state and exact numerical replay within one build/configuration, spring OFF and ON; no cross-platform determinism claim | [Fixture/replay report](../reports/2026-10-18-phase8-fixture-replay.md), [testing policy](../design/PHYSICS_INTEGRATION.md#10-testing) |
| Bone association and followBone/dynamic/dynamicWithBone modes | Preserved; USDA/USDC save/reopen checked | Optional `mmdPhysicsAdapter`: mode 1 feeds position/rotation to associated bones; mode 2 retains local translation and feeds rotation; initial-pose query covers all associated modes and seeds live-host frame zero/Reset; generated mixed-chain/root/offset tests and installed continuation smoke; rigid static placements only; adapter owns no Step; rendered/avatar L3 acceptance remains open | [Corrected feedback report](../reports/2026-10-25-phase8-dynamic-bone-feedback.md), [Start-pose report](../reports/2026-10-25-phase8-start-pose-initialization.md), [§5–§6](../design/PHYSICS_INTEGRATION.md#5-rigid-body-modes) |
| Bullet/MMD full trajectories and solver equivalence | Source values remain inspectable | Unverified; not a release gate | [§12](../design/PHYSICS_INTEGRATION.md#12-fidelity-and-acceptance-policy) |

Malformed metadata rejects in the probe's strict readers; an unsupported
well-formed feature is distinct from invalid input. Existing probe output
uses `jointFidelity=approximate/unverified` and approximation counters,
not a uniform four-state per-feature capability API.

Corpus counts, finite-step results, SDK/hosted evidence and response error
measurements remain in the linked reports. Accepted reader/finite-state
tests prove their bounded claims, not native MMD trajectories.
The [live keyed-input report](../reports/2026-10-24-phase8-live-keyed-input.md)
adds a generated VMD-driven pendulum with concrete body/final-skinning checks
and exact reset replay. Anonymous private rest-input smoke and explicit
morph/damping refusals do not establish representative PMX/VMD acceptance.
[DEPENDENCIES §7](../architecture/DEPENDENCIES.md#7-usd-physics-plugins)
owns package/ABI requirements and option discovery.

## Outside the PMX importer

The optional runtime-owned [live usdview host](../reports/2026-10-21-phase8-usdview-live.md)
validates a generated skinned pendulum through live Jolt, owner bone feedback
and final UsdSkel publication. Its
[body-properties follow-up](../reports/2026-10-21-phase8-live-body-properties.md)
consumes owner `ReadBodyProperties` for admission masks, contact coefficients
and finite damping rates, with generated live response/refusal checks. The
[spring follow-up](../reports/2026-10-21-phase8-live-springs.md) consumes owner
`ReadJointSprings` with independent default-OFF Hookean translation and
small-angle rotation opt-ins. Generated response, activation, reset/replay,
refusal and actual-window checks validate that bounded approximation;
negative/locked/format-only meaning remains unsupported. This visual path does
not establish representative PMX/VMD,
hair/skirt, material/morph or Hydra/direct parity acceptance.

The later [private-avatar visual review](../reports/2026-10-27-phase8-hydra2-visual-acceptance.md)
adds user-accepted Hydra2 display of original PMX/VMD input through the live
usdview host and current `hdToon` renderer. The dedicated viewport separately
played recorded final USD output. A subsequent scoped
[native-viewport host](https://github.com/animu-sphere/hydra-toon/blob/main/docs/reports/renderer/107-2026-10-27-live-mmd-viewport.md)
now consumes original PMX/VMD through the runtime's reusable MMD/Jolt session,
with final joint/morph/primary-UV resident bindings, Play/Pause/Reset/Stop and
whole-world teardown. Private-model performance is below real time.
Private-avatar Hydra/direct numerical or pixel parity, material output,
broader asset admission, final user visual acceptance and representative L3
acceptance remain open.

| Capability | Status | Where it belongs |
| --- | --- | --- |
| IK solving, append-transform evaluation at import | unsupported by design | never the importer ([DESIGN_POLICY.md §2.2](../design/DESIGN_POLICY.md#22-the-static-importer-boundary)) |
| IK solving, append-transform evaluation of a bound motion (`mmdControl`) | supported, outside the importer — see [Control evaluation](#control-evaluation-mmdcontrol) | [MOTION §11](../design/MOTION_CONTRACT.md#11-evaluating-the-control-rig) |
| A VMD as a `MotionClip`, and a PMX model as a retarget target (`SkeletonDescriptor`, humanoid `RetargetMap`) | supported — see [Shared-motion adapters](#shared-motion-adapters-mmdskeletonadapter-mmdmotionadapter) | [MOTION §10](../design/MOTION_CONTRACT.md#10-normalizing-into-the-shared-motion-core), [§12](../design/MOTION_CONTRACT.md#12-the-humanoid-role-table) |
| Retargeting, recording, `UsdSkelAnimation` authoring of motion | unsupported by design | `usd-motion-plugins` ([MOTION §10.6](../design/MOTION_CONTRACT.md#106-what-this-repository-does-not-do-with-the-result)) |
| Generic physics simulation | unsupported here by design | `usd-physics-plugins` backend; runtime schedules execution; see [physics capabilities](#physics-preservation-and-execution) |
| `MmdMaterialAPI` → Hydra data (`mmdImaging`) | supported: `mmd/material/<field>` and `mmd/drawOrder` on the Hydra material prim, per-field invalidation, time-sampled values followed; checked over a hand-authored stage and every importer fixture with materials | [MATERIAL §12.1](../design/MATERIAL_POLICY.md#121-the-hydra-view-mmdimaging) |
| Toon rendering | unsupported by design | `hydra-toon`; this repository supplies the schema and imaging adapter, not the renderer |
| Opening a `.vmd` as a stage (`usdVmdFileFormat`) | — (waits for MOT-O2) | [MOTION §2](../design/MOTION_CONTRACT.md#2-components-and-boundaries) |
| VMD playback | unsupported by design | a runtime scheduling `mmdControl` (Phase 8) |
| VMD bake to `UsdSkelAnimation` | supported through the evaluated clip/shared-core path above; no standalone bake CLI | `mmdControl` evaluates, the adapters normalize, the shared core authors ([MOTION §8.2](../design/MOTION_CONTRACT.md#82-a-bake-is-not-a-data-conversion)) |
| PMD | — (not planned) | `mmdPmd`, if ever |
| PMX / VMD writing | — (not planned) | [DESIGN_POLICY.md §2.4](../design/DESIGN_POLICY.md#24-reader-first) |
| Export to a format that is not USD (FBX, glTF, …) | unsupported by design | [PACKAGING §15](../design/PACKAGING_POLICY.md#15-non-goals) |

## See also

- [DIAGNOSTICS.md](DIAGNOSTICS.md) — the codes an approximation or refusal raises.
- [SOURCE_MAPPING.md](SOURCE_MAPPING.md) — where each PMX field lands on the stage.
- [current.md](../roadmap/current.md) — active priorities and external blockers.
