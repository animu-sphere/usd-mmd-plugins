# OpenUSD MMD Character Plugins

[![License: Apache-2.0](https://img.shields.io/github/license/animu-sphere/usd-mmd-plugins?label=license)](LICENSE)
[![CI](https://github.com/animu-sphere/usd-mmd-plugins/actions/workflows/ost-source-ci.yml/badge.svg)](https://github.com/animu-sphere/usd-mmd-plugins/actions/workflows/ost-source-ci.yml)

OpenUSD plugins for [MikuMikuDance](https://sites.google.com/view/vpvp/) (MMD)
assets: PMX models and VMD motion bound to them. Open a PMX as an
`/Asset`-rooted stage, inspect models and motions, or export a self-contained
USDZ. See the [capability matrix](docs/reference/CAPABILITY_MATRIX.md) for
support and limitations.

The importer parses MMD, normalizes coordinates once, and authors conventional
OpenUSD. Motion evaluation lives in MMD libraries outside the importer;
generic motion and physics come from `usd-motion-plugins` and
`usd-physics-plugins`. `usd-avatar-runtime` composes avatar evaluation and owns
its order and shared physics barriers; `usd-stage-runner` supplies the host
loop, and `hydra-toon` renders the result.

```text
PMX → mmdPmx → mmdModel → usdMmdFileFormat → USD stage
VMD → motionVmd → mmdMotionBinding → mmdControl → mmdMotionAdapter → MotionClip
PMX skeleton → mmdSkeletonAdapter → shared skeleton and retarget map
```

The stage uses Y-up and meters, preserves Japanese source names beside stable
ASCII identifiers, and carries MMD rig and physics metadata. Opening a model
requires no physics runtime. Runtime ownership and the physics acceptance
policy are defined in the [physics integration contract](docs/design/PHYSICS_INTEGRATION.md).

## Build and use

Dependencies are installed packages on `CMAKE_PREFIX_PATH`; the
[dependency contract](docs/architecture/DEPENDENCIES.md) links to the
authoritative pins and manifests.

```powershell
$env:CMAKE_PREFIX_PATH = "<OpenUSD>;<motionCore>;<motionRetarget>;<motionSource>;<motionUsd>"
cmake --preset windows-msvc
cmake --build --preset windows-release
ctest --preset windows-release
```

OpenStrata users can run `ost build` and `ost test`. Follow the
[build guide](docs/guides/building.md) for setup and installed packages, then:

- [Open a PMX](docs/guides/opening.md) with `Usd.Stage.Open("model.pmx")`.
- [Inspect a PMX or VMD](docs/guides/inspecting.md) with `mmd_inspect` or `vmd_inspect`.
- [Export USDZ](docs/guides/exporting.md) with `mmd_export`, including portable archive paths.

## Documentation

| Entry | Purpose |
| --- | --- |
| [Documentation index](docs/README.md) | Document owners and navigation |
| [Capabilities](docs/reference/CAPABILITY_MATRIX.md) | Current support, approximations and limitations |
| [Current roadmap](docs/roadmap/current.md) | Active priorities, owners and exit gates |
| [Stage contract](docs/design/STAGE_CONTRACT.md) | Authored hierarchy, coordinates and metadata |
| [Design policy](docs/design/DESIGN_POLICY.md) | Principles and scope definitions |
| [Workspace](docs/architecture/WORKSPACE.md) | Component identities and dependency directions |
| [Release records](docs/releases/README.md) · [Changelog](CHANGELOG.md) | Shipped behavior |
| [Documentation guidelines](docs/contributing/documentation.md) | Where changes belong |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md), the [Code of Conduct](CODE_OF_CONDUCT.md)
and the [Security Policy](SECURITY.md).

## License

Apache-2.0 — see [LICENSE](LICENSE).

MMD models, motions and textures carry their authors' individual terms.
Test fixtures are generated from code in this repository; third-party assets
require explicit redistribution permission before inclusion.
