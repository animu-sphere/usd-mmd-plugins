# Phase 8 experimental rotation spring execution (2026-10-18)

Local Windows follow-up to the [translation spring slice](2026-10-18-phase8-translation-springs.md).

## Boundary and inference

The isolated consumer adds `MMD_PHYSICS_ROTATION_SPRINGS=ON`, default OFF,
requiring execution mode but independent of the translation option. It fills
the installed neutral descriptor's `rotationSprings` for positive coefficients
on free/limited angular axes of standard `spring6Dof` joints. Missing/other
types, negative/locked coefficients and format-only joints stay unexecuted.
Malformed type metadata rejects through the activation helper shared with
translation springs. No source audit count is reduced by approximate execution.

Dimensional inference: with length scaled by `s=0.08` and mass/time unchanged,
rotational inertia and torque scale by `s^2`; angles do not scale. The small-angle
SI stiffness is therefore `k_source * 0.0064` N*m/rad. Identity equilibrium and
zero viscous damping are selected. Reflection changes the signs of torque and
angle together, leaving diagonal stiffness unchanged. This inference assumes
source coefficients describe torque per radian; it is not native MMD evidence.

three.js r168's [MMD adapter](https://github.com/mrdoob/three.js/blob/r168/examples/jsm/animation/MMDPhysics.js#L1131-L1139)
passes rotational coefficients to Bullet without scaling in its source-length
world. [Bullet 3.25](https://github.com/bulletphysics/bullet3/blob/3.25/src/BulletDynamics/ConstraintSolver/btGeneric6DofSpringConstraint.cpp#L117-L129)
uses Euler displacement and an iterative spring motor. Its dimensionless spring
damping has no inferred viscous equivalent here. Independent body damping still
applies.

The shared backend uses small-angle orientation motors, not a Bullet constraint.
Jolt 5.5.0's [implementation](https://github.com/jrouwe/JoltPhysics/blob/v5.5.0/Jolt/Physics/Constraints/SixDOFConstraint.cpp#L421-L517)
resolves motors in the second joint frame and approximates orientation error
after projecting onto enabled axes. Finite coupled rotations and anisotropic
springs need separate evidence. Shared packages know no MMD names, units or
activation rules; the generic USD reader remains unchanged.

## Results

Windows x86_64, MSVC 19.51, OpenUSD 26.08, deterministic Jolt 5.5.0.

| Check | Result |
| --- | --- |
| Shared USD/six-DOF CTests | All 23 pass, including installed rotational spring response |
| Shared no-Jolt CTests | All 11 pass |
| Execution with both spring options enabled | All nine CTests pass |
| Execution with only rotation springs enabled | All seven CTests pass |
| Reader-only consumer CTests | All five pass |
| Execution with both spring options OFF | All five CTests pass |
| Generic probe controls | Twelve accepted |
| Physics-bearing PMX fixtures | All 22 create three bodies/two constraints and step 120 times with finite state; three rotation axes each (66 total), alongside two translation axes each |
| Physics-free fixtures | Six empty snapshots; 120 empty steps each |
| Source audit | Still 23 prims/115 coefficients reported as fidelity gaps; all 22 physics-bearing results remain `approximate` |

The source-to-SI response control reads a standard USD sphere of radius 0.16 m
(two source units), mass 1/4 kg and initial rotation 0.1 rad. For each X/Y/Z
axis and source stiffness 0/25/100, it compares every sample over 0.5 seconds
with `theta(t)=0.1*cos(sqrt(k_source/I_source)*t)`, where `I_source=1.6*mass`.
The oracle is evaluated in source units independently of the SI conversion.
Omitting the squared length conversion would change the period by 12.5.

| Rate | Maximum angular error / initial displacement | Bound |
| --- | ---: | ---: |
| 60 Hz | 0.189139 | 0.20 |
| 240 Hz | 0.0522146 | 0.06 |

The decreasing error measures timestep approximation and numerical damping;
zero configured damping does not imply energy conservation. Shared controls
also cover rotated frames, swapped world/static endpoints, two dynamic bodies
with reduced inertia and angular momentum balance, damping, anisotropic
settings, hard angular stops, unsafe scalar rejection and cleanup. Metadata
controls check activation, wrong/blocked/time-sampled type, signed/locked axes,
and that conversion does not edit the stage.

CLI output names `rotationSpringModel=experimentalSmallAngle` and
`approximateRotationSpringAxes`, with per-prim approximation diagnostics.
`unsupportedSpringJoints`, `unsupportedSpringAxes` and `jointFidelity` continue
to describe source fidelity, including approximated axes.

## Reproduction

Rebuild/install the shared core/backend/bridge into a separate prefix, using
the deterministic SDK/OpenUSD configuration from the
[execution report](2026-10-18-phase8-six-dof-execution.md#reproduction).
The local installation is `build/phase8-rotation-spring-prefix` in the shared
repository. Old static descriptor layouts are incompatible: rebuild every
consumer against the new prefix.

```powershell
cmake -S tests/physics_consumer -B build/physics-consumer-rotation-spring `
  -G 'Visual Studio 18 2026' -A x64 `
  "-DCMAKE_PREFIX_PATH=$env:PHYSICS_PREFIX;$env:USD_ROOT" `
  "-DJolt_DIR=$env:JOLT_ROOT/lib/cmake/Jolt" `
  -DMMD_PHYSICS_EXECUTION=ON -DMMD_PHYSICS_TRANSLATION_SPRINGS=ON `
  -DMMD_PHYSICS_ROTATION_SPRINGS=ON
cmake --build build/physics-consumer-rotation-spring --config Release
$env:PATH = "$env:USD_ROOT/bin;$env:USD_ROOT/lib;$env:PATH"
ctest --test-dir build/physics-consumer-rotation-spring -C Release --output-on-failure
```

With the fixture paths/plugin environment from the
[filter report](2026-10-18-phase8-collision-filter.md#reproduction):

```powershell
& build/physics-consumer-rotation-spring/Release/mmd_physics_consumer_probe.exe @stagePaths `
  2> build/physics-consumer-rotation-spring/diagnostics.log |
  Set-Content build/physics-consumer-rotation-spring/results.tsv
```

The corpus run exits 0. The importer, fixture bytes, normal dependencies and
published pins do not change. Bullet/MMD trajectories, signed/locked/source-only
spring behavior, finite-angle angular/limit fidelity, zero mass, instantaneous
body damping, hosted spring acceptance and bone synchronization remain open.
