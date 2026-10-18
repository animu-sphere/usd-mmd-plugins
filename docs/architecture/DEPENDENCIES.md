# External dependencies

What `usd-mmd-plugins` builds against, what it refuses to depend on, and the
test a new dependency has to pass. Edges *between* this repository's own
components are [WORKSPACE.md §2](WORKSPACE.md#2-dependency-directions)'s.

Exact versions, ranges and artifact digests live in build manifests and pin
modules. This page owns dependency boundaries and required interfaces.

## 1. OpenUSD

| | |
| --- | --- |
| Pin source | [cmake/UsdMmdOpenUsd.cmake](../../cmake/UsdMmdOpenUsd.cmake) enforces the exact release for both build modes; [bundle manifests](../../plugins/usdMmdFileFormat/openstrata.plugin.yaml) mirror it as runtime.openusd. All plugins composed in one process must agree |
| Used by | `usdMmdFileFormat`; `mmdSchema`; `mmd_export`; `mmdImaging`, the only user of imaging; `mmdMotionAdapter` and `mmdSkeletonAdapter` use only the foundation types (`gf`, `tf`, `vt`) exposed through the shared motion packages, and no stage (§6) |
| Modules | linked today: `arch`, `tf`, `gf`, `vt`, `ar`, `sdf`, `usd`, `usdGeom`, `usdPhysics`, `usdShade`, `usdSkel`, `kind` (`usdShade` and `usdSkel` since Phase 2, `usdPhysics` since Phase 6); `hd` and `usdImaging` by `mmdImaging` alone, since Phase 8 |
| Not used | OpenExec, Hydra render delegates — nothing is evaluated or rendered here, so unlike `usd-vrm-plugins`' pin module this one probes for no OpenExec. `mmdImaging` uses UsdImaging's scene-index API to describe materials, and draws nothing |
| CI runtimes | the runtime matrix and digests declared in [openstrata.ci.yaml](../../openstrata.ci.yaml) |

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
| OpenStrata | The tool pin in [openstrata.ci.yaml](../../openstrata.ci.yaml); requires external-library artifacts and the usd-imaging bundle kind |
| Unit-test framework | **none**, as in `usd-vrm-plugins`: each suite is a plain executable that checks with `assert()`, compiled with `NDEBUG` undefined so Release builds still check, and registered with CTest |
| Sanitizers and fuzzing | Clang 18's AddressSanitizer, UndefinedBehaviorSanitizer and libFuzzer, from Ubuntu 24.04's packages, in [parser-sanitizers.yml](../../.github/workflows/parser-sanitizers.yml) only; the `USDMMD_SANITIZERS` and `USDMMD_BUILD_FUZZERS` options of [cmake/UsdMmdSanitizers.cmake](../../cmake/UsdMmdSanitizers.cmake) switch them on, and nothing shipped is built with them. Toolchain runtimes, not dependencies: no code is vendored and nothing links them outside that lane |

## 3. Refused dependencies

| Dependency | Refused because |
| --- | --- |
| Bullet, PhysX, Jolt, any physics engine directly | production components own no backend objects; the isolated installed-backend probe exception is defined in §7. Simulation is reached through the optional shared `usd-physics-plugins` contract ([PHYSICS_INTEGRATION.md §8](../design/PHYSICS_INTEGRATION.md#8-dependency-policy)) |
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

The shared motion core owns format-neutral poses/clips, humanoid semantics,
sampling, retargeting, recording and the UsdSkelAnimation bridge. MMD names,
IK and source interpretation remain here.

| Boundary | Contract |
| --- | --- |
| Installed interfaces | `motionCore` (poses/clips), `motionRetarget` (skeleton, maps and source/target rests), `motionSource` (public T-pose directions and shortest rotation), `motionUsd` (motion-stage reader/writer) |
| Adapters | `mmdMotionAdapter` uses motionCore; `mmdSkeletonAdapter` uses motionRetarget and privately motionSource. Static consumers still need the private link closure |
| Test-only interface | motionUsd is used by skeletal acceptance and generic-clip tests; exported adapter targets/packages do not require it |
| Consumption | Installed `find_package(<name> CONFIG)` targets `<name>::<name>`; never a copy of shared algorithms |
| Required semantics | TargetRestPose, shared T-pose directions and the opt-in unbound-intermediate rotation fold; [MOTION §10.9](../design/MOTION_CONTRACT.md#109-a-generic-clip-onto-a-pmx) owns the consumer recipe |
| Pin/range sources | [Motion adapter manifest](../../libs/mmdMotionAdapter/openstrata.library.yaml), [skeleton adapter manifest](../../libs/mmdSkeletonAdapter/openstrata.library.yaml), and their [installed package config](../../libs/mmdSkeletonAdapter/cmake/mmdSkeletonAdapterConfig.cmake.in) |
| OpenUSD / direction | Same exact release as §1; shared motion never depends on MMD |

Package manifests own the artifact closure for workspace and standalone
builds. They and generated package configs own compatible ranges; do not
duplicate the version history or digest tables here. Measured adapter changes
are linked from the [evidence map](../reports/2026-10-18-docs-ownership-audit.md#retained-evidence-and-unverified-signals).

## 7. usd-physics-plugins

The shared physics layer owns neutral descriptors, reader APIs, backend
objects, constraints, contacts, damping, spring approximation and stepping.
MMD metadata interpretation and bone/body coupling stay here.

| Boundary | Contract |
| --- | --- |
| Production consumer | An optional MMD coupling adapter, whose component/API is fixed by the first runtime consumer under [WORKSPACE](WORKSPACE.md); no normal physics dependency is declared yet |
| Static path | Parser/model/importer and motion components do not require physics packages |
| Consumption | Installed packages with declared compatibility; never sibling source, submodules or vendored backend code |
| Backend direction | Production MMD components use shared neutral interfaces; no direct Jolt/Bullet/PhysX backend integration |
| Runtime | usd-stage-runner owns order/tick; usd-avatar-runtime owns composition |
| OpenUSD | Same exact release as §1 wherever shared interfaces expose OpenUSD types |
| Probe requirements | [tests/physics_consumer/CMakeLists.txt](../../tests/physics_consumer/CMakeLists.txt) owns package names, requested versions and default-OFF options; it links installed physicsUsd and, for execution, physicsJolt |
| API/build compatibility | The probe requires package compatibility metadata revision 1. Installed configs check the core descriptor/build identity across backend/bridge/consumer, installed core/bridge headers and OpenUSD version/headers; execution requires a deterministic SDK. Matching a package's minor version alone does not prove ABI compatibility |
| Experimental enablement | Execution requires the installed six-DOF capability; translation and rotation options independently require execution and the corresponding descriptor fields |

The probe is isolated contract evidence, outside normal workspace/release
membership. Its direct link to the installed backend facade is a test-only
exception, not ownership of backend implementation. No published physics
artifact pin or production coupling API is selected.

For contact/damping/spring API changes, rebuild shared core, backend, bridge
and every consumer together into a separate compatible prefix. Mixing new
descriptor headers with older static libraries is invalid.
The conservative package identity does not audit every compiler flag, custom
macro or SDK binary; keep headers, configs and static libraries from the same
build together. Packages lacking metadata must be rebuilt, including reader-only
configurations. The [build guide](../guides/building.md#optional-physics-consumer)
provides the installed-package checks.
`BodyDescriptor::contactMaterial` / `damping`,
`validateContactMaterial()` / `validateBodyDamping()`, and
`SixDofConstraintDescriptor::translationSprings` / `rotationSprings` are
required where the corresponding probe feature is used. The probe's source
mapping remains outside the generic USD reader.

[PHYSICS_INTEGRATION](../design/PHYSICS_INTEGRATION.md) owns conversions,
diagnostic meaning and acceptance.
[Capabilities](../reference/CAPABILITY_MATRIX.md#physics-preservation-and-execution)
own feature availability; [current.md](../roadmap/current.md) owns package
adoption and runtime gates. Dated
[execution](../reports/2026-10-18-phase8-six-dof-execution.md) and
[spring](../reports/2026-10-18-phase8-rotation-springs.md) reports record
validated environments and reproduction commands.
