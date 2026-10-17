# Phase 8 collider-local pose consumer follow-up (2026-10-18)

Dated local Windows evidence, following the
[centered child Box probe](2026-10-18-phase8-child-box-reader.md).
This verifies a bounded reader increment, not PMX simulation.

## Change under test

Local `usd-physics-plugins` changes over `1a41b77` add an identity-defaulting
`ShapeDescriptor::localTransform`, populated from a child Cube's composed
relative translation and rotation. Scale remains in shape dimensions.
Jolt consumes the local pose while body state retains the body origin.
All packages and consumers are rebuilt together for the descriptor layout
addition; this is a local install, not a published artifact pin.

The [isolated consumer](../../tests/physics_consumer/main.cpp), over MMD
baseline `8c5875a`, keeps both prior controls and adds a translated/rotated
child Cube control. It verifies local translation and orientation, unchanged
dimensions and unchanged body identity/world transform. Dimensions allow
`1e-9` for rotation decomposition roundoff. It still links only installed
`physicsUsd::physicsUsd`; the importer is unchanged.

## Results

Windows x86_64, MSVC 19.51, Visual Studio 18 2026, OpenUSD 26.08 and
Python 3.13.14. The shared physics suite passes all 12 tests, including
eight-corner USD geometry checks, Jolt ray queries and contact settling with
offset geometry, body-origin changed states, negative fixtures and installed
consumers. Hosted verification remains pending.

The installed `physicsUsd.lib` has SHA-256
`8dd9842bad3c2f76422e086ba0faf5a71ecbd4fbfefeecb087ba4e49bbbd02ab`.

| Input | Count | Result |
| --- | ---: | --- |
| Original dynamic Cube control | 1 | Accepted |
| Centered parent-body/child-Cube control | 1 | Accepted with composed body pose and dimensions |
| Offset/rotated child-Cube control | 1 | Accepted with distinct collider-local and body-world poses |
| Full PMX fixtures with rigid bodies | 22 | Rejected with `unsupportedDeclaration` at `/Asset/physics/joints/joint_0000` |
| PMX fixtures without rigid bodies | 6 | Accepted with an empty snapshot |

The process exits zero after reporting these typed outcomes. Declaration
validation still rejects joints before body conversion; the full PMX results
establish no body support. No PMX simulation was performed.

## Reproduction

Build and install core/backend/reader together into a new prefix; this run
used `build/phase8-local-pose-prefix` in the physics repository. Set
`PHYSICS_PREFIX` to it and `USD_ROOT` to the matching SDK:

```powershell
cmake -S tests/physics_consumer -B build/physics-consumer-local-pose `
  -G 'Visual Studio 18 2026' -A x64 `
  "-DCMAKE_PREFIX_PATH=$env:PHYSICS_PREFIX;$env:USD_ROOT"
cmake --build build/physics-consumer-local-pose --config Release
$env:PATH = "$env:USD_ROOT/bin;$env:USD_ROOT/lib;$env:PATH"
$env:PXR_PLUGINPATH_NAME = "$PWD/plugins/mmdSchema/plugin/resources/mmdSchema;$PWD/plugins/usdMmdFileFormat/plugin/resources/usdMmdFileFormat"
$fixtureRoot = Join-Path $PWD 'plugins/usdMmdFileFormat/tests/fixtures'
$manifest = Get-Content -Raw (Join-Path $fixtureRoot 'fixtures.json') | ConvertFrom-Json -AsHashtable
$stagePaths = @($manifest.Keys | Sort-Object | Where-Object { $manifest[$_].opens } | ForEach-Object { Join-Path $fixtureRoot $_ })
& build/physics-consumer-local-pose/Release/mmd_physics_consumer_probe.exe @stagePaths `
  2> build/physics-consumer-local-pose/diagnostics.log |
  Tee-Object -FilePath build/physics-consumer-local-pose/results.tsv
```

Earlier prefixes remain separate. All inputs are committed synthetic
fixtures; no third-party assets are redistributed.

## Next boundary

Add sphere/capsule shapes and kinematic operations through the shared
core/backend, then joint frames and limits. Rerun the unchanged full-PMX
probe after each installed increment. MMD bone/body coupling stays deferred
until generic body and synchronization contracts are concrete.
