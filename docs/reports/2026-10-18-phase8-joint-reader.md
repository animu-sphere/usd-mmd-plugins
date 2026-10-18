# Phase 8 joint snapshot reader follow-up (2026-10-18)

Follow-up: [primitive mass inference evidence](2026-10-18-phase8-mass-reader.md).

Dated local Windows evidence following the
[kinematic reader probe](2026-10-18-phase8-kinematic-reader.md).
This validates joint declaration conversion, not PMX simulation.

## Change under test

Local `usd-physics-plugins` changes over `01b727c` add `readPhysicsScene()`
with primitive bodies and generic/fixed joint snapshots. Joint endpoints are
exact rigid-body paths or a world anchor. Local frames retain body origins,
quaternions normalize, and standard six-axis limits convert angular degrees
to radians while retaining linear meters and free/locked/infinite bounds.
The original body-only APIs still reject joints. No generic six-DOF runtime
constraint implementation is added.

The [isolated consumer](../../tests/physics_consumer/main.cpp) links only
installed `physicsUsd::physicsUsd`. Its eight existing controls remain and a
new two-body control checks joint endpoints, body-local frame translations,
free/equal linear limits and angular units. For accepted PMX stages it checks
body/joint counts and reconstructs both world joint frames from neutral body
and local joint poses, agreeing within `1e-6` per matrix element.
It separately counts format-only joints that the generic reader cannot see.
No importer stage or published artifact pin changes.

## Results

Windows x86_64, MSVC 19.51, Visual Studio 18 2026, OpenUSD 26.08 and
Python 3.13.14. All 14 shared physics tests pass, including the focused joint
snapshot suite and clean-prefix installed consumers. Hosted checks remain
pending.

Installed `physicsUsd.lib` SHA-256:
`41df83f6125ba50eaf9a356f78004e46e058661fe20f3f1f6f4e46c70e4d56a0`.

| Input | Count | Result |
| --- | ---: | --- |
| Previous primitive/kinematic controls | 8 | Accepted |
| Two-body joint frame/limit control | 1 | Accepted: two bodies, one joint |
| PMX fixtures with missing dynamic mass | 21 | Rejected: `invalidValue` at `/Asset/physics/rigidBodies/chest` |
| `recoverable/physics-repairs.pmx` | 1 | Three bodies/two standard joints accepted; one format-only joint excluded |
| PMX fixtures without rigid bodies | 6 | Accepted with empty body/joint snapshots |

The process exits zero after recording typed outcomes. The 21 rejected stages
omit `physics:mass` on a dynamic body, so USD's default zero requests mass
inference, which this reader does not implement. No arbitrary positive mass
is substituted. The accepted fixture demonstrates standard body/joint
snapshots only: its remaining format-only joint and all preserved MMD
coefficients, filtering, spring and bone-coupling semantics remain outside
generic conversion. This is not full-PMX physics acceptance.

## Reproduction

Build/install the shared packages into a separate prefix (this run used
`build/phase8-joint-prefix` in that repository), then set `PHYSICS_PREFIX`
and `USD_ROOT` to that prefix and the matching SDK:

```powershell
cmake -S tests/physics_consumer -B build/physics-consumer-joints `
  -G 'Visual Studio 18 2026' -A x64 `
  "-DCMAKE_PREFIX_PATH=$env:PHYSICS_PREFIX;$env:USD_ROOT"
cmake --build build/physics-consumer-joints --config Release
$env:PATH = "$env:USD_ROOT/bin;$env:USD_ROOT/lib;$env:PATH"
$env:PXR_PLUGINPATH_NAME = "$PWD/plugins/mmdSchema/plugin/resources/mmdSchema;$PWD/plugins/usdMmdFileFormat/plugin/resources/usdMmdFileFormat"
$fixtureRoot = Join-Path $PWD 'plugins/usdMmdFileFormat/tests/fixtures'
$manifest = Get-Content -Raw (Join-Path $fixtureRoot 'fixtures.json') | ConvertFrom-Json -AsHashtable
$stagePaths = @($manifest.Keys | Sort-Object | Where-Object { $manifest[$_].opens } | ForEach-Object { Join-Path $fixtureRoot $_ })
& build/physics-consumer-joints/Release/mmd_physics_consumer_probe.exe @stagePaths `
  2> build/physics-consumer-joints/diagnostics.log |
  Tee-Object -FilePath build/physics-consumer-joints/results.tsv
```

Inputs are unchanged committed synthetic fixtures. Earlier prefixes stay
separate. The importer remains independent of the shared physics package.

## Next boundary

Define generic USD mass inference with fixtures, or otherwise supply explicit
mass in an authorized runtime composition layer. Add solver-neutral six-DOF
execution and progressively resolve coefficients/filtering and source-only
semantics. MMD synchronization stays deferred until generic scene consumption
and synchronization contracts are concrete; compatible public package
publication remains open.
