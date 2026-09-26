# OpenUSD MMD Character Plugins

[![License: Apache-2.0](https://img.shields.io/github/license/animu-sphere/usd-mmd-plugins?label=license)](LICENSE)
[![CI](https://github.com/animu-sphere/usd-mmd-plugins/actions/workflows/ost-source-ci.yml/badge.svg)](https://github.com/animu-sphere/usd-mmd-plugins/actions/workflows/ost-source-ci.yml)
[![OpenUSD 26.08](https://img.shields.io/badge/OpenUSD-26.08-1f6feb)](docs/architecture/DEPENDENCIES.md#1-openusd)

OpenUSD plugins for [MikuMikuDance](https://sites.google.com/view/vpvp/) (MMD)
assets: PMX models, and VMD motion bound to them.

> **Status: Phases 0–7.** `Usd.Stage.Open("model.pmx")` authors the model —
> mesh, UVs, materials with `preview` and `mtlx` graphs, skeleton and
> skinning, morphs, and the rig and physics preserved without being solved or
> simulated — Y-up, in meters, Japanese names preserved beside ASCII
> identifiers, and `mmd_inspect` reports what a file contains. A VMD is read
> without a model and reported by `vmd_inspect`, and bound to a model by MMD's
> own name rule. Phase 9 now evaluates MMD's IK and append transforms over a
> bound motion, and its motion and skeleton adapters hand evaluated clips and
> PMX rig descriptions to `usd-motion-plugins`, which retargets and authors
> `UsdSkelAnimation`. End-to-end retarget and authoring acceptance remains.
> Future physics execution consumes the existing static
> stage through `usd-physics-plugins`; the importer remains solver-free. The
> [capability matrix](docs/reference/CAPABILITY_MATRIX.md) is the only page
> that says what is implemented, and [the roadmap](docs/roadmap/current.md)
> what comes next.

`usd-mmd-plugins` is the MMD sibling of
[`usd-vrm-plugins`](https://github.com/animu-sphere/usd-vrm-plugins): the
same workspace discipline, the same plugin/library separation, the same
`/Asset`-rooted stage and the same static-importer boundary, so that both can
be composed by `usd-avatar-runtime` without special cases. It is a format
adapter — not an MMD application, an editor, or a renderer.

## The central rule

> **Parse MMD, normalize it once, author conventional OpenUSD, and keep runtime
> evaluation and rendering outside the file-format importer.**

```text
PMX bytes ─→ mmdPmx ─→ mmdModel ─→ usdMmdFileFormat ─→ USD stage
             syntax    canonical    SdfFileFormat        ─→ renderers, runtimes, tools
                       semantics
```

```text
VMD bytes ─→ motionVmd ─→ mmdMotionBinding (+ mmdModel) ─→ a bound motion
             syntax,      by source name, in the model's
             tracks       basis
          ─→ mmdControl ─→ mmdMotionAdapter ─→ MotionClip ─→ usd-motion-plugins
             IK, append,                                    retarget, record, UsdSkelAnimation
             bone morphs

PMX skeleton ─→ mmdSkeletonAdapter ─→ SkeletonDescriptor / RetargetMap
```

The importer authors data only. It never solves IK, evaluates bone constraints
or morphs, simulates physics, performs toon shading, or plays motion — those
belong to runtimes and renderers that read the stage, and MMD's IK and append
semantics to `mmdControl`, a library a runtime schedules. The same bytes always
produce the same stage.

Generic motion — poses, clips, retargeting, recording — belongs to
`usd-motion-plugins`, which this repository depends on and never the reverse; VMD and everything
that needs MMD to be understood stay here.

Generic physics — world construction, stepping, queries and backend ownership
— belongs to `usd-physics-plugins`. This repository preserves PMX physics and
will own only the MMD-specific bone/body coupling adapter; the integration
contract is [docs/design/PHYSICS_INTEGRATION.md](docs/design/PHYSICS_INTEGRATION.md).

## Components

| Component | Kind | Role | State |
| --- | --- | --- | --- |
| `mmdPmx` | plain C++ library | PMX 2.0/2.1 syntax, text decoding, validation — no OpenUSD | reads every table |
| `mmdModel` | plain C++ library | canonical MMD semantics and the single source → USD coordinate conversion — no OpenUSD | identifiers, joint order, skinning, mesh, textures |
| `usdMmdFileFormat` | OpenUSD `SdfFileFormat` bundle | `.pmx` → a USD stage | authors `geo`, `mtl`, `skel` ([guide](docs/guides/opening.md)) |
| `mmd_inspect` | CLI | what a PMX contains, without USD | exists ([guide](docs/guides/inspecting.md)) |
| `motionVmd` | plain C++ library | VMD syntax, CP932 names and tracks — no dependency at all | reads every section |
| `mmdMotionBinding` | plain C++ library | binds a VMD motion to a canonical model by source name, in the model's basis — no OpenUSD, nothing evaluated | exists |
| `mmdControl` | plain C++ library | evaluates a bound motion at an explicit time over MMD's control rig — Bézier curves, bone morphs, appends, IK — into deformation-joint transforms; no OpenUSD, scheduled by a runtime | exists |
| `mmdSkeletonAdapter` | plain C++ library | exposes the PMX skeleton, source rest and versioned humanoid map to `usd-motion-plugins`; no retarget algorithm | exists |
| `mmdMotionAdapter` | plain C++ library | turns fully evaluated MMD poses into `MotionClip`; no target-avatar knowledge | exists |
| `vmd_inspect` | CLI | what a VMD contains, without a model or USD | exists ([guide](docs/guides/inspecting.md)) |
| `mmd_export` | CLI | the imported stage written out as conventional OpenUSD; a self-contained USDZ that opens without these plugins, BMP and TGA converted to PNG | `.usdz` ([guide](docs/guides/exporting.md)) |

`MmdMaterialAPI` passed the
[admission test](docs/design/DESIGN_POLICY.md#6-the-schema-admission-test) on
2026-09-22 for the `hydra-toon` consumer. `mmdSchema` holds it, and since
stage-contract v2 the importer applies it to every material, with the values
as Material interface inputs `inputs:mmd:material:*` that the fallback graphs
connect to (v1 authored schema-less `mmd:material:*` attributes). Phase 8
also adds the independent `mmdImaging` bridge. Identities and dependency
directions are fixed in
[docs/architecture/WORKSPACE.md](docs/architecture/WORKSPACE.md).

## What the importer will author

```text
/Asset                    UsdSkelRoot, kind = component, defaultPrim; Y-up, meters
  geo/Mesh                UsdGeomMesh, skinned; one GeomSubset per material
  mtl/<material>          UsdShadeMaterial + MmdMaterialAPI: MMD semantics as inputs,
                          and the /preview and /mtlx graphs connected to them
  skel/Skeleton           UsdSkelSkeleton in canonical joint order
  morph/<morph>           UsdSkelBlendShape for vertex morphs; others preserved declaratively
  rig/Bones, rig/ik/<bone> MMD control semantics: IK chains, append relations, axes — never solved
  physics/                rigid bodies and joints: UsdPhysics + mmd:physics:*, never simulated
```

Japanese names are preserved exactly, beside stable ASCII identifiers used for
paths, and Japanese texture filenames resolve. The full contract is
[docs/design/STAGE_CONTRACT.md](docs/design/STAGE_CONTRACT.md).

## Building

The workspace builds both with
[OpenStrata](https://github.com/animu-sphere/open-strata) (`ost`) and with
plain CMake, against OpenUSD 26.08 exactly and `usd-motion-plugins`' released
packages, all found as installed packages on `CMAKE_PREFIX_PATH`
([WORKSPACE.md §5](docs/architecture/WORKSPACE.md#5-build-modes)):

```powershell
$env:CMAKE_PREFIX_PATH = "<OpenUSD 26.08>;<motionCore>;<motionRetarget>;<motionUsd>"
cmake --preset windows-msvc
cmake --build --preset windows-release
ctest --preset windows-release
```

or `ost build` and `ost test`. [docs/guides/building.md](docs/guides/building.md)
has the full set — one component on its own, packaging, and the
installed-consumer lane — every command in it run.

## Documentation

| | |
| --- | --- |
| [docs/design/](docs/design/) | What the importer authors and why — start with [DESIGN_POLICY.md](docs/design/DESIGN_POLICY.md) |
| [docs/architecture/](docs/architecture/) | The binding workspace contract, external dependencies, and installed packages |
| [docs/guides/](docs/guides/) | How to build, test and package, how to open a PMX as a stage, and how to inspect a PMX or a VMD |
| [docs/reference/](docs/reference/) | What is implemented, diagnostics, and where each PMX field lands |
| [docs/roadmap/](docs/roadmap/) | What is planned next (incomplete work only) |
| [docs/contributing/](docs/contributing/) | How the documentation is maintained |

Changes are recorded in the [changelog](CHANGELOG.md).

## Contributing

Small fixes, documentation, tests and design discussion are welcome. Start
with [CONTRIBUTING.md](CONTRIBUTING.md), and see the [Code of Conduct](CODE_OF_CONDUCT.md)
for the community expectations. Please report security issues privately using
the [Security Policy](SECURITY.md).

## License

Apache-2.0 — see [LICENSE](LICENSE).

MMD models, motions and textures are created and distributed by their authors
under individual terms of use. None is included in this repository: test
fixtures are generated by code committed here, and a model used locally for
evaluation is never committed or redistributed.
