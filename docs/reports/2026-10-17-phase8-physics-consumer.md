# Phase 8 first generic physics consumer (2026-10-17)

Forward note: the [2026-10-18 follow-up](2026-10-18-phase8-child-box-reader.md)
tests a bounded child Box reader increment; the current probe adds a second
positive control. Use source `d3c6da7` to reproduce this report's original probe.

Dated local Windows evidence; append-only. This is a contract probe, not
simulation acceptance.

## Question

Can the current installed `physicsUsd` read the existing
[/Asset/physics contract](../design/STAGE_CONTRACT.md#13-physics) without
backend metadata or changes to the importer?

## What was run

The isolated [probe](../../tests/physics_consumer/main.cpp) links only to
`physicsUsd::physicsUsd`, found as an installed CMake package with compatible
version 0.1. It first verifies a dynamic collision Cube with mass 2 kg and a
translation, then opens every `opens: true` PMX in the committed fixture
manifest through Plug and calls `usd_physics::usd::readBoxScene()` on the
unmodified stage. No solver is created and no USD is edited.

Environment: Windows x86_64, MSVC 19.51, Visual Studio 18 2026, OpenUSD 26.08,
Python 3.13.14. MMD source baseline: `8aaff35`. The physics source reviewed
was `3daad33`; the local installed `physicsUsd.lib` used for execution had
SHA-256 `43a8ea720d19b8d58dd6efedc0967c40a4d7f3ed193508ea533e2a102862069d`.
This is a local artifact, not a published package pin. The direct PMX open
reports stage-contract version 2.

## Results

| Input | Count | Result |
| --- | ---: | --- |
| Positive dynamic Cube control | 1 | One validated body snapshot, with the expected mass, half extent and translation |
| PMX fixtures with rigid bodies | 22 | Every input rejects with `ImportErrorCode::unsupportedDeclaration` at `/Asset/physics/rigidBodies/head` |
| PMX fixtures without rigid bodies | 6 | Every input returns an empty snapshot |

The first rejection says `expected CollisionAPI on a Cube at the body prim`.
Each physics-bearing fixture has three rigid bodies; its first is a body
Xform with a sphere child collider. The reader rejects the body prim before
it reaches that child. An accepted empty snapshot is not MMD physics support.
The process exits zero when it has reported all observations, including
typed rejections; it exits nonzero for a failed positive control, an input
that cannot open, or an unexpected exception.

## Further gaps from source inspection

Only the hierarchy failure above was reached by the full fixture run. The
following additional limitations are established by inspection of
`libs/physicsUsd/src/box_scene.cpp`, its public `box_scene.h`, and the core
descriptor surface in `usd-physics-plugins`, not by successful simulation:

| Existing authored meaning | Current shared reader/core gap |
| --- | --- |
| Body Xform plus child collider | Collision must be on the body Cube; body/shape association and collider-local transforms are absent |
| Rest translation and orientation | Reader admits translate followed by scales only; composed rigid orientation is not supported |
| Sphere, box and Y-axis capsule | Reader and core shape descriptor admit boxes only |
| `followBone` as kinematic | Reader rejects kinematic bodies; core motion types are static and dynamic only |
| Generic 6-DOF joints with local frames and per-axis limits | Reader rejects joints; core constraints currently admit fixed pairs only |
| PMX group/mask and physical coefficients | Box snapshot carries no filter, damping, friction or restitution fields; core has neutral filters, but MMD interpretation remains an adapter concern |
| A source mass that is not positive | Dynamic Box reader requires explicit positive mass; runtime inference/diagnostics must be decided without inventing source mass |

The importer already uses conventional USD for the hierarchy and shapes.
This run demonstrates the reader's bounded subset, not a defect that calls
for a stage-contract change.

## Next implementation boundary

1. Extend the generic bridge with fixtures for a body Xform and child Cube,
   including composed rest orientation and collider-local transforms.
2. Add sphere/capsule and kinematic capabilities to the shared core/backend
   and reader, keeping tolerances in backend tests.
3. Add joint frames and limits incrementally, reporting fidelity gaps.
4. Rerun this probe against the installed packages. Fix an optional MMD
   coupling API only when the shared runtime can represent the required
   bodies and synchronization operations.

MMD bone association, modes, masks and feedback stay in the MMD adapter;
generic geometry, transforms, stepping and backend creation stay in the
shared physics repository. The [runtime sequence](../design/PHYSICS_INTEGRATION.md#9-implementation-sequence)
has not advanced past generic scene consumption.

## Reproduction

PowerShell from the repository root. Set `USD_ROOT` to an OpenUSD 26.08 SDK
and `PHYSICS_PREFIX` to an installed compatible `physicsUsd`/`physicsCore`
prefix. The importer and schema bundles must already be built for that SDK.
The probe is deliberately outside the normal workspace and release graph.

```powershell
cmake -S tests/physics_consumer -B build/physics-consumer `
  -G 'Visual Studio 18 2026' -A x64 `
  "-DCMAKE_PREFIX_PATH=$env:PHYSICS_PREFIX;$env:USD_ROOT"
cmake --build build/physics-consumer --config Release
$env:PATH = "$env:USD_ROOT/bin;$env:USD_ROOT/lib;$env:PATH"
$env:PXR_PLUGINPATH_NAME = "$PWD/plugins/mmdSchema/plugin/resources/mmdSchema;$PWD/plugins/usdMmdFileFormat/plugin/resources/usdMmdFileFormat"
$fixtureRoot = Join-Path $PWD 'plugins/usdMmdFileFormat/tests/fixtures'
$manifest = Get-Content -Raw (Join-Path $fixtureRoot 'fixtures.json') | ConvertFrom-Json -AsHashtable
$stagePaths = @($manifest.Keys | Sort-Object | Where-Object { $manifest[$_].opens } | ForEach-Object { Join-Path $fixtureRoot $_ })
& build/physics-consumer/Release/mmd_physics_consumer_probe.exe @stagePaths |
  Tee-Object -FilePath build/physics-consumer/results.tsv
```

The TSV is local output; importer diagnostics remain on stderr. This run
uses only committed synthetic fixtures and redistributes no third-party
models or screenshots.
