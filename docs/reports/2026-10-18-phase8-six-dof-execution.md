# Phase 8 optional six-DOF execution follow-up (2026-10-18)

Later evidence: [MMD collision filtering](2026-10-18-phase8-collision-filter.md) is now exercised by the isolated consumer.

Dated Windows evidence after the [mass reader probe](2026-10-18-phase8-mass-reader.md).
This is bounded standard-USD execution, not MMD simulation equivalence.

## Change under test

Local `usd-physics-plugins` adds neutral six-axis descriptors, an optional
`SixDofConstraints` capability implemented by Jolt, and stateless
`physicsUsd::toSixDofConstraint()` conversion. Frames use body origins,
including displaced colliders; one world endpoint is allowed. Axes may be
free, locked at zero, or finite limited ranges. Angular execution uses X twist
and Y/Z pyramid swing, not a claim of PMX Euler-limit equivalence. Joint pair
collision flags are honored; source collision groups/masks remain unmapped.
Springs, drives, nonzero equal-bound locks and one-sided infinite execution
ranges remain outside this increment. The existing declaration reader keeps
its wider snapshot subset.

The shared backend capability is experimental and defaults OFF through
`PHYSICSJOLT_ENABLE_SIX_DOF`. The isolated consumer adds an independent
`MMD_PHYSICS_EXECUTION=ON` option, linking only installed `physicsUsd` and
`physicsJolt` packages. Its default reader-only mode remains available. After
the existing counts and coincident world-frame checks, execution mode creates
body/constraint resources, runs 120 fixed steps at 1/120 s, and checks every
reported position, orientation and linear velocity for finiteness. Rejections
now make the probe exit nonzero. The twelve reader controls remain unchanged.

## SDK gate and results

Windows x86_64, MSVC 19.51, OpenUSD 26.08, Python 3.13.14, Jolt 5.5.0.
The previous local scalar/non-deterministic Jolt SDK failed an added Z-axis
load test: the declared upper bound was 0.4 m but observed displacement peaked
near 0.0624 m while velocity grew. Generation and finite-state checks alone
did not detect this. A separate SDK build with
`CROSS_PLATFORM_DETERMINISTIC=ON`, upstream default x86 SIMD features and
dynamic MSVC runtime passes all axis tests. Multiple SDK settings changed;
the precise cause is not isolated. The old SDK is not accepted for enabling
this experimental feature. Public pins remain unchanged.

With the passing SDK, all 18 shared tests pass, including per-axis loads,
rotated frames, offset centers of mass, world locks, collision suppression and
restoration, handle cleanup and clean-prefix consumers.

| Input | Count | Result |
| --- | ---: | --- |
| Reader controls | 12 | Accepted |
| Physics-bearing PMX fixtures | 22 | Three bodies/two constraints each; 120 steps with finite states |
| Physics-free PMX fixtures | 6 | Empty snapshots; 120 empty steps |

The process exits zero with no reader/conversion rejection. One format-only
joint in `recoverable/physics-repairs.pmx` remains outside execution. Kinematic
bodies hold their initial poses; no bone motion is sampled. Default backend
coefficients/filtering are used except standard joint pair collision flags.
These checks establish resource creation and finite stepping, not correct PMX
trajectories, springs, coefficients, filtering, zero-mass behavior or bone
feedback. Hosted validation remains pending.

Installed SHA-256 values:

| Library | SHA-256 |
| --- | --- |
| `physicsCore.lib` | `50bf030db053d67ca5044211277f3dcde0526bd286bf1b0b0502b48c18f0a34a` |
| `physicsJolt.lib` | `dd10011da5fc52799351a4772e377a24e6e632295228dc5ad2cf3d496e951937` |
| `physicsUsd.lib` | `99aa70a8ddb16d53fbff96c4f7dbbca54455a879e0d9908b729832016d11fa9a` |

## Reproduction

Build/install the shared packages with `PHYSICSJOLT_ENABLE_SIX_DOF=ON` and a
validated SDK. Set `PHYSICS_PREFIX`, `USD_ROOT` and `JOLT_ROOT` to those
installed prefixes (the local shared run used `build/phase8-six-dof-prefix`):

```powershell
cmake -S tests/physics_consumer -B build/physics-consumer-six-dof `
  -G 'Visual Studio 18 2026' -A x64 `
  "-DCMAKE_PREFIX_PATH=$env:PHYSICS_PREFIX;$env:USD_ROOT" `
  "-DJolt_DIR=$env:JOLT_ROOT/lib/cmake/Jolt" -DMMD_PHYSICS_EXECUTION=ON
cmake --build build/physics-consumer-six-dof --config Release
$env:PATH = "$env:USD_ROOT/bin;$env:USD_ROOT/lib;$env:PATH"
$env:PXR_PLUGINPATH_NAME = "$PWD/plugins/mmdSchema/plugin/resources/mmdSchema;$PWD/plugins/usdMmdFileFormat/plugin/resources/usdMmdFileFormat"
$fixtureRoot = Join-Path $PWD 'plugins/usdMmdFileFormat/tests/fixtures'
$manifest = Get-Content -Raw (Join-Path $fixtureRoot 'fixtures.json') | ConvertFrom-Json -AsHashtable
$stagePaths = @($manifest.Keys | Sort-Object | Where-Object { $manifest[$_].opens } | ForEach-Object { Join-Path $fixtureRoot $_ })
& build/physics-consumer-six-dof/Release/mmd_physics_consumer_probe.exe @stagePaths `
  2> build/physics-consumer-six-dof/diagnostics.log |
  Tee-Object -FilePath build/physics-consumer-six-dof/results.tsv
```

No importer contract, normal build dependency or fixture bytes change. Next:
resolve SDK configuration/hosted delivery, coefficients, source filtering and
springs; define runtime synchronization before MMD bone/body coupling.
