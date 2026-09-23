# External dependencies

What `usd-mmd-plugins` builds against, what it refuses to depend on, and the
test a new dependency has to pass. Edges *between* this repository's own
components are [WORKSPACE.md §2](WORKSPACE.md#2-dependency-directions)'s.

Status (2026-09-21): the Phase 0 decisions are closed, and every value below
is what the workspace builds with. The OpenUSD pin is enforced at configure
time. §6, `usd-motion-plugins`, was added on 2026-09-17 and is planned: no
component links it yet. §7 records the proposed optional runtime edge to
`usd-physics-plugins`; the static import path never takes it.

## 1. OpenUSD

| | |
| --- | --- |
| Pin | OpenUSD **26.08**, exactly (`PXR_VERSION` 2608), enforced by [cmake/UsdMmdOpenUsd.cmake](../../cmake/UsdMmdOpenUsd.cmake) for `ost` and plain-CMake builds alike and declared as `runtime.openusd: "==26.08"` in the bundle manifest; the release the rest of the ecosystem pins (`usd-vrm-plugins` too), because `usd-avatar-runtime` composes every plugin into one OpenUSD process |
| Used by | `usdMmdFileFormat`; `mmdMotionAdapter` and `mmdSkeletonAdapter` use only the foundation types (`gf`, `tf`, `vt`) exposed through the shared motion packages, and no stage (§6) |
| Modules | linked today: `arch`, `tf`, `gf`, `vt`, `ar`, `sdf`, `usd`, `usdGeom`, `usdPhysics`, `usdShade`, `usdSkel`, `kind` (`usdShade` and `usdSkel` since Phase 2, `usdPhysics` since Phase 6) |
| Not used | OpenExec, Hydra, `usdImaging` — nothing is evaluated or rendered here, so unlike `usd-vrm-plugins`' pin module this one probes for no OpenExec |
| CI runtimes | the OpenUSD 26.08 leaves of OpenStrata's runtime matrix, the same digests `usd-vrm-plugins` pins ([openstrata.ci.yaml](../../openstrata.ci.yaml)) |

The pin is exact rather than a range for the same reason as in
`usd-vrm-plugins`: a plugin built against one OpenUSD release is not loadable
in another, so a range would be a claim no artifact could keep.

**MaterialX is not linked.** The importer authors `UsdShadeShader` prims whose
`info:id` names MaterialX nodes (`ND_gltf_pbr_surfaceshader`, …); it never
calls the MaterialX library. The MaterialX document version it declares
(`config:mtlx:version = "1.39"`) is the one the pinned OpenUSD ships.

## 2. Toolchain

| | |
| --- | --- |
| Language | C++20 (the public parser API takes `std::span`) |
| Build | CMake **3.22** or later, the minimum `usd-vrm-plugins` declares; `CMakePresets.json` for the plain-CMake path, which takes every dependency from `CMAKE_PREFIX_PATH` as an installed package ([WORKSPACE.md §5](WORKSPACE.md#5-build-modes)) |
| Compilers | MSVC on Windows, Clang on macOS (arm64), GCC on Linux — the three hosted lanes `usd-vrm-plugins` runs |
| Windows flags | `/utf-8`, `NOMINMAX`, applied by `usdmmd_target_defaults()` in [cmake/UsdMmdTargets.cmake](../../cmake/UsdMmdTargets.cmake) ([WORKSPACE.md §5](WORKSPACE.md#5-build-modes)) |
| Python | the Python OpenUSD was built against — 3.13 for the 26.08 runtimes — for stage tests and tooling. The root project finds the interpreter *after* OpenUSD, so it inherits the one `pxrConfig.cmake` names |
| OpenStrata | `ost` **0.23.4**, pinned in `openstrata.ci.yaml`; required for digest-pinned external-library artifacts in root builds |
| Unit-test framework | **none**, as in `usd-vrm-plugins`: each suite is a plain executable that checks with `assert()`, compiled with `NDEBUG` undefined so Release builds still check, and registered with CTest |
| Sanitizers and fuzzing | Clang 18's AddressSanitizer, UndefinedBehaviorSanitizer and libFuzzer, from Ubuntu 24.04's packages, in [parser-sanitizers.yml](../../.github/workflows/parser-sanitizers.yml) only; the `USDMMD_SANITIZERS` and `USDMMD_BUILD_FUZZERS` options of [cmake/UsdMmdSanitizers.cmake](../../cmake/UsdMmdSanitizers.cmake) switch them on, and nothing shipped is built with them. Toolchain runtimes, not dependencies: no code is vendored and nothing links them outside that lane |

## 3. Refused dependencies

| Dependency | Refused because |
| --- | --- |
| Bullet, PhysX, Jolt, any physics engine directly | nothing in this repository owns backend objects; future simulation is reached through the optional shared `usd-physics-plugins` contract ([PHYSICS_INTEGRATION.md §8](../design/PHYSICS_INTEGRATION.md#8-dependency-policy)) |
| An image decoder in the importer | the importer never reads texture pixels; authoring stays independent of image content and of whether files exist ([TEXT_ENCODING_POLICY.md §7.3](../design/TEXT_ENCODING_POLICY.md#73-no-filesystem-access-while-authoring)) |
| ICU, `iconv`, OS code-page APIs | PMX text is UTF-8 or UTF-16LE, decoded by the parser; CP932 (for VMD, PMD) uses a table the project owns ([TEXT_ENCODING_POLICY.md §9](../design/TEXT_ENCODING_POLICY.md#9-pmd-and-vmd)) |
| A third-party PMX parser, by default | PMX is a bounded format; a purpose-built parser avoids inheriting an application's semantics ([DESIGN_POLICY.md §10](../design/DESIGN_POLICY.md#10-parser-strategy)) — adoption is possible only through §4 |
| OpenExec, Hydra render delegates, `hydra-toon`, `usd-stage-runner` | the importer neither evaluates nor renders ([WORKSPACE.md §2.2](WORKSPACE.md#22-forbidden-edges)); `mmdImaging` uses only OpenUSD's UsdImaging API, `mmdControl` evaluates as a plain library, and a runtime that wants it as an OpenExec node wraps it there |
| `motion-connectors`, device SDKs, network transports | live input reaches this repository only as the shared core's types, if at all ([WORKSPACE.md §2.2](WORKSPACE.md#22-forbidden-edges)) |
| A copy of any `usd-motion-plugins` algorithm | generic motion is consumed, never duplicated ([WORKSPACE.md §7](WORKSPACE.md#7-invariants), invariant 9) |

## 4. Third-party code

A third-party library is adopted only after a written review, in the pull
request that adds it, against:

- **license** — compatible with Apache-2.0 for redistribution (MIT, BSD,
  Apache-2.0, zlib and similar); copyleft is refused;
- **maintenance** — actively maintained, or small enough to own;
- **malformed-input safety** — bounded reads, no unchecked indexing;
- **encoding behavior** — no dependence on the process locale;
- **coverage** — for a format library, PMX 2.0 and 2.1 (or VMD) completely;
- **weight** — what it pulls in transitively;
- **fit** — it preserves source facts without imposing application or
  renderer semantics.

Adopted code is vendored under `third_party/<name>/` with its license file and
exact version, and listed in `THIRD_PARTY_NOTICES.md`.

## 5. Data that is not a dependency

- **MMD's shared toon textures** (`toon01.bmp`–`toon10.bmp`) belong to MMD and
  are not redistributed; a shared toon slot is authored as an index
  ([MATERIAL_POLICY.md §7](../design/MATERIAL_POLICY.md#7-toon-ramps)).
- **Distributed MMD models** carry their creators' individual terms of use and
  are never committed, vendored, or used as fixtures. Fixtures are generated
  ([DESIGN_POLICY.md §13](../design/DESIGN_POLICY.md#13-testing-policy)).
- **The CP932 mapping** `motionVmd` decodes and encodes with,
  [libs/motionVmd/src/Cp932Table.inc](../../libs/motionVmd/src/Cp932Table.inc),
  is generated by
  [libs/motionVmd/tools/generate_cp932_table.py](../../libs/motionVmd/tools/generate_cp932_table.py)
  from the `cp932` codec of Python's standard library, which implements
  Microsoft's published CP932 code page. The committed file holds only the
  correspondence between byte sequences and code points — no code of
  Python's — and nothing is vendored, so `THIRD_PARTY_NOTICES.md` lists
  nothing for it. Python is a build-time tool here, as it is for the
  fixtures.

## 6. usd-motion-plugins

The shared motion core: vendor- and avatar-format-neutral poses and clips,
humanoid joint semantics, sampling, retargeting, recording and the
`UsdSkelAnimation` bridge. v0.5.0 was published on 2026-09-20 with installable
`motionCore`, `motionRetarget` and `motionUsd`; the adapters and their skeletal
acceptance test consume per-target, digest-pinned OpenStrata artifacts.

| | |
| --- | --- |
| Packages | `motionCore` (`HumanJoint`, `MotionPose`, `RootMotion`, `MotionClip`), `motionRetarget` (`SkeletonDescriptor`, `RetargetMap`, `SourceRestPose`) and `motionUsd` (the standalone motion-stage writer/reader), each by `find_package(<name> CONFIG)` and linked as `<name>::<name>` |
| Used by | `mmdMotionAdapter` (`motionCore`), `mmdSkeletonAdapter` (`motionRetarget`, plus its required `motionCore` artifact closure) and the Phase 9 acceptance test (`motionRetarget`, `motionUsd`); later perhaps `usdVmdFileFormat` (MOT-O2) |
| Consumed as | an installed package, by `find_package` with a version range admitting the release it was verified against, the way siblings are ([WORKSPACE.md §5](WORKSPACE.md#5-build-modes)) |
| Version | `>=0.5,<0.6`, verified against v0.5.0 |
| OpenUSD | the same exact pin as §1 |
| Direction | one way: `usd-motion-plugins` never depends on this repository |

**Why a dependency and not a copy.** The motion policy places generic
motion in one repository so that VRM, MMD and live sources share one
retarget and one USD mapping; a private MMD copy would be the permanent
duplication that policy forbids (its §37). What stays here is what needs MMD
to be understood ([MOTION_CONTRACT.md §10](../design/MOTION_CONTRACT.md#10-normalizing-into-the-shared-motion-core)).

`motionUsd` is test-only today. Its pin lives on `mmdMotionAdapter`'s manifest
so a standalone or workspace test has the same artifact CI uses; the exported
`mmdMotionAdapter::mmdMotionAdapter` target and installed package do not link
or require it.

## 7. usd-physics-plugins

The future backend-neutral physics layer: scene construction from authored
`UsdPhysics`, simulation stepping, queries and backend integration. Proposed,
not linked; no package name or version is assumed until that repository
publishes its contract.

| | |
| --- | --- |
| Used by | a future MMD-specific bone/body coupling adapter, only after its first runtime consumer fixes the component identity in [WORKSPACE.md](WORKSPACE.md) |
| Not used by | `mmdPmx`, `mmdModel`, `usdMmdFileFormat`, `motionVmd`, `mmdMotionBinding`, `mmdControl`, `mmdMotionAdapter` or `mmdSkeletonAdapter` |
| Consumed as | an installed package with a declared compatible version, never sibling source, a submodule or vendored code |
| Backend direction | this repository never links Jolt, PhysX, Bullet or another backend directly |
| OpenUSD | the same exact pin as §1 when the package exposes OpenUSD types |
| Runtime ownership | `usd-stage-runner` orders the step; `usd-avatar-runtime` composes it; neither moves generic physics or MMD coupling into the importer |

The authored hand-off and staged integration are
[PHYSICS_INTEGRATION.md](../design/PHYSICS_INTEGRATION.md). Static PMX import,
inspection and stage authoring remain fully functional when this dependency is
absent.
