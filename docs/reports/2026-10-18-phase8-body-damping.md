# Phase 8 MMD body damping follow-up (2026-10-18)

Local Windows evidence after [contact coefficients](2026-10-18-phase8-contact-material.md).
This is bounded consumption of saved linear/angular damping, with explicit
source conversion and a finite-step approximation.

## Change under test

Locally rebuilt shared packages append neutral `BodyDamping` to
`BodyDescriptor`: independent inverse-second linear/angular decay rates,
finite and non-negative. Defaults preserve the previous Jolt `0.05` values.
Jolt consumes the rates at body creation and rejects unsafe scalar conversions.
All shared packages and consumers rebuild together into a separate prefix.

The test-only `body_damping.h` reads the existing constant `float` attributes
`mmd:physics:linearDamping` and `mmd:physics:angularDamping`. It rejects partial
pairs, wrong types, missing/blocked values, samples and invalid domains.
Conventional bodies with neither attribute retain shared defaults. The helper
never edits the stage or modifies the generic reader.

three.js r168's [MMD adapter](https://github.com/mrdoob/three.js/blob/r168/examples/jsm/animation/MMDPhysics.js#L912)
passes both source coefficients to Bullet's `setDamping()`.
[Bullet 3.25](https://github.com/bulletphysics/bullet3/blob/3.25/src/BulletDynamics/Dynamics/btRigidBody.cpp#L123-L144)
uses `(1-d)^dt` by default. The helper converts `0 <= d < 1` to
`rate=-log1p(-d)` instead of copying the fraction into a rate. The valid source
endpoint `d=1` requires instantaneous stopping, which finite rates cannot
represent, and rejects explicitly as unsupported. Values outside `[0,1]`
reject without clamping. This source reference is not native MMD trajectory
evidence.

[Jolt 5.5.0](https://github.com/jrouwe/JoltPhysics/blob/v5.5.0/Jolt/Physics/Body/MotionProperties.inl#L114-L129)
uses `max(0,1-rate*dt)` per substep after force integration. Source-solver
ordering and finite-step equivalence remain open.

## Results

Windows x86_64, MSVC 19.51, OpenUSD 26.08, Python 3.13.14, deterministic
Jolt 5.5.0. Rebuilt installed core/backend/bridge packages are consumed in both
probe modes.

| Check | Result |
| --- | --- |
| Shared USD/six-DOF CTests including clean-prefix consumer | All 20 pass |
| Shared no-Jolt build/CTests | All 11 pass |
| MMD execution and reader-only CTests | Three pass in each mode: filters, contact material, damping |
| Damping metadata controls | Independent conversion, zero, near-one, unsupported unit endpoint, invalid types/domains, samples, missing/blocked/partial values, neutral defaults, no stage edits |
| Damping response controls | Four independent linear/angular coefficient pairs at each of 60/240 Hz |
| Existing generic reader controls | All twelve accepted in both modes |
| Physics-bearing PMX fixtures | All 22 accepted in both modes; three damping records/bodies, two standard joints; execution creates both constraints and steps 120 times with finite state |
| Physics-free fixtures | Six empty snapshots; execution steps each 120 times |

Response controls translate a body at 10 m/s and measure a sphere's free spin
after contact seeds rotation and the floor is removed. Independent source
pairs `(0,0)`, `(0.5,0)`, `(0,0.75)` and `(0.5,0.75)` prove both channels reach
body creation without affecting the other channel.

| Step rate | Source linear 0.5: one-second speed ratio | Source angular 0.75: one-second spin ratio |
| --- | ---: | ---: |
| 60 Hz | 0.497987 | 0.245966 |
| 240 Hz | 0.499496 | 0.248997 |

Expected continuous ratios are `0.5` and `0.25`; disabled channels retain
unity within `0.000002`. These controls establish the bounded approximation,
not complete PMX simulation fidelity.

## Reproduction

Build/install the shared packages with the deterministic SDK and experimental
six-DOF enabled, as in the [execution report](2026-10-18-phase8-six-dof-execution.md#reproduction).
Set `PHYSICS_PREFIX` to the new damping prefix and `USD_ROOT`/`JOLT_ROOT` to
the same OpenUSD/SDK roots. Older prefixes lack the new descriptor field.

```powershell
cmake -S tests/physics_consumer -B build/physics-consumer-damping `
  -G 'Visual Studio 18 2026' -A x64 `
  "-DCMAKE_PREFIX_PATH=$env:PHYSICS_PREFIX;$env:USD_ROOT" `
  "-DJolt_DIR=$env:JOLT_ROOT/lib/cmake/Jolt" -DMMD_PHYSICS_EXECUTION=ON
cmake --build build/physics-consumer-damping --config Release
$env:PATH = "$env:USD_ROOT/bin;$env:USD_ROOT/lib;$env:PATH"
ctest --test-dir build/physics-consumer-damping -C Release --output-on-failure

cmake -S tests/physics_consumer -B build/physics-consumer-damping-reader `
  -G 'Visual Studio 18 2026' -A x64 `
  "-DCMAKE_PREFIX_PATH=$env:PHYSICS_PREFIX;$env:USD_ROOT" -DMMD_PHYSICS_EXECUTION=OFF
cmake --build build/physics-consumer-damping-reader --config Release
ctest --test-dir build/physics-consumer-damping-reader -C Release --output-on-failure
```

Using `$stagePaths` and the plugin environment from the
[filter report](2026-10-18-phase8-collision-filter.md#reproduction):

```powershell
& build/physics-consumer-damping/Release/mmd_physics_consumer_probe.exe @stagePaths `
  2> build/physics-consumer-damping/diagnostics.log |
  Tee-Object -FilePath build/physics-consumer-damping/results.tsv
& build/physics-consumer-damping-reader/Release/mmd_physics_consumer_probe.exe @stagePaths `
  2> build/physics-consumer-damping-reader/diagnostics.log |
  Tee-Object -FilePath build/physics-consumer-damping-reader/results.tsv
```

All commands completed locally. No importer stage, normal dependency, fixture
bytes or public runtime-adapter API changes.

## Remaining boundary

Instantaneous source damping (`d=1`), large-rate/step and force-order fidelity,
friction/restitution mixing, springs, one format-only joint, PMX angular and
zero-mass equivalence, bone synchronization and mixed-format categories remain
open. Hosted damping/MMD acceptance and compatible published packages also
remain.
