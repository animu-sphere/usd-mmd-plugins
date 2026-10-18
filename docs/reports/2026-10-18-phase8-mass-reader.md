# Phase 8 primitive mass inference follow-up (2026-10-18)

Dated local Windows evidence following the
[joint snapshot probe](2026-10-18-phase8-joint-reader.md).
This validates standard declaration snapshots, not PMX simulation.

## Change under test

Local `usd-physics-plugins` changes over `c420c35` resolve dynamic mass from
explicit mass or scaled single-Cube/Sphere/Capsule collision volume and
body-local density. Zero/absent mass requests inference; zero/absent density
uses the standard USD fallback of 1000 kg/m³. Positive explicit mass wins.
The behavior follows the
[OpenUSD mass specification](https://openusd.org/release/api/usd_physics_page_front.html).
Inherited/collider MassAPI overrides, physics materials, inertia and
center-of-mass overrides remain rejected. No arbitrary positive mass is
authored into the imported stage, and no importer contract changes.

The [isolated consumer](../../tests/physics_consumer/main.cpp) still links
only installed `physicsUsd::physicsUsd`. Its nine previous controls are
retained, with three new child-primitive inference controls: a scaled Box
yields 8000 kg, a Sphere yields `1000π/6` kg, and a Capsule yields
`11000π/12` kg. All use zero mass without density, checking the standard
fallback independently of the reader's volume calculation. The PMX loop still
checks body/joint counts, reconstructs coincident world joint frames to
`1e-6` per matrix element, and reports format-only joints separately.

## Results

Windows x86_64, MSVC 19.51, Visual Studio 18 2026, OpenUSD 26.08 and
Python 3.13.14. All 15 shared physics tests pass, including the focused mass
suite and clean-prefix installed consumers. Hosted checks remain pending.

Installed `physicsUsd.lib` SHA-256:
`1e5e92629d45cafb81257b1042e55014ff8cce786dd9fea084f08f42c309aab7`.

| Input | Count | Result |
| --- | ---: | --- |
| Previous primitive/kinematic/joint controls | 9 | Accepted |
| Added inferred-mass controls | 3 | Accepted |
| Physics-bearing PMX fixtures | 22 | Each accepted with three bodies/two standard joints |
| PMX fixtures without rigid bodies | 6 | Accepted with empty body/joint snapshots |

The process exits zero with no reader rejections. The 21 previously rejected
fixtures now pass the missing-mass gate. `recoverable/physics-repairs.pmx`
still has one format-only joint outside the generic snapshot. Standard USD
inference does not establish equivalence to PMX's treatment of zero source
mass. Coefficients, collision filtering, springs, six-DOF execution and MMD
bone/body coupling remain outside this acceptance.

## Reproduction

Build/install the shared packages into a separate prefix (this run used
`build/phase8-mass-prefix` in that repository), then set `PHYSICS_PREFIX`
and `USD_ROOT` to that prefix and the matching SDK:

```powershell
cmake -S tests/physics_consumer -B build/physics-consumer-mass `
  -G 'Visual Studio 18 2026' -A x64 `
  "-DCMAKE_PREFIX_PATH=$env:PHYSICS_PREFIX;$env:USD_ROOT"
cmake --build build/physics-consumer-mass --config Release
$env:PATH = "$env:USD_ROOT/bin;$env:USD_ROOT/lib;$env:PATH"
$env:PXR_PLUGINPATH_NAME = "$PWD/plugins/mmdSchema/plugin/resources/mmdSchema;$PWD/plugins/usdMmdFileFormat/plugin/resources/usdMmdFileFormat"
$fixtureRoot = Join-Path $PWD 'plugins/usdMmdFileFormat/tests/fixtures'
$manifest = Get-Content -Raw (Join-Path $fixtureRoot 'fixtures.json') | ConvertFrom-Json -AsHashtable
$stagePaths = @($manifest.Keys | Sort-Object | Where-Object { $manifest[$_].opens } | ForEach-Object { Join-Path $fixtureRoot $_ })
& build/physics-consumer-mass/Release/mmd_physics_consumer_probe.exe @stagePaths `
  2> build/physics-consumer-mass/diagnostics.log |
  Tee-Object -FilePath build/physics-consumer-mass/results.tsv
```

Inputs remain unchanged committed synthetic fixtures. Earlier prefixes remain
separate; public package pins are unchanged and the importer has no shared
physics dependency.

## Next boundary

Add solver-neutral six-DOF execution, then progressively resolve physical
coefficients, filtering, springs and source-only semantics. Define runtime
synchronization contracts before adding MMD bone/body coupling. Hosted
validation and compatible public package publication remain open.
