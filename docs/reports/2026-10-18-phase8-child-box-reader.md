# Phase 8 child Box reader follow-up (2026-10-18)

Follow-up: [collider-local pose consumer evidence](2026-10-18-phase8-collider-local-pose.md).

Dated local Windows evidence; append-only. This follows the
[first consumer probe](2026-10-17-phase8-physics-consumer.md). It validates a
bounded generic reader increment, not PMX simulation acceptance.

## Change under test

Local `usd-physics-plugins` changes over baseline `3daad33` accept a rigid
body Xform with one centered axis-aligned child Cube. Body transforms compose
through rotated/translated ancestors; child Cube scale becomes shape
dimensions. Direct collision Cubes also admit composed transforms. Body path,
mass and body frame are preserved without moving the frame to the collider.

The core's shape descriptor still has no collider-local pose. Offsets and
rotations below a separate body therefore reject explicitly. Sphere/capsule
geometry, kinematic motion, joints/limits and physical coefficients are not
implemented by this increment.

The isolated [consumer](../../tests/physics_consumer/main.cpp), over MMD
baseline `25a09ee`, adds a second positive control: a body Xform with explicit
3 kg mass, a scaled child Cube and a rotated/translated ancestor. It checks
body identity, dimensions, world translation and orientation beside the
original dynamic Cube control. It still links only installed
`physicsUsd::physicsUsd`; the importer and its stage contract are unchanged.

## Results

Windows x86_64, MSVC 19.51, Visual Studio 18 2026, OpenUSD 26.08 and
Python 3.13.14. The new installed `physicsUsd.lib` has SHA-256
`f1c0b7005024cdd70c149d9c86b80a5e8859a3d7d1b4e94df6478cf8de6171c9`.
This is a local build, not a published artifact pin.

| Input | Count | Result |
| --- | ---: | --- |
| Original dynamic Cube control | 1 | Accepted with the expected body snapshot |
| Parent-body/child-Cube control | 1 | Accepted with body identity, mass, composed pose and scaled dimensions |
| Full PMX fixtures with rigid bodies | 22 | Rejected with `unsupportedDeclaration` at `/Asset/physics/joints/joint_0000` |
| PMX fixtures without rigid bodies | 6 | Accepted with an empty snapshot |

The reader now validates declarations before converting body candidates, so
joint API rejection precedes geometry conversion. The changed rejection path
does not prove that any full PMX body's geometry, mass or mode was accepted.
The process exits zero after reporting these typed outcomes. The physics
repository's full 12-test suite also passes, including its Jolt backend,
eight-corner USD geometry checks, negative transform/hierarchy fixtures and
clean-prefix installed consumers. No PMX solver was created.

## Reproduction

Build and install the updated shared packages into a separate local prefix;
the run used `build/phase8-box-prefix` in the physics repository. Set
`PHYSICS_PREFIX` to that installed prefix and `USD_ROOT` to the matching
OpenUSD SDK. From this repository root:

```powershell
cmake -S tests/physics_consumer -B build/physics-consumer-child-box `
  -G 'Visual Studio 18 2026' -A x64 `
  "-DCMAKE_PREFIX_PATH=$env:PHYSICS_PREFIX;$env:USD_ROOT"
cmake --build build/physics-consumer-child-box --config Release
$env:PATH = "$env:USD_ROOT/bin;$env:USD_ROOT/lib;$env:PATH"
$env:PXR_PLUGINPATH_NAME = "$PWD/plugins/mmdSchema/plugin/resources/mmdSchema;$PWD/plugins/usdMmdFileFormat/plugin/resources/usdMmdFileFormat"
$fixtureRoot = Join-Path $PWD 'plugins/usdMmdFileFormat/tests/fixtures'
$manifest = Get-Content -Raw (Join-Path $fixtureRoot 'fixtures.json') | ConvertFrom-Json -AsHashtable
$stagePaths = @($manifest.Keys | Sort-Object | Where-Object { $manifest[$_].opens } | ForEach-Object { Join-Path $fixtureRoot $_ })
& build/physics-consumer-child-box/Release/mmd_physics_consumer_probe.exe @stagePaths `
  2> build/physics-consumer-child-box/diagnostics.log |
  Tee-Object -FilePath build/physics-consumer-child-box/results.tsv
```

The earlier installed Box reader fails the new child control, as intended.
Keep the original probe/build for historical reproduction. All input is
committed synthetic data; no third-party assets are redistributed.

## Next boundary

Represent collider-local poses neutrally before supporting their offsets and
rotations. Add sphere/capsule shapes and kinematic operations through the
shared core/backend, then joint frames and limits. Rerun the unmodified PMX
probe after each installed increment. The MMD coupling adapter remains
deferred until those generic body and synchronization contracts are concrete.
