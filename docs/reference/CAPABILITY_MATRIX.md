# Capability matrix

What the current code supports, feature by feature. This page states **facts
about the tree**, not plans; a status here changes only in the change that adds
the fixture proving it.

**As of 2026-09-15 the tree holds Phases 0 and 1:** `.pmx` is registered,
every table of a PMX 2.0 or 2.1 file is parsed and validated (by `mmdPmx`,
reported by `mmd_inspect`), and the stage metadata and `/Asset` are authored.
Nothing is authored from the tables yet, so parsing is claimed in its own
section below and the import rows stay `—` until the Phase that authors each
one. The *intended* column is the claim the design makes for the
first substantial release
([DESIGN_POLICY.md §14.1](../design/DESIGN_POLICY.md#141-first-substantial-release--definition-of-done));
it is listed so reviewers can see the target, and it is not a support claim.

The fixtures behind the current claims are in
[plugins/usdMmdFileFormat/tests/fixtures/](../../plugins/usdMmdFileFormat/tests/fixtures/),
and `fixtures.json` there says what each must do.

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

## PMX model import

| Capability | Current | Intended | Phase | Contract |
| --- | :---: | --- | :---: | --- |
| `.pmx` registration, `Usd.Stage.Open` | supported | supported | 0 | [STAGE §1](../design/STAGE_CONTRACT.md#1-scope) |
| Non-ASCII file paths (read through `Ar`) | supported | supported | 0 | [TEXT §4](../design/TEXT_ENCODING_POLICY.md#4-no-locale-anywhere) |
| PMX 2.0 and 2.1 open; a malformed file fails with its fatal code | supported | supported | 0, 1 | [PMX §15](../design/PMX_CONTRACT.md#15-fatal-versus-recoverable) |
| Recoverable parser diagnostics recorded on the stage | supported | supported | 1 | [DIAGNOSTICS §4](DIAGNOSTICS.md#4-surfacing) |
| Stage metadata (`/Asset`, Y-up, meters, contract version) | supported | supported | 0, 2 | [STAGE §2](../design/STAGE_CONTRACT.md#2-contract-version), [§4](../design/STAGE_CONTRACT.md#4-prim-hierarchy) |
| Mesh: points, faces, normals | — | supported | 2 | [STAGE §8](../design/STAGE_CONTRACT.md#8-geometry) |
| Primary UV (`primvars:st`) | — | supported | 2 | [STAGE §8.3](../design/STAGE_CONTRACT.md#83-uvs) |
| Additional UV 1–4 | — | preserved | 2 | [STAGE §8.3](../design/STAGE_CONTRACT.md#83-uvs) |
| Material face ranges (`GeomSubset`) | — | supported | 2 | [STAGE §8.1](../design/STAGE_CONTRACT.md#81-one-mesh-material-subsets) |
| Per-material culling | — | approximated + preserved | 2 | [STAGE §8.5](../design/STAGE_CONTRACT.md#85-double-sidedness) |
| Skeleton (`UsdSkelSkeleton`) | — | supported | 2 | [STAGE §9](../design/STAGE_CONTRACT.md#9-skeleton-and-skinning) |
| BDEF1 / BDEF2 / BDEF4 | — | supported | 2 | [STAGE §9.4](../design/STAGE_CONTRACT.md#94-skinning) |
| SDEF | — | approximated + preserved | 2 | [STAGE §9.4](../design/STAGE_CONTRACT.md#94-skinning) |
| QDEF | — | unverified | 2 | [PMX §5](../design/PMX_CONTRACT.md#5-vertices-and-deform) |
| Japanese names preserved | — | supported | 2 | [TEXT §5](../design/TEXT_ENCODING_POLICY.md#5-identity-versus-display) |
| Stable ASCII identifiers | — | supported | 2 | [TEXT §6](../design/TEXT_ENCODING_POLICY.md#6-stable-identifiers) |
| Japanese texture filenames | — | supported | 2 | [TEXT §7](../design/TEXT_ENCODING_POLICY.md#7-texture-paths) |
| Unsafe texture paths | — | unsupported (refused, preserved) | 2 | [TEXT §7.2](../design/TEXT_ENCODING_POLICY.md#72-unsafe-paths) |
| `UsdPreviewSurface` | — | approximated | 3 | [MATERIAL §5](../design/MATERIAL_POLICY.md#5-usdpreviewsurface-realization) |
| MaterialX `gltf_pbr` | — | approximated | 3 | [MATERIAL §6](../design/MATERIAL_POLICY.md#6-materialx-gltf_pbr-realization) |
| Native MMD material semantics | — | preserved | 3 | [MATERIAL §4](../design/MATERIAL_POLICY.md#4-canonical-material-semantics) |
| Sphere textures | — | preserved | 3 | [MATERIAL §8](../design/MATERIAL_POLICY.md#8-sphere-textures) |
| Toon ramps (individual and shared) | — | preserved | 3 | [MATERIAL §7](../design/MATERIAL_POLICY.md#7-toon-ramps) |
| Edge / outline | — | preserved | 3 | [MATERIAL §9](../design/MATERIAL_POLICY.md#9-edges) |
| Vertex morph | — | supported | 4 | [STAGE §11](../design/STAGE_CONTRACT.md#11-morphs) |
| Group, flip morph | — | preserved | 4 | [STAGE §11](../design/STAGE_CONTRACT.md#11-morphs) |
| Bone morph | — | preserved | 4 | [STAGE §11](../design/STAGE_CONTRACT.md#11-morphs) |
| UV and additional-UV morph | — | preserved | 4 | [STAGE §11](../design/STAGE_CONTRACT.md#11-morphs) |
| Material morph | — | preserved | 4 | [MATERIAL §11](../design/MATERIAL_POLICY.md#11-material-morphs) |
| Impulse morph | — | preserved | 4 | [STAGE §11](../design/STAGE_CONTRACT.md#11-morphs) |
| IK chains | — | preserved | 5 | [STAGE §12](../design/STAGE_CONTRACT.md#12-control-rig--reserved) |
| Append transforms, fixed and local axes, external parent | — | preserved | 5 | [STAGE §12](../design/STAGE_CONTRACT.md#12-control-rig--reserved) |
| Display frames | — (parsed; contract v1 authors none) | unsupported (parsed, not authored) | 1 | [PMX §11](../design/PMX_CONTRACT.md#11-display-frames) |
| Rigid bodies and joints | — | preserved | 6 | [STAGE §13](../design/STAGE_CONTRACT.md#13-physics--reserved) |
| Soft bodies (2.1) | unsupported (parsed, not authored) | unsupported (parsed, not authored) | 1 | [PMX §12](../design/PMX_CONTRACT.md#12-soft-bodies-21) |

## Outside the PMX importer

| Capability | Status | Where it belongs |
| --- | --- | --- |
| IK solving, append-transform evaluation | unsupported by design | a runtime ([DESIGN_POLICY.md §2.2](../design/DESIGN_POLICY.md#22-the-static-importer-boundary)) |
| Physics simulation | unsupported by design | `usd-stage-runner` or another runtime |
| Toon rendering | unsupported by design | `hydra-toon` |
| VMD parsing | — (Phase 7) | `motionVmd` ([MOTION_CONTRACT.md](../design/MOTION_CONTRACT.md)) |
| VMD playback / bake | outside the PMX importer | a motion or avatar runtime ([MOTION §8.2](../design/MOTION_CONTRACT.md#82-a-bake-is-not-a-data-conversion)) |
| PMD | — (not planned) | `mmdPmd`, if ever |
| PMX / VMD writing | — (not planned) | [DESIGN_POLICY.md §2.4](../design/DESIGN_POLICY.md#24-reader-first) |

## See also

- [DIAGNOSTICS.md](DIAGNOSTICS.md) — the codes an approximation or refusal raises.
- [SOURCE_MAPPING.md](SOURCE_MAPPING.md) — where each PMX field lands on the stage.
- [roadmap/](../roadmap/README.md) — when each Phase is scheduled.
