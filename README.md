# OpenUSD MMD Plugins

OpenUSD plugins for [MikuMikuDance](https://sites.google.com/view/vpvp/) (MMD)
assets: PMX models first, VMD motion later.

> **Status: PMX parser.** Every table of a PMX 2.0 or 2.1 file is parsed and
> validated, and `mmd_inspect` reports what a file contains.
> `Usd.Stage.Open("model.pmx")` works, and authors an empty `/Asset` with the
> stage metadata and the parser's diagnostics: geometry, materials and the
> skeleton are Phase 2. Most of what is described below is still design; the
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

The importer authors data only. It never solves IK, evaluates bone constraints
or morphs, simulates physics, performs toon shading, or plays motion — those
belong to runtimes and renderers that read the stage. The same bytes always
produce the same stage.

## Components

| Component | Kind | Role | State |
| --- | --- | --- | --- |
| `mmdPmx` | plain C++ library | PMX 2.0/2.1 syntax, text decoding, validation — no OpenUSD | reads every table |
| `mmdModel` | plain C++ library | canonical MMD semantics and the single source → USD coordinate conversion — no OpenUSD | Phase 2 |
| `usdMmdFileFormat` | OpenUSD `SdfFileFormat` bundle | `.pmx` → a USD stage | registers `.pmx`, authors `/Asset` |
| `mmd_inspect` | CLI | what a PMX contains, without USD | exists ([guide](docs/guides/inspecting.md)) |
| `motionVmd` | plain C++ library | VMD syntax, extraction-ready for the shared motion architecture | Phase 7 |

`mmdSchema` exists only if an MMD API schema passes the
[admission test](docs/design/DESIGN_POLICY.md#6-the-schema-admission-test); the
first stage uses standard schemas only. Identities and dependency directions
are fixed in [docs/architecture/WORKSPACE.md](docs/architecture/WORKSPACE.md).

## What the importer will author

```text
/Asset                    UsdSkelRoot, kind = component, defaultPrim; Y-up, meters
  geo/Mesh                UsdGeomMesh, skinned; one GeomSubset per material
  mtl/<material>          UsdShadeMaterial: MMD semantics + /preview and /mtlx graphs
  skel/Skeleton           UsdSkelSkeleton in canonical joint order
  morph/<morph>           UsdSkelBlendShape for vertex morphs; others preserved declaratively
  rig/                    MMD control semantics (IK, append transforms) — later
  physics/                rigid bodies and joints, preserved, never simulated — later
```

Japanese names are preserved exactly, beside stable ASCII identifiers used for
paths, and Japanese texture filenames resolve. The full contract is
[docs/design/STAGE_CONTRACT.md](docs/design/STAGE_CONTRACT.md).

## Building

The workspace builds both with
[OpenStrata](https://github.com/animu-sphere/open-strata) (`ost`) and with
plain CMake, against OpenUSD 26.08 exactly
([WORKSPACE.md §5](docs/architecture/WORKSPACE.md#5-build-modes)):

```powershell
$env:USD_INSTALL_ROOT = "<OpenUSD 26.08 install>"
cmake --preset windows-msvc
cmake --build --preset windows-release
ctest --preset windows-release
```

or `ost build` and `ost test`. [docs/guides/building.md](docs/guides/building.md)
has the full set — the bundle on its own, packaging, and the
installed-consumer lane — every command in it run.

## Documentation

| | |
| --- | --- |
| [docs/design/](docs/design/) | What the importer authors and why — start with [DESIGN_POLICY.md](docs/design/DESIGN_POLICY.md) |
| [docs/architecture/](docs/architecture/) | The binding workspace contract, external dependencies, and installed packages |
| [docs/guides/](docs/guides/) | How to build, test and package, and how to inspect a PMX |
| [docs/reference/](docs/reference/) | What is implemented, diagnostics, and where each PMX field lands |
| [docs/roadmap/](docs/roadmap/) | What is planned next (incomplete work only) |
| [docs/contributing/](docs/contributing/) | How the documentation is maintained |

Changes are recorded in the [changelog](CHANGELOG.md).

## License

Apache-2.0 — see [LICENSE](LICENSE).

MMD models, motions and textures are created and distributed by their authors
under individual terms of use. None is included in this repository: test
fixtures are generated by code committed here, and a model used locally for
evaluation is never committed or redistributed.
