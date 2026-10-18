# Phase 8 installed physics package compatibility (2026-10-18)

## Scope and environment

P0-1 installed-package evidence, without a production coupling adapter or an
avatar L3 claim. MMD baseline `389534c0171c7170ff21aab0b6d74ed7e86e2352` and shared
physics baseline `05b44312077569ead92038599e3105a27df415a7`, plus the working-tree
compatibility changes described below. No published package or artifact pin
changed. Windows x86_64, MSVC 19.51.36256.0, CMake 4.4, OpenUSD 26.08,
Python 3.13.14 and deterministic Jolt 5.5.0, Release with the dynamic MSVC runtime.

## Change and boundary

The shared packages now export compatibility metadata revision 1. Their configs
compare the core's public-header/build identity, check installed core/bridge
headers, and require the OpenUSD version/header set used to build the bridge.
Six-DOF builds require deterministic Jolt. The MMD probe requires this metadata
and exact OpenUSD 26.08, preventing accidental reuse of older `0.1` packages.
The owning interface and limitations are in
[DEPENDENCIES §7](../architecture/DEPENDENCIES.md#7-usd-physics-plugins).

All MMD configurations linked the installed core/backend/bridge from one clean
prefix. No shared source was added to the MMD project. The importer, source
fixtures, conversion algorithms and normal dependency graph did not change.

## Results

| Configuration / check | Result |
| --- | --- |
| Shared deterministic backend + USD suite | 23/23 CTests pass |
| Shared no-Jolt suite | 11/11 CTests pass |
| MMD reader-only | 5/5 CTests pass |
| MMD execution, both spring options OFF | 5/5 CTests pass |
| MMD execution, both spring options ON | 9/9 CTests pass |
| Corpus, each of the three MMD configurations | 12 reader controls and 28 opening PMX fixtures accepted; exit 0 |
| Execution corpus | 22 physics-bearing inputs: three bodies/two constraints and 120 finite steps each; six empty inputs: 120 empty steps each |
| Shared installed-consumer negative controls | Reject mismatched/missing core identity, compiler identity, modified installed core headers and incompatible Jolt definitions; bridge/core mismatch also tested without importing the backend |
| Old installed physics packages in MMD | Configure rejects with a compatibility-metadata/rebuild diagnostic |
| Old nondeterministic SDK in a standalone six-DOF backend build | Configure rejects with `Six-DOF execution requires a deterministic Jolt SDK` |

The installed-consumer/doc checks were rerun after adding the modified-header
negative control. The source spring-fidelity counters and approximation labels
remain unchanged. These results establish bounded package consumption and
existing finite-step behavior; no new repeatability, source trajectory or
bone synchronization claim is made. Hosted verification of this compatibility
increment and publication remain separate from these local results.

Installed static-library SHA-256 values:

| Library | SHA-256 |
| --- | --- |
| physicsCore.lib | `4fd5840e19f8d212ff33fba787d898820174b26186a108eba58c0cbc82edc313` |
| physicsJolt.lib | `f5255cb3604fe5002251899303397ffe9ee251bb3d66116ac9a3702a25730e25` |
| physicsUsd.lib | `0d398cd5d367223d56abd7a782db0df13acf1e6d2f758e4bb1edfc165fb3ac48` |

## Reproduction

In the shared physics repository, using `USD_ROOT` and `JOLT_ROOT` for the
installed validated SDKs:

```powershell
cmake -S . -B build/phase8-package-compatibility `
  -G 'Visual Studio 18 2026' -A x64 `
  -DUSDPHYSICS_BUILD_USD=ON -DPHYSICSJOLT_ENABLE_SIX_DOF=ON `
  -DPHYSICSJOLT_REQUIRE_JOLT=ON "-DCMAKE_PREFIX_PATH=$env:USD_ROOT" `
  "-Dpxr_DIR=$env:USD_ROOT" "-DJolt_DIR=$env:JOLT_ROOT/lib/cmake/Jolt"
cmake --build build/phase8-package-compatibility --config Release
$env:PATH = "$env:USD_ROOT/bin;$env:USD_ROOT/lib;$env:PATH"
ctest --test-dir build/phase8-package-compatibility -C Release --output-on-failure
cmake --install build/phase8-package-compatibility --config Release `
  --prefix build/phase8-package-compatibility-prefix
```

Set `PHYSICS_PREFIX` to that installation. In MMD, run the three configurations
from [the build guide](../guides/building.md#optional-physics-consumer), then:

```powershell
$env:PXR_PLUGINPATH_NAME = "$PWD/plugins/mmdSchema/plugin/resources/mmdSchema;$PWD/plugins/usdMmdFileFormat/plugin/resources/usdMmdFileFormat"
$fixtureRoot = Join-Path $PWD 'plugins/usdMmdFileFormat/tests/fixtures'
$manifest = Get-Content -Raw (Join-Path $fixtureRoot 'fixtures.json') | ConvertFrom-Json -AsHashtable
$stagePaths = @($manifest.Keys | Sort-Object | Where-Object { $manifest[$_].opens } |
  ForEach-Object { Join-Path $fixtureRoot $_ })
foreach ($mode in @('reader','execution','springs')) {
  & "build/physics-package-$mode/Release/mmd_physics_consumer_probe.exe" @stagePaths `
    2> "build/physics-package-$mode/diagnostics.log" |
    Set-Content "build/physics-package-$mode/results.tsv"
  if ($LASTEXITCODE) { throw "Physics corpus failed: $mode" }
}
```

Local logs live under the three build directories. Corpus stability/replay
automation and CI adoption belong to P0-2; composed avatar acceptance continues
under [PHYSICS_INTEGRATION §12](../design/PHYSICS_INTEGRATION.md#12-fidelity-and-acceptance-policy).
