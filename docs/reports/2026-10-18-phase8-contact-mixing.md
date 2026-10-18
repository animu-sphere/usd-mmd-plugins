# Phase 8 MMD contact pair mixing (2026-10-18)

Local Windows evidence after [body damping](2026-10-18-phase8-body-damping.md).
The isolated consumer now selects reference contact coefficient rules through
installed shared packages. This does not establish native MMD trajectories.

## Reference and boundary

[Bullet 3.25](https://github.com/bulletphysics/bullet3/blob/3.25/src/BulletCollision/CollisionDispatch/btManifoldResult.cpp#L48-L63)
multiplies body friction and clamps it to `[-10,10]`; restitution is the product
of the two body values. The existing MMD reader accepts non-negative friction,
so the probe needs only the upper friction ceiling. Jolt's defaults instead
use geometric-mean friction and maximum restitution
([Jolt 5.5.0](https://github.com/jrouwe/JoltPhysics/blob/v5.5.0/Jolt/Physics/PhysicsSystem.h#L62-L69)).
The earlier same-coefficient response controls did not resolve this gap.

The test-only `ContactMixing()` helper returns neutral product rules with a
friction ceiling of 10. The execution probe chooses them at world creation;
`physicsUsd` still reads no MMD metadata. Contact coefficients are read from
each source prim through the existing strict reader. Reader-only mode checks
the policy without linking a backend. Shared code has no MMD name table or
source profile, and keeps its existing no-argument defaults.

This policy applies to the entire isolated probe world, including conventional
control bodies. It is not a decision about mixed-format runtime contacts.
Importer output, fixture bytes, normal dependencies and the absence of a
public coupling adapter are unchanged. Packages and consumers are rebuilt
together into a separate local prefix; no public pins change.

## Results

Windows x86_64, MSVC 19.51, OpenUSD 26.08, deterministic Jolt 5.5.0.

| Check | Result |
| --- | --- |
| Shared USD/six-DOF CTests | All 21 pass, including clean-prefix consumer |
| Shared no-Jolt CTests | All 11 pass, including typed unavailable consumer |
| MMD reader/execution CTests | All three pass in each mode: filters, material/mixing, damping |
| Generic reader controls | All twelve accepted in both modes |
| Physics-bearing PMX fixtures | All 22 accepted in both modes, with three filtered/material/damped bodies and two joints; execution creates both constraints and runs 120 finite steps |
| Physics-free fixtures | Six empty snapshots and 120 empty execution steps |

Material response controls disable damping. A box starts sliding at 5 m/s and
is measured after 0.5 seconds. A sphere drops from center height 3 m; bounce
is maximum upward velocity. Each body uses independently read MMD attributes.

| Step rate | Friction product `(0.25,1)` speed | Restitution product `(0.25,0.8)` bounce | Unit restitution bounce |
| --- | ---: | ---: | ---: |
| 60 Hz | 3.77375 | 1.40610 | 7.03050 |
| 240 Hz | 3.77378 | 1.39792 | 6.98962 |

Swapped pairs match, friction `(0.5,0.5)` matches `(0.25,1)`, and zero
restitution on either endpoint absorbs. Friction `(4,4)` capped at 10 matches
`(5,2)` from a 100 m/s start: 36.9609 / 36.9241 m/s at 60/240 Hz. The
equivalent-pair comparison accounts for high-friction box rotation. Shared
backend controls additionally check a `0.25` ceiling and all three combine
rules while differently configured worlds remain alive, proving default
behavior and world isolation.

## Reproduction

Build/install shared packages with the validated SDK, experimental six-DOF
enabled and this increment. Set `PHYSICS_PREFIX` to the rebuilt mixing prefix,
and `USD_ROOT`/`JOLT_ROOT` to the same SDK roots as the
[execution report](2026-10-18-phase8-six-dof-execution.md#reproduction).
Older prefixes lack the mixing API.

```powershell
cmake -S tests/physics_consumer -B build/physics-consumer-mixing `
  -G 'Visual Studio 18 2026' -A x64 `
  "-DCMAKE_PREFIX_PATH=$env:PHYSICS_PREFIX;$env:USD_ROOT" `
  "-DJolt_DIR=$env:JOLT_ROOT/lib/cmake/Jolt" -DMMD_PHYSICS_EXECUTION=ON
cmake --build build/physics-consumer-mixing --config Release
$env:PATH = "$env:USD_ROOT/bin;$env:USD_ROOT/lib;$env:PATH"
ctest --test-dir build/physics-consumer-mixing -C Release --output-on-failure

cmake -S tests/physics_consumer -B build/physics-consumer-mixing-reader `
  -G 'Visual Studio 18 2026' -A x64 `
  "-DCMAKE_PREFIX_PATH=$env:PHYSICS_PREFIX;$env:USD_ROOT" -DMMD_PHYSICS_EXECUTION=OFF
cmake --build build/physics-consumer-mixing-reader --config Release
ctest --test-dir build/physics-consumer-mixing-reader -C Release --output-on-failure
```

Using the fixture paths and plugin environment from the
[filter report](2026-10-18-phase8-collision-filter.md#reproduction):

```powershell
& build/physics-consumer-mixing/Release/mmd_physics_consumer_probe.exe @stagePaths `
  2> build/physics-consumer-mixing/diagnostics.log |
  Tee-Object -FilePath build/physics-consumer-mixing/results.tsv
& build/physics-consumer-mixing-reader/Release/mmd_physics_consumer_probe.exe @stagePaths `
  2> build/physics-consumer-mixing-reader/diagnostics.log |
  Tee-Object -FilePath build/physics-consumer-mixing-reader/results.tsv
```

All commands completed locally. Instantaneous source damping (`d=1`), timestep
and source trajectory fidelity, springs, the format-only joint, PMX angular
and zero-mass behavior, bone synchronization and mixed-format policy/category
allocation remain open. Hosted MMD/mixing acceptance and compatible published
packages also remain.
