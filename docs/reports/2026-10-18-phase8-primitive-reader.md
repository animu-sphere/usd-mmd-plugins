# Phase 8 Sphere/Capsule reader follow-up (2026-10-18)

Follow-up: [kinematic reader evidence](2026-10-18-phase8-kinematic-reader.md).

Dated local Windows evidence following the
[collider-local pose probe](2026-10-18-phase8-collider-local-pose.md).
This validates a bounded shared-reader increment, not PMX simulation.

## Change under test

Local `usd-physics-plugins` changes over `a2121b4` add neutral sphere/capsule
dimensions, Jolt geometry and `readPrimitiveScene()`. Its Cube-only
`readBoxScene()` API remains available. Rounded geometry admits uniform
composed scale, collider-local translation/rotation and capsule X/Y/Z axes;
nonuniform scale rejects. Core, backend, reader and consumers are rebuilt
together because the shape descriptor layout changes. This is a local
install, not a published artifact pin.

The [isolated consumer](../../tests/physics_consumer/main.cpp) retains all
three Box controls and adds child Sphere/Capsule controls with explicit
mass, scale, translation and rotation below the composed body frame. It
checks dimensions, body path/world transform, mass and local translation.
The full PMX loop now calls `readPrimitiveScene()` on the unchanged imported
stages. The probe still links only installed `physicsUsd::physicsUsd`.

## Results

Windows x86_64, MSVC 19.51, Visual Studio 18 2026, OpenUSD 26.08 and
Python 3.13.14. The shared physics suite passes all 12 tests, including
USD geometry comparisons for every capsule axis, invalid/animated dimensions,
Jolt queries and settling, and clean-prefix installed consumers. Hosted
verification remains pending.

The installed `physicsUsd.lib` has SHA-256
`2e06c0d6a3ecd0657da3077ff4847af92b35ca9407a11013f1e41ee107c61885`.

| Input | Count | Result |
| --- | ---: | --- |
| Existing direct/child/offset Box controls | 3 | Accepted |
| Child Sphere control | 1 | Accepted with local pose and scaled radius |
| Child X-axis Capsule control | 1 | Accepted with local pose, radius and cylinder half-height |
| Full PMX fixtures with rigid bodies | 22 | Rejected with `unsupportedDeclaration` at `/Asset/physics/joints/joint_0000` |
| PMX fixtures without rigid bodies | 6 | Accepted with an empty snapshot |

The process exits zero after reporting typed outcomes. Declaration validation
rejects joints before converting bodies, so the full PMX results establish no
body or simulation support. No importer or stage-contract change was needed.

## Reproduction

Build/install the shared packages together into a separate prefix; this run
used `build/phase8-primitives-prefix` in the physics repository. Set
`PHYSICS_PREFIX` to that prefix and `USD_ROOT` to the matching SDK:

```powershell
cmake -S tests/physics_consumer -B build/physics-consumer-primitives `
  -G 'Visual Studio 18 2026' -A x64 `
  "-DCMAKE_PREFIX_PATH=$env:PHYSICS_PREFIX;$env:USD_ROOT"
cmake --build build/physics-consumer-primitives --config Release
$env:PATH = "$env:USD_ROOT/bin;$env:USD_ROOT/lib;$env:PATH"
$env:PXR_PLUGINPATH_NAME = "$PWD/plugins/mmdSchema/plugin/resources/mmdSchema;$PWD/plugins/usdMmdFileFormat/plugin/resources/usdMmdFileFormat"
$fixtureRoot = Join-Path $PWD 'plugins/usdMmdFileFormat/tests/fixtures'
$manifest = Get-Content -Raw (Join-Path $fixtureRoot 'fixtures.json') | ConvertFrom-Json -AsHashtable
$stagePaths = @($manifest.Keys | Sort-Object | Where-Object { $manifest[$_].opens } | ForEach-Object { Join-Path $fixtureRoot $_ })
& build/physics-consumer-primitives/Release/mmd_physics_consumer_probe.exe @stagePaths `
  2> build/physics-consumer-primitives/diagnostics.log |
  Tee-Object -FilePath build/physics-consumer-primitives/results.tsv
```

Earlier prefixes/builds remain separate. Inputs are committed synthetic
fixtures; no third-party assets are redistributed.

## Next boundary

Add kinematic operations through the shared core/backend, then joint frames
and limits. Rerun the unchanged full-PMX inputs after each installed
increment. Keep MMD bone/body coupling deferred until generic body and
synchronization contracts are concrete. Physical coefficients and filtering
also remain necessary before full fidelity can be claimed.
