# Phase 8 MMD contact-coefficient follow-up (2026-10-18)

Local Windows evidence after the [collision-filter consumer](2026-10-18-phase8-collision-filter.md).
This is bounded consumption of preserved friction/restitution, not full MMD
simulation fidelity.

## Change under test

The locally rebuilt `usd-physics-plugins` core adds neutral `ContactMaterial`
on `BodyDescriptor`, validated independently and during body creation. Its
Jolt backend passes friction/restitution into native body settings. Default
coefficients retain the previous behavior. The generic USD reader still
interprets no MMD metadata.

The isolated consumer's `contact_material.h` reads the stage's existing
`mmd:physics:friction` and `mmd:physics:restitution` constants. It rejects
partial pairs, wrong types, missing/blocked values, samples, non-finite or
negative coefficients, and restitution above one. Friction above one is
preserved. Conventional bodies with neither attribute retain shared defaults.
Execution passes the result beside the existing collision filter when creating
each body. No imported stage, normal dependency or fixture bytes change.

## Results

Windows x86_64, MSVC 19.51, OpenUSD 26.08, Python 3.13.14, deterministic
Jolt 5.5.0. Shared core/backend/bridge packages and all consumers were rebuilt
together into a separate local prefix; the body descriptor layout changes.

| Check | Result |
| --- | --- |
| Shared package CTests, including clean-prefix consumer | All 19 pass |
| Shared no-Jolt build and CTests | All 11 pass |
| MMD reader-only and execution builds | Both pass two CTests: filter and material |
| Source material controls | Values, domains, missing/blocked/wrongly typed/sampled metadata, conventional defaults, no stage edits |
| MMD contact-response controls | Four pass: free/rough sliding, inelastic/elastic bounce |
| Existing contact filter controls | All six still pass |
| Existing generic reader controls | All twelve accepted in both modes |
| Physics-bearing PMX fixtures | All 22 accepted in both modes; three materials/bodies and two standard joints each; execution creates both constraints and steps 120 times with finite state |
| Physics-free PMX fixtures | Six empty snapshots; 120 empty steps each in execution mode |

The response controls use the MMD metadata reader before backend body
creation. A box starts at 5 m/s; after 0.5 s it retains 4.87655 m/s with zero
friction, versus 0.0333689 m/s with friction one. A sphere dropped from a
center height of 3 m has maximum upward speed zero with restitution zero,
versus 5.52308 m/s with restitution 0.8. Shared backend tests independently
cover unequal pairs and either body's coefficient, plus scalar-range rejection
and resource cleanup. These observations establish coefficient effects, not
correct PMX trajectories.

## Reproduction

Build/install the updated shared packages with experimental six-DOF enabled
and the deterministic SDK, as in the [execution report](2026-10-18-phase8-six-dof-execution.md#reproduction).
Set `PHYSICS_PREFIX` to that new prefix, `USD_ROOT` to OpenUSD and `JOLT_ROOT`
to the deterministic SDK install. Older prefixes lack the new descriptor.

```powershell
cmake -S tests/physics_consumer -B build/physics-consumer-contact `
  -G 'Visual Studio 18 2026' -A x64 `
  "-DCMAKE_PREFIX_PATH=$env:PHYSICS_PREFIX;$env:USD_ROOT" `
  "-DJolt_DIR=$env:JOLT_ROOT/lib/cmake/Jolt" -DMMD_PHYSICS_EXECUTION=ON
cmake --build build/physics-consumer-contact --config Release
$env:PATH = "$env:USD_ROOT/bin;$env:USD_ROOT/lib;$env:PATH"
ctest --test-dir build/physics-consumer-contact -C Release --output-on-failure

cmake -S tests/physics_consumer -B build/physics-consumer-contact-reader `
  -G 'Visual Studio 18 2026' -A x64 `
  "-DCMAKE_PREFIX_PATH=$env:PHYSICS_PREFIX;$env:USD_ROOT" -DMMD_PHYSICS_EXECUTION=OFF
cmake --build build/physics-consumer-contact-reader --config Release
ctest --test-dir build/physics-consumer-contact-reader -C Release --output-on-failure
```

With the fixture `$stagePaths` and plugin environment from the
[filter report](2026-10-18-phase8-collision-filter.md#reproduction):

```powershell
& build/physics-consumer-contact/Release/mmd_physics_consumer_probe.exe @stagePaths `
  2> build/physics-consumer-contact/diagnostics.log |
  Tee-Object -FilePath build/physics-consumer-contact/results.tsv
& build/physics-consumer-contact-reader/Release/mmd_physics_consumer_probe.exe @stagePaths `
  2> build/physics-consumer-contact-reader/diagnostics.log |
  Tee-Object -FilePath build/physics-consumer-contact-reader/results.tsv
```

All commands above completed successfully locally.

## Remaining boundary

Jolt combines friction by geometric mean and restitution by maximum; source
coefficients alone do not establish PMX/Bullet pair-mixing equivalence.
Linear/angular damping still use backend defaults. Springs, one format-only
joint, PMX angular/zero-mass fidelity, bone synchronization and mixed-format
category allocation remain open. Hosted MMD acceptance and compatible
published packages also remain. No public runtime-adapter API is introduced.
