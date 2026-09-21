# Building and testing

How to build the workspace, run its tests, and package the plugin. Every
command on this page has been run, on Windows 11 with Visual Studio 18
(MSVC 19.51), CMake 4.4, Python 3.13 and OpenUSD 26.08, on 2026-09-15 — except
the sanitizer build, which was run on Ubuntu (WSL) with GCC 15, as that
section says. macOS and Linux run the same `ost` commands in CI
([openstrata.ci.yaml](../../openstrata.ci.yaml)); their plain-CMake presets
exist but have not been run by hand, so they are not documented here yet.

Commands are PowerShell, run from the repository root.

## Requirements

- **OpenUSD 26.08, exactly.** Configuring against any other release fails in
  [cmake/UsdMmdOpenUsd.cmake](../../cmake/UsdMmdOpenUsd.cmake)
  ([DEPENDENCIES.md §1](../architecture/DEPENDENCIES.md#1-openusd)).
- **The Python OpenUSD was built against** (3.13 for the 26.08 runtimes). The
  integration tests import OpenUSD's bindings, which refuse any other version
  (`Module use of python313.dll conflicts with this version of Python`).
- CMake 3.22 or later and a C++20 compiler.
- For the OpenStrata path: `ost` 0.23.2 and a `cy2026` / `usd` runtime.

## Plain CMake

`CMakePresets.json` reads the OpenUSD install from `USD_INSTALL_ROOT`:

```powershell
$env:USD_INSTALL_ROOT = "<OpenUSD 26.08 install>"
cmake --preset windows-msvc
cmake --build --preset windows-release
ctest --preset windows-release
```

The Windows preset names no generator, so CMake picks the newest Visual Studio
installed. The build tree is `build/windows-msvc/`. The plugin library is
staged into the bundle itself, `plugins/usdMmdFileFormat/lib/`, where the
bundle's `plugInfo.json` expects it, `mmd_inspect` into
`tools/mmdInspect/bin/`, and `vmd_inspect` into `tools/vmdInspect/bin/`
([inspecting.md](inspecting.md) says how to use them).

`ctest` runs every test in the workspace:

| Test | What it proves |
| --- | --- |
| `mmdPmx_unit` | the parser table by table — every record variant, every index width, each malformed case at its byte — the text decoders, `Diagnostic`, `Result<T>` and the diagnostic limit |
| `mmdPmx_robustness` | every byte of the sample models overwritten, and every prefix read: no crash, and no document that breaks its invariants |
| `mmdPmx_boundaries` | `mmdPmx`'s sources include no OpenUSD, its link line is empty, and a binary linking it imports no OpenUSD library |
| `mmdPmx_boundaries_selftest` | the boundary check's own rules reject what they must |
| `mmdModel_unit` | canonicalization over documents stated as data — the basis conversion, identifiers, joint order and its repairs, weight normalization, texture paths, face ranges, every morph type and its index remapping — and the diagnostic each repair raises |
| `mmdModel_robustness` | 20,000 generated documents with wild parents, weights, names, paths and morph graphs, each canonicalized twice: no crash, the same bits both times, every promise of `CanonicalDocument.h` kept — expanding the group morphs always terminates |
| `mmdModel_boundaries` | `mmdModel`'s sources include no OpenUSD, it links `mmdPmx` and nothing else, and a binary linking it imports no OpenUSD library |
| `motionVmd_unit` | the VMD reader section by section — both signatures, a file ending after any section, every truncation, counts, names cut inside a character or unmapped, the diagnostic limit — the CP932 table in both directions, and the track grouping, duplicate frames and interpolation bytes |
| `motionVmd_robustness` | every byte of the sample motions overwritten, and every prefix read: no crash, and no document or motion that breaks its invariants |
| `motionVmd_boundaries` | `motionVmd`'s sources include no OpenUSD and no `mmdPmx` or `mmdModel` header, its link line is empty, and a binary linking it imports no OpenUSD library |
| `motionVmd_cp932_table` | the committed CP932 table is exactly what its generator writes |
| `mmdMotionBinding_unit` | binding a motion to a canonicalized model: CP932 field bytes, matching per field width, unmatched, ambiguous and unencodable names, the basis conversion of a key, and carrying a `motionVmd` diagnostic into the workspace record |
| `mmdMotionBinding_boundaries` | `mmdMotionBinding`'s sources include no OpenUSD, it links `mmdModel` and `motionVmd` and nothing else, and a binary linking it imports no OpenUSD library |
| `mmdControl_unit` | evaluation over synthetic rigs with known answers: Bézier progress and which key's curve a segment follows, held and stepped tracks, forward kinematics, the evaluation order, bone morphs through nested groups and the channels left, appends with negative ratios and in chains, IK on one link, with an angle limit and with Euler limits, a leg with a plane knee and `足D`, the IK-enable track, the three diagnostics, and a VMD through `Bind` |
| `mmdControl_robustness` | 20,000 generated rigs and motions — appends and IK chains naming any joint, wild loop counts, limits and keys, cyclic group morphs — each evaluated at five times, twice: no crash, the same bits both times, and finite unit-rotation poses from the tame half |
| `mmdControl_boundaries` | `mmdControl`'s sources include no OpenUSD and no `motionCore/` header, it links `mmdMotionBinding` and `mmdModel` and nothing else, and a binary linking it imports no OpenUSD library |
| `mmdSkeletonAdapter_boundaries`, `mmdMotionAdapter_boundaries` | each adapter links only its declared local and shared-motion packages; its test binary may import the OpenUSD foundation closure those packages expose, including the private `usd_boost`/`usd_python` support libraries on macOS, but no stage, schema or imaging library |
| `mmd_inspect_fixtures` | `mmd_inspect` reads every generated fixture as `fixtures.json` says, from an ASCII and a non-ASCII directory |
| `mmd_inspect_boundaries` | `mmd_inspect` links `mmdPmx` and nothing else, and imports no OpenUSD library |
| `vmd_inspect_fixtures` | `vmd_inspect` reads every generated VMD fixture as its `fixtures.json` says, from an ASCII and a non-ASCII directory |
| `vmd_inspect_boundaries` | `vmd_inspect` links `motionVmd` and nothing else, includes no model library, and imports no OpenUSD library |
| `workspace_fixtures` | the committed fixtures and texture files are exactly what the generator writes |
| `workspace_docs`, `workspace_docs_selftest` | links and anchors resolve; every version and pin mirror agrees; the diagnostic catalog matches the declared codes |
| `usdMmdFileFormat_stage_open` | every fixture opens, or fails with its fatal code, as `fixtures.json` says; each stage that opens holds what `fixtures.json` says it must — identifiers, joint paths, bind translations, material subsets, texture asset paths and whether they resolve, a vertex through the conversion, every morph prim and, for each blend shape, the points UsdSkel moves when it is driven to weight 1 — passes the stage checklist, and passes every validator OpenUSD registers |
| `usdMmdFileFormat_unicode_paths` | the same, with the fixture directory and its texture files copied under `ユニコード-é/`, from a Python host |
| `usdMmdFileFormat_notice_listeners` | every Python entry point that reaches the importer (`Usd.Stage.Open`, `Sdf.Layer.Reload`, `Sdf.Layer.OpenAsAnonymous`) returns while a global Python notice listener is registered |
| `workspace_installed_consumer` | the installed-consumer lane (below) |

The installed-consumer lane configures and builds a second project, so it is
labelled and can be left out:

```powershell
ctest --preset windows-release -LE installed-consumer
```

## OpenStrata

The dependency graph, before anything builds:

```powershell
ost plugin test --workspace --graph-only
```

The whole workspace, as CI's workspace cells build it (Ninja, the runtime's
toolchain, the root CTest suite):

```powershell
ost build
ost test
```

The bundle on its own, as CI's bundle cells build it — `ost` builds and
installs `mmdPmx` into the workspace prefix first, because the bundle's
manifest requires it:

```powershell
ost plugin build plugins/usdMmdFileFormat
ost plugin test plugins/usdMmdFileFormat
```

`ost plugin test` runs the verification pyramid, L0 (bundle structure) to L5
(the flattened stage of `minimal.pmx` against its golden).

Packaging, and the pyramid again against the extracted package rather than
the build tree:

```powershell
ost plugin package plugins/usdMmdFileFormat
ost plugin test plugins/usdMmdFileFormat --from-package
```

Build the bundle with `ost plugin build` immediately before packaging. A root
build (`ost build`, or plain CMake) also configures the bundle and rewrites
its `buildInfo.json`, and `ost plugin package` then refuses with
`PLUGIN_PACKAGE_OUTPUT_MISMATCH`, because the file no longer matches the last
managed bundle build.

The product a release ships — the bundle and both tools in one archive — as
[release.yml](../../.github/workflows/release.yml) builds it, root tree first
and bundle second for the same reason (run on 2026-09-17):

```powershell
ost build
ost plugin build plugins/usdMmdFileFormat
ost plugin package --workspace --product
ost plugin product install --prefix <new directory> dist\products\usd-mmd-plugins\<version>\<target>
python scripts\product_smoke.py --product dist\products\usd-mmd-plugins\<version>\<target>
```

`product_smoke.py` installs the product into a scratch prefix itself, runs
both tools from it, and opens a fixture through the installed plugin with
nothing from the build tree on the discovery path.

Each library on its own, including a consumer `ost` generates from the
[package contract](../architecture/PACKAGE_CONTRACT.md) — for `mmdModel`,
`ost` builds and installs `mmdPmx` first, as its manifest requires:

```powershell
ost library build libs/mmdPmx
ost library test libs/mmdPmx
ost library verify-consumer libs/mmdPmx
ost library build libs/mmdModel
ost library test libs/mmdModel
ost library verify-consumer libs/mmdModel
ost library build libs/motionVmd
ost library test libs/motionVmd
ost library verify-consumer libs/motionVmd
ost library build libs/mmdMotionBinding
ost library test libs/mmdMotionBinding
ost library verify-consumer libs/mmdMotionBinding
ost library build libs/mmdControl
ost library test libs/mmdControl
ost library verify-consumer libs/mmdControl
```

`mmdMotionBinding` requires `mmdModel` and `motionVmd`, so `ost` builds and
installs its closure of four libraries first; `mmdControl`, which requires
`mmdMotionBinding` and `mmdModel`, a closure of five.

A root build and `ost build` both stage the plugin library into the same
`plugins/usdMmdFileFormat/lib/`. On 2026-09-17 one `ost` build tree held an
object whose recorded header dependencies were empty (`ninja -t deps` printed
`#deps 0`), so it was never rebuilt after `CanonicalDocument.h` changed, and
every stage-opening test crashed in the Python host. Deleting that object —
or the build tree — and running `ost build` again fixed it; a fresh checkout
never showed it.

[opening.md](opening.md) shows what to do with the stage once the plugin is
built.

## Sanitizers

`mmdPmx` builds on its own, and its unit, robustness and boundary tests run
under AddressSanitizer and UndefinedBehaviorSanitizer with two cache options.
Run on Ubuntu 24.04 under WSL, with GCC 15, CMake 4.2 and Ninja, from the
repository root:

```sh
cmake -S libs/mmdPmx -B ~/mmd-sanitize -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo       -DMMDPMX_SANITIZERS="address;undefined" -DMMDPMX_BUILD_TESTS=ON
cmake --build ~/mmd-sanitize
ctest --test-dir ~/mmd-sanitize --output-on-failure
```

`mmdModel` builds on its own against an installed `mmdPmx`, so the
instrumented parser is installed into a prefix first:

```sh
cmake --install ~/mmd-sanitize --prefix ~/mmd-san-prefix
cmake -S libs/mmdModel -B ~/mmd-san-model -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo       -DMMDMODEL_SANITIZERS="address;undefined" -DMMDMODEL_BUILD_TESTS=ON       -DCMAKE_PREFIX_PATH=~/mmd-san-prefix
cmake --build ~/mmd-san-model
ctest --test-dir ~/mmd-san-model --output-on-failure
```

`motionVmd`, `mmdMotionBinding` and `mmdControl` declare the same options
(`MOTIONVMD_SANITIZERS`, `MOTIONVMD_BUILD_FUZZER`,
`MMDMOTIONBINDING_SANITIZERS`, `MMDCONTROL_SANITIZERS`); instrumented, they
have run only in CI.

The fuzz targets need Clang's libFuzzer: `-DMMDPMX_BUILD_FUZZER=ON` or
`-DMOTIONVMD_BUILD_FUZZER=ON` with `clang++`, and the sanitizers on. They have
run only in CI;
[parser-sanitizers.yml](../../.github/workflows/parser-sanitizers.yml) is the
record of its commands, including how the corpus is seeded from the generated
fixtures.

## The installed-consumer lane

[scripts/check_installed_consumer.py](../../scripts/check_installed_consumer.py)
installs a build tree into a temporary prefix, checks that the prefix holds
what each package promises and names no source or build location, builds
[tests/installed_consumer/](../../tests/installed_consumer/) — copied out of the
repository — against that prefix alone, runs the installed `mmd_inspect` over
every fixture, reads every generated VMD fixture through the installed
`motionVmd` and `vmd_inspect`, binds one to a PMX fixture through the
installed `mmdMotionBinding`, evaluates it at every frame through the
installed `mmdControl`, and opens a PMX from a Python host whose only plugin
path is the prefix's. `ctest` runs it as
`workspace_installed_consumer`.

## Fixtures

Every PMX fixture is written by
[tests/fixtures/generate_fixtures.py](../../tests/fixtures/generate_fixtures.py)
into `plugins/usdMmdFileFormat/tests/fixtures/`: models that use every table
and record variant, ones that exercise each canonical repair, recoverable ones
that open with a recorded diagnostic (`recoverable/`), and fatal ones
(`malformed/`) — with the one-pixel texture files the sample models name, in
`tex/`, `sph/` and `toon/`. `fixtures.json` beside them says what each must
do, and for each one that opens, what its stage must hold. After changing the
generator:

```powershell
python tests/fixtures/generate_fixtures.py
python tests/fixtures/generate_fixtures.py --check
```

and commit the result; `workspace_fixtures` fails until the two agree.

VMD fixtures are written by
[tests/fixtures/generate_vmd_fixtures.py](../../tests/fixtures/generate_vmd_fixtures.py)
into whatever directory a test gives it, with their own `fixtures.json`, and
are never committed:

```powershell
python tests/fixtures/generate_vmd_fixtures.py --out build/vmd-fixtures
```

The CP932 table `motionVmd` decodes with is generated too. After changing its
generator:

```powershell
python libs/motionVmd/tools/generate_cp932_table.py
python libs/motionVmd/tools/generate_cp932_table.py --check
```

The two L5 goldens, `minimal.pmx.golden.usda` and
`recoverable/unsafe-texture-paths.pmx.golden.usda`, are the importer's output
rather than the generator's. Regenerate each through the plugin's runtime
session:

```powershell
ost plugin run plugins\usdMmdFileFormat -- usdcat --flatten plugins\usdMmdFileFormat\tests\fixtures\minimal.pmx --out plugins\usdMmdFileFormat\tests\fixtures\minimal.pmx.golden.usda
ost plugin run plugins\usdMmdFileFormat -- usdcat --flatten plugins\usdMmdFileFormat\tests\fixtures\recoverable\unsafe-texture-paths.pmx --out plugins\usdMmdFileFormat\tests\fixtures\recoverable\unsafe-texture-paths.pmx.golden.usda
```

`usdcat --flatten` writes the absolute path of the file into the stage's
`doc`. Replace it with the bundle-relative path (`tests/fixtures/minimal.pmx`,
`tests/fixtures/recoverable/unsafe-texture-paths.pmx`) before committing:
`ost` ignores that line when it compares, and a committed file never carries a
machine-local path. Flattening also makes every asset path absolute, which is
why the second golden is of the fixture whose texture paths are all refused:
its stage has none.
