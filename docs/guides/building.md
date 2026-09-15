# Building and testing

How to build the workspace, run its tests, and package the plugin. Every
command on this page has been run, on Windows 11 with Visual Studio 18
(MSVC 19.51), CMake 4.4, Python 3.13 and OpenUSD 26.08, on 2026-09-15. macOS
and Linux run the same `ost` commands in CI
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
- For the OpenStrata path: `ost` 0.22.10 and a `cy2026` / `usd` runtime.

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
bundle's `plugInfo.json` expects it.

`ctest` runs every test in the workspace:

| Test | What it proves |
| --- | --- |
| `mmdPmx_unit` | the header reader, `Diagnostic` and `Result<T>` |
| `mmdPmx_boundaries` | `mmdPmx`'s sources include no OpenUSD, its link line is empty, and a binary linking it imports no OpenUSD library |
| `mmdPmx_boundaries_selftest` | the boundary check's own rules reject what they must |
| `workspace_fixtures` | the committed fixtures are exactly what the generator writes |
| `workspace_docs`, `workspace_docs_selftest` | links and anchors resolve; every version and pin mirror agrees |
| `usdMmdFileFormat_stage_open` | every fixture opens, or fails with its fatal code, as `fixtures.json` says |
| `usdMmdFileFormat_unicode_paths` | the same opens under `ユニコード-é/`, from a Python host |
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

The library on its own, including a consumer `ost` generates from the
[package contract](../architecture/PACKAGE_CONTRACT.md):

```powershell
ost library build libs/mmdPmx
ost library test libs/mmdPmx
ost library verify-consumer libs/mmdPmx
```

## The installed-consumer lane

[scripts/check_installed_consumer.py](../../scripts/check_installed_consumer.py)
installs a build tree into a temporary prefix, checks that the prefix holds
what each package promises and names no source or build location, builds
[tests/installed_consumer/](../../tests/installed_consumer/) — copied out of the
repository — against that prefix alone, and opens a PMX from a Python host
whose only plugin path is the prefix's. `ctest` runs it as
`workspace_installed_consumer`.

## Fixtures

Every PMX fixture is written by
[tests/fixtures/generate_fixtures.py](../../tests/fixtures/generate_fixtures.py)
into `plugins/usdMmdFileFormat/tests/fixtures/`. After changing the generator:

```powershell
python tests/fixtures/generate_fixtures.py
python tests/fixtures/generate_fixtures.py --check
```

and commit the result; `workspace_fixtures` fails until the two agree.

The L5 golden, `minimal.pmx.golden.usda`, is the importer's output rather than
the generator's. Regenerate it through the plugin's runtime session:

```powershell
ost plugin run plugins\usdMmdFileFormat -- usdcat --flatten plugins\usdMmdFileFormat\tests\fixtures\minimal.pmx --out plugins\usdMmdFileFormat\tests\fixtures\minimal.pmx.golden.usda
```

`usdcat --flatten` writes the absolute path of the file into the stage's
`doc`. Replace it with the bundle-relative `tests/fixtures/minimal.pmx` before
committing: `ost` ignores that line when it compares, and a committed file
never carries a machine-local path.
