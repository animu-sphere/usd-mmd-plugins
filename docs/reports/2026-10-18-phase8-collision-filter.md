# Phase 8 MMD collision-filter consumer follow-up (2026-10-18)

Local Windows evidence after the [six-DOF execution probe](2026-10-18-phase8-six-dof-execution.md)
and shared hosted acceptance recorded in
[PHYSICS_INTEGRATION §9](../design/PHYSICS_INTEGRATION.md#9-implementation-sequence).

## Change under test

The isolated consumer translates the stage's `mmd:physics:collisionGroup`
and `mmd:physics:collisionMask` into the installed core's existing
`CollisionFilter`: category `1 << group`, acceptance mask copied into the
low 16 bits. The pair rule requires mutual acceptance. This follows
[STAGE_CONTRACT §13.1](../design/STAGE_CONTRACT.md#131-rigid-bodies).
As an independent implementation check, three.js r168's
[PMX parser](https://github.com/mrdoob/three.js/blob/r168/examples/jsm/libs/mmdparser.module.js#L10937-L10958)
reads the unsigned 16-bit mask as `groupTarget`, and its
[MMD physics adapter](https://github.com/mrdoob/three.js/blob/r168/examples/jsm/animation/MMDPhysics.js#L915)
passes it directly alongside `1 << groupIndex` to Bullet. No inversion is
applied, despite the structural parser field's `nonCollisionMask` name.

The helper remains test-only in `tests/physics_consumer/collision_filter.h`.
It rejects partial metadata, wrong attribute types, missing values,
out-of-range groups/masks and time samples. Conventional bodies with neither
MMD attribute retain the shared default. The reader-only probe validates
filters; execution passes them into `BodyDescriptor` before creating bodies.
Neither the stage nor the installed generic reader is modified.

## Results

Windows x86_64, MSVC 19.51, OpenUSD 26.08, Python 3.13.14, the same installed
shared packages and deterministic Jolt 5.5.0 SDK as the execution report.

| Check | Result |
| --- | --- |
| Reader-only CMake build and `mmd_physics_collision_filter` CTest | Passed |
| Execution-enabled CMake build and the same CTest | Passed |
| Filter controls | All 16 groups; zero/all/high-bit masks; asymmetric pair rejection; same-group distinct masks; invalid declarations; no stage edits |
| Contact controls | Six passed: admitted drop; rejection by either body's mask; same-group admitted/rejected drops; two same-group bodies with different masks in one world |
| Existing reader controls | All twelve accepted in both builds |
| Physics-bearing PMX fixtures | All 22 accepted in both builds; three filters/bodies, two constraints, 120 finite steps each in execution mode |
| Physics-free fixtures | All six empty snapshots; 120 empty steps each in execution mode |

Contact controls run 240 fixed steps at 1/120 s. Admitted boxes settle with
their body origins within 3 cm of the expected 1 m height and vertical speed
below 0.05 m/s; rejected boxes fall below -1 m. They use no joint, so joint
collision suppression cannot conceal an incorrect source mask. The mixed-mask
control checks both outcomes in one backend world.

## Reproduction

Set `PHYSICS_PREFIX`, `USD_ROOT` and `JOLT_ROOT` to the installed prefixes
described in the [execution report](2026-10-18-phase8-six-dof-execution.md#reproduction).

```powershell
cmake -S tests/physics_consumer -B build/physics-consumer-filters `
  -G 'Visual Studio 18 2026' -A x64 `
  "-DCMAKE_PREFIX_PATH=$env:PHYSICS_PREFIX;$env:USD_ROOT" `
  -DMMD_PHYSICS_EXECUTION=OFF
cmake --build build/physics-consumer-filters --config Release
$env:PATH = "$env:USD_ROOT/bin;$env:USD_ROOT/lib;$env:PATH"
ctest --test-dir build/physics-consumer-filters -C Release --output-on-failure
```

The existing execution build exercises actual contact controls:

```powershell
cmake --build build/physics-consumer-six-dof --config Release
ctest --test-dir build/physics-consumer-six-dof -C Release --output-on-failure
$env:PXR_PLUGINPATH_NAME = "$PWD/plugins/mmdSchema/plugin/resources/mmdSchema;$PWD/plugins/usdMmdFileFormat/plugin/resources/usdMmdFileFormat"
$fixtureRoot = Join-Path $PWD 'plugins/usdMmdFileFormat/tests/fixtures'
$manifest = Get-Content -Raw (Join-Path $fixtureRoot 'fixtures.json') | ConvertFrom-Json -AsHashtable
$stagePaths = @($manifest.Keys | Sort-Object | Where-Object { $manifest[$_].opens } | ForEach-Object { Join-Path $fixtureRoot $_ })
& build/physics-consumer-six-dof/Release/mmd_physics_consumer_probe.exe @stagePaths `
  2> build/physics-consumer-six-dof/filter-diagnostics.log |
  Tee-Object -FilePath build/physics-consumer-six-dof/filter-results.tsv
```

Run the reader-only executable against the same paths to reproduce its
snapshot results. Every command above completed successfully locally.

## Remaining boundary

This establishes local source-filter conversion and backend contact behavior,
not full MMD simulation fidelity. Coefficients still use backend defaults;
the installed core body descriptor has no friction, restitution or damping
fields. Springs, one format-only joint, PMX angular/zero-mass equivalence,
bone synchronization and mixed-format category allocation remain open.
Hosted MMD-filter acceptance and published compatible packages also remain.
No importer contract, fixture bytes, normal workspace dependency or public
runtime-adapter API changes.
