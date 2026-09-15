# External dependencies

What `usd-mmd-plugins` builds against, what it refuses to depend on, and the
test a new dependency has to pass. Edges *between* this repository's own
components are [WORKSPACE.md §2](WORKSPACE.md#2-dependency-directions)'s.

Status (2026-09-15): the Phase 0 decisions are closed, and every value below
is what the workspace builds with. The OpenUSD pin is enforced at configure
time.

## 1. OpenUSD

| | |
| --- | --- |
| Pin | OpenUSD **26.08**, exactly (`PXR_VERSION` 2608), enforced by [cmake/UsdMmdOpenUsd.cmake](../../cmake/UsdMmdOpenUsd.cmake) for `ost` and plain-CMake builds alike and declared as `runtime.openusd: "==26.08"` in the bundle manifest; the release the rest of the ecosystem pins (`usd-vrm-plugins` too), because `usd-avatar-runtime` composes every plugin into one OpenUSD process |
| Used by | `usdMmdFileFormat` (and later `mmdSchema`, `usdVmdFileFormat`) only |
| Modules | linked today: `arch`, `tf`, `gf`, `vt`, `ar`, `sdf`, `usd`, `usdGeom`, `kind`; `usdSkel` and `usdShade` join in Phases 2–4, `usdPhysics` in Phase 6 |
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
| Build | CMake **3.22** or later, the minimum `usd-vrm-plugins` declares; `CMakePresets.json` for the plain-CMake path |
| Compilers | MSVC on Windows, Clang on macOS (arm64), GCC on Linux — the three hosted lanes `usd-vrm-plugins` runs |
| Windows flags | `/utf-8`, `NOMINMAX`, applied by `usdmmd_target_defaults()` in [cmake/UsdMmdTargets.cmake](../../cmake/UsdMmdTargets.cmake) ([WORKSPACE.md §5](WORKSPACE.md#5-build-modes)) |
| Python | the Python OpenUSD was built against — 3.13 for the 26.08 runtimes — for stage tests and tooling. The root project finds the interpreter *after* OpenUSD, so it inherits the one `pxrConfig.cmake` names |
| OpenStrata | `ost` **0.22.10**, pinned in `openstrata.ci.yaml` |
| Unit-test framework | **none**, as in `usd-vrm-plugins`: each suite is a plain executable that checks with `assert()`, compiled with `NDEBUG` undefined so Release builds still check, and registered with CTest |

## 3. Refused dependencies

| Dependency | Refused because |
| --- | --- |
| Bullet, PhysX, Jolt, any physics engine | nothing is simulated; physics execution belongs to a runtime ([DESIGN_POLICY.md §8](../design/DESIGN_POLICY.md#8-physics-policy)) |
| An image decoder in the importer | the importer never reads texture pixels; authoring stays independent of image content and of whether files exist ([TEXT_ENCODING_POLICY.md §7.3](../design/TEXT_ENCODING_POLICY.md#73-no-filesystem-access-while-authoring)) |
| ICU, `iconv`, OS code-page APIs | PMX text is UTF-8 or UTF-16LE, decoded by the parser; CP932 (for VMD, PMD) uses a table the project owns ([TEXT_ENCODING_POLICY.md §9](../design/TEXT_ENCODING_POLICY.md#9-pmd-and-vmd)) |
| A third-party PMX parser, by default | PMX is a bounded format; a purpose-built parser avoids inheriting an application's semantics ([DESIGN_POLICY.md §10](../design/DESIGN_POLICY.md#10-parser-strategy)) — adoption is possible only through §4 |
| OpenExec, Hydra, `hydra-toon`, `usd-stage-runner` | the importer neither evaluates nor renders ([WORKSPACE.md §2.2](WORKSPACE.md#22-forbidden-edges)) |

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
- **The CP932 mapping** needed by `motionVmd` (Phase 7) is generated from a
  published mapping whose license is recorded when it is added.
