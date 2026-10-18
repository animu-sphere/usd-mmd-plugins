# Phase 8 kinematic reader follow-up (2026-10-18)

Dated local Windows evidence following the
[Sphere/Capsule probe](2026-10-18-phase8-primitive-reader.md).
This validates a bounded shared-reader increment, not PMX simulation.

## Change under test

Local `usd-physics-plugins` changes over `81905cf` add neutral kinematic body
descriptors and an optional `KinematicMotion` capability implemented by Jolt.
It moves to a world-space body-origin target through velocity commands;
offset colliders retain their local frames. The caller supplies duration and
step order. Both snapshot APIs map time-independent standard kinematic flags.
Enabled kinematic bodies need no mass; disabled bodies remain static.
Core, backend, bridge and consumers are rebuilt together in a separate local
prefix. No published artifact pin changes.

The [isolated consumer](../../tests/physics_consumer/main.cpp) retains its five
previous controls and adds mass-free child Cube/Sphere/Capsule controls with
`physics:kinematicEnabled = true`. It checks motion type, body identity,
composed world transform and collider-local translation. It continues to link
only installed `physicsUsd::physicsUsd`. Generic backend motion is tested in
the shared repository; no MMD bone coupling or simulation is added here.

## Results

Windows x86_64, MSVC 19.51, Visual Studio 18 2026, OpenUSD 26.08 and
Python 3.13.14. All 13 shared physics tests pass, including target movement,
rotation with offset geometry, holding, moving-platform contact with a dynamic
body, changed states, invalid commands and clean-prefix installed consumers.
Hosted verification remains pending.

Installed `physicsUsd.lib` SHA-256:
`dcf264db958377b89eb42c07ce9170843eef174cc71fb2305f4090620a75142d`.

| Input | Count | Result |
| --- | ---: | --- |
| Existing direct/child/offset Box and child Sphere/Capsule controls | 5 | Accepted |
| Mass-free kinematic child Cube/Sphere/Capsule controls | 3 | Accepted with distinct body and local collider poses |
| Full PMX fixtures with rigid bodies | 22 | Rejected with `unsupportedDeclaration` at `/Asset/physics/joints/joint_0000` |
| PMX fixtures without rigid bodies | 6 | Accepted with an empty snapshot |

The process exits zero after reporting typed outcomes. Joint declaration
validation still precedes body conversion. These full PMX results establish
no full-body or simulation acceptance; no importer or stage-contract change
was needed.

## Reproduction

Build/install the shared packages together into a separate prefix; this run
used `build/phase8-kinematic-prefix` in the physics repository. Set
`PHYSICS_PREFIX` to it and `USD_ROOT` to the matching SDK:

```powershell
cmake -S tests/physics_consumer -B build/physics-consumer-kinematic `
  -G 'Visual Studio 18 2026' -A x64 `
  "-DCMAKE_PREFIX_PATH=$env:PHYSICS_PREFIX;$env:USD_ROOT"
cmake --build build/physics-consumer-kinematic --config Release
$env:PATH = "$env:USD_ROOT/bin;$env:USD_ROOT/lib;$env:PATH"
$env:PXR_PLUGINPATH_NAME = "$PWD/plugins/mmdSchema/plugin/resources/mmdSchema;$PWD/plugins/usdMmdFileFormat/plugin/resources/usdMmdFileFormat"
$fixtureRoot = Join-Path $PWD 'plugins/usdMmdFileFormat/tests/fixtures'
$manifest = Get-Content -Raw (Join-Path $fixtureRoot 'fixtures.json') | ConvertFrom-Json -AsHashtable
$stagePaths = @($manifest.Keys | Sort-Object | Where-Object { $manifest[$_].opens } | ForEach-Object { Join-Path $fixtureRoot $_ })
& build/physics-consumer-kinematic/Release/mmd_physics_consumer_probe.exe @stagePaths `
  2> build/physics-consumer-kinematic/diagnostics.log |
  Tee-Object -FilePath build/physics-consumer-kinematic/results.tsv
```

Earlier prefixes remain separate; inputs are committed synthetic fixtures.

## Next boundary

Add generic joint frames and limits, then rerun the unchanged full-PMX probe.
Physical coefficients and filtering fidelity also remain. Keep MMD bone/body
coupling deferred until generic scene consumption and synchronization
contracts are concrete. Compatible public package publication remains open.
