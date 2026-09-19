# Capability matrix

What the current code supports, feature by feature. This page states **facts
about the tree**, not plans; a status here changes only in the change that adds
the fixture proving it.

**As of 2026-09-19 the tree holds Phases 0–7 and the first part of Phase 9:** `.pmx` is registered, every
table of a PMX 2.0 or 2.1 file is parsed and validated (by `mmdPmx`, reported
by `mmd_inspect`), canonicalized (by `mmdModel`), and authored as the
canonical stage — mesh, UVs, material prims and subsets, skeleton and
skinning. Phase 3 adds MMD material semantics plus unlit-compatible preview
and MaterialX graphs, and Phase 4 every morph: vertex morphs as blend shapes,
every other type preserved declaratively. Phase 5 preserves every bone's
control semantics under `/Asset/rig` — IK chains, append relations, axes,
external parents, tails, transform layers — with nothing solved, and Phase 6
every rigid body and joint under `/Asset/physics`, as `UsdPhysics` where it
matches and `mmd:physics:*` throughout, with nothing simulated. Phase 7 reads
VMD motion (`motionVmd`, reported by `vmd_inspect`) and binds it to a
canonical model by MMD's name rule (`mmdMotionBinding`), baking nothing.
Phase 9's `mmdControl` evaluates a bound motion at an explicit time over the
control rig — Bézier curves, bone and group morphs, appends, IK — into
deformation-joint transforms, outside the importer.
The *intended* column is the
claim the design makes for the first substantial release
([DESIGN_POLICY.md §14.1](../design/DESIGN_POLICY.md#141-first-substantial-release--definition-of-done));
it is listed so reviewers can see the target, and it is not a support claim.

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
backed by a synthetic rig with a known answer in `mmdControl_unit`, and every
rule by `mmdControl_robustness`.

| Capability | Current | Contract |
| --- | :---: | --- |
| Bone tracks sampled on their Bézier curves; morph tracks linearly; IK and visibility as steps | supported | [MOTION §11.2](../design/MOTION_CONTRACT.md#112-sampling-the-tracks) |
| Bone morphs, directly and through nested group morphs, evaluated into the pose | supported | [MOTION §11.3](../design/MOTION_CONTRACT.md#113-morphs) |
| Every other bound morph as a channel with its sampled weight | supported | [MOTION §11.3](../design/MOTION_CONTRACT.md#113-morphs) |
| MMD's evaluation order: after-physics flag, transform layer, source index | supported | [MOTION §11.5](../design/MOTION_CONTRACT.md#115-evaluation-order) |
| Rotation and translation appends, negative ratios, chains of appends, appends from IK links | supported | [MOTION §11.6](../design/MOTION_CONTRACT.md#116-appends) |
| Local appends | approximated (evaluated as global, reported) | [MOTION §11.6](../design/MOTION_CONTRACT.md#116-appends) |
| IK by cyclic coordinate descent: angle limit, plane links, Euler-limited links, the IK-enable track | supported | [MOTION §11.7](../design/MOTION_CONTRACT.md#117-ik) |
| IK matching MMD's own playback within a distance | unverified (MOT-O9) | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) |
| External parents | unsupported (ignored, reported) | [MOTION §11.8](../design/MOTION_CONTRACT.md#118-diagnostics) |
| Physics before after-physics bones | unsupported by design (nothing is simulated) | [MOTION §10.3](../design/MOTION_CONTRACT.md#103-evaluation) |
| The same inputs give the same bits, whatever was evaluated before | supported | [MOTION §11.1](../design/MOTION_CONTRACT.md#111-the-evaluator) |

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
| Display frames | — (parsed; contract v1 authors none) | unsupported (parsed, not authored) | 1 | [PMX §11](../design/PMX_CONTRACT.md#11-display-frames) |
| Rigid bodies: shape, size, rest frame, mass, kinematic mode (`UsdPhysics`) | supported | supported | 6 | [STAGE §13.1](../design/STAGE_CONTRACT.md#131-rigid-bodies) |
| Rigid bodies: bone, collision group and mask, damping, restitution, friction, physics mode | preserved | preserved | 6 | [STAGE §13.1](../design/STAGE_CONTRACT.md#131-rigid-bodies) |
| Spring 6-DOF and 6-DOF joints: bodies, frames, limits (`UsdPhysics`) | supported | supported | 6 | [STAGE §13.2](../design/STAGE_CONTRACT.md#132-joints) |
| Joint springs, free axes, every joint value | preserved | preserved | 6 | [STAGE §13.2](../design/STAGE_CONTRACT.md#132-joints) |
| PMX 2.1 point-to-point, cone-twist, slider, hinge joints (no `UsdPhysics` joint) | preserved | preserved | 6 | [STAGE §13.2](../design/STAGE_CONTRACT.md#132-joints) |
| Undefined shape, mode or joint type; a joint whose body is missing | supported | supported | 6 | [STAGE §13.3](../design/STAGE_CONTRACT.md#133-repairs) |
| Soft bodies (2.1) | unsupported (parsed, not authored) | unsupported (parsed, not authored) | 1 | [PMX §12](../design/PMX_CONTRACT.md#12-soft-bodies-21) |

## Outside the PMX importer

| Capability | Status | Where it belongs |
| --- | --- | --- |
| IK solving, append-transform evaluation at import | unsupported by design | never the importer ([DESIGN_POLICY.md §2.2](../design/DESIGN_POLICY.md#22-the-static-importer-boundary)) |
| IK solving, append-transform evaluation of a bound motion (`mmdControl`) | supported, outside the importer — see [Control evaluation](#control-evaluation-mmdcontrol) | [MOTION §11](../design/MOTION_CONTRACT.md#11-evaluating-the-control-rig) |
| A VMD as a `MotionClip`, and a PMX model as a retarget target (`SkeletonDescriptor`, humanoid `RetargetMap`) (`mmdMotionAdapter`) | — (Phase 9; the role table is decided, the adapter waits for `usd-motion-plugins`) | [MOTION §10](../design/MOTION_CONTRACT.md#10-normalizing-into-the-shared-motion-core), [§12](../design/MOTION_CONTRACT.md#12-the-humanoid-role-table) |
| Retargeting, recording, `UsdSkelAnimation` authoring of motion | unsupported by design | `usd-motion-plugins` ([MOTION §10.6](../design/MOTION_CONTRACT.md#106-what-this-repository-does-not-do-with-the-result)) |
| Physics simulation | unsupported by design | `usd-stage-runner` or another runtime |
| Toon rendering | unsupported by design | `hydra-toon` |
| Opening a `.vmd` as a stage (`usdVmdFileFormat`) | — (waits for MOT-O2) | [MOTION §2](../design/MOTION_CONTRACT.md#2-components-and-boundaries) |
| VMD playback | unsupported by design | a runtime scheduling `mmdControl` (Phase 8) |
| VMD bake | — (Phase 9; `mmdControl` exists, the shared core's authoring waits for `usd-motion-plugins`) | `mmdControl` evaluates, the shared core authors ([MOTION §8.2](../design/MOTION_CONTRACT.md#82-a-bake-is-not-a-data-conversion)) |
| PMD | — (not planned) | `mmdPmd`, if ever |
| PMX / VMD writing | — (not planned) | [DESIGN_POLICY.md §2.4](../design/DESIGN_POLICY.md#24-reader-first) |

## See also

- [DIAGNOSTICS.md](DIAGNOSTICS.md) — the codes an approximation or refusal raises.
- [SOURCE_MAPPING.md](SOURCE_MAPPING.md) — where each PMX field lands on the stage.
- [roadmap/](../roadmap/README.md) — when each Phase is scheduled.
