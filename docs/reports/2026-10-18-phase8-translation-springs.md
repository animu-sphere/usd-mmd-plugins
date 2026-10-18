# Phase 8 experimental translation spring execution (2026-10-18)

Local Windows evidence after the [spring audit](2026-10-18-phase8-joint-spring-audit.md).

## Boundary and source inference

Locally rebuilt installed shared packages append three neutral translation
springs to the six-DOF descriptor: stiffness N/m, viscous damping N*s/m and
equilibrium meters, in the first joint frame. Jolt uses position motors with
stiffness/damping settings. Shared code learns no MMD names or conventions;
the generic USD reader/converter still ignores source spring metadata.

The test-only consumer adds `MMD_PHYSICS_TRANSLATION_SPRINGS=ON`, default OFF
and requiring `MMD_PHYSICS_EXECUTION=ON`. A constant source type `spring6Dof`
activates positive translation coefficients only on free/limited standard
joint axes. The source type must have a constant token value if present.
Other/missing types, negative coefficients, locked axes, rotational coefficients
and format-only joints remain unexecuted source-fidelity gaps.

three.js r168's [MMD adapter](https://github.com/mrdoob/three.js/blob/r168/examples/jsm/animation/MMDPhysics.js#L1098-L1139)
passes translation coefficients directly to Bullet's spring constraint.
[Bullet 3.25](https://github.com/bulletphysics/bullet3/blob/3.25/src/BulletDynamics/ConstraintSolver/btGeneric6DofSpringConstraint.cpp#L97-L115)
uses displacement times stiffness in an iterative motor, with dimensionless
spring damping. Its motor response is not equivalent to a continuous viscous
damper. The consumer chooses a Hookean approximation, zero viscous damping,
and zero equilibrium; it does not reinterpret Bullet's default damping as
N*s/m. Existing independent body damping still applies.

Dimensional inference: with source mass/time unchanged and length multiplied
by `s`, acceleration and force both scale by `s`. Thus `s*k*x = k*(s*x)` and
translation stiffness keeps its numeric value (no `0.08` multiplier). This
derivation and the oscillator controls support an SI Hookean interpretation,
not MMD or Bullet trajectory equivalence. No angular conversion is claimed.

The CLI reports `translationSpringModel=experimentalHookean` and
`approximateTranslationSpringAxes`, with a per-prim approximation diagnostic.
The original `unsupportedSpringJoints`/`unsupportedSpringAxes` counts retain
every nonzero coefficient whose source response remains unsupported, including
approximated axes. `jointFidelity` remains `approximate` for those fixtures.

## Results

Windows x86_64, MSVC 19.51, OpenUSD 26.08, deterministic Jolt 5.5.0.

| Check | Result |
| --- | --- |
| Shared USD/six-DOF CTests | All 22 pass, including installed spring consumer |
| Shared no-Jolt CTests | All 11 pass |
| MMD execution CTests | All seven pass |
| MMD reader-only CTests | All five pass |
| Execution with translation option OFF | All five CTests pass; CLI retains its previous fields without an experimental spring model |
| Generic controls | Twelve accepted in both modes |
| Physics-bearing generated PMX fixtures | All 22 accepted, three bodies/two constraints each; 120 finite steps with two approximate translation axes per fixture (44 total) |
| Physics-free fixtures | Six empty snapshots; 120 empty steps in execution mode |
| Source spring audit | Still 23 prims/115 components reported as source-fidelity gaps, including the format-only prim |

The MMD response test builds standard USD bodies/joints, reads them through
installed `physicsUsd`, converts descriptors, applies source metadata and
steps installed Jolt. It covers all three translation axes and source
coefficients 0/25/100. Initial displacement is 0.08 m (one source length unit),
with mass 1 kg and gravity cancelled. Every sample over 0.5 seconds is compared
with the continuous oscillator solution. The maximum error divided by initial
displacement is 0.231214 at 60 Hz and 0.0655821 at 240 Hz, within bounds 0.24
and 0.07. The improvement with smaller steps measures implicit numerical
damping; zero configured viscous damping does not imply conserved energy.

Shared response controls additionally cover mass 1/4 kg, viscous damping,
nonzero equilibrium, rotated joint frames, world/static/two-dynamic endpoints,
reduced mass, center-of-mass conservation and springs loading hard limits.
Metadata controls cover type activation, signed/locked axes and no stage edits;
CLI regressions distinguish enabled springs from a `sixDof` source joint.

## Reproduction

Build/install shared core/backend/bridge with the existing deterministic SDK,
OpenUSD and experimental six-DOF configuration. Set `PHYSICS_PREFIX` to the
new installation (the local run used `build/phase8-translation-spring-prefix`
in the shared repository), and `USD_ROOT`/`JOLT_ROOT` as in the
[execution report](2026-10-18-phase8-six-dof-execution.md#reproduction).
Old static libraries are layout-incompatible; rebuild packages and consumers
together.

```powershell
cmake -S tests/physics_consumer -B build/physics-consumer-translation-spring `
  -G 'Visual Studio 18 2026' -A x64 `
  "-DCMAKE_PREFIX_PATH=$env:PHYSICS_PREFIX;$env:USD_ROOT" `
  "-DJolt_DIR=$env:JOLT_ROOT/lib/cmake/Jolt" `
  -DMMD_PHYSICS_EXECUTION=ON -DMMD_PHYSICS_TRANSLATION_SPRINGS=ON
cmake --build build/physics-consumer-translation-spring --config Release
$env:PATH = "$env:USD_ROOT/bin;$env:USD_ROOT/lib;$env:PATH"
ctest --test-dir build/physics-consumer-translation-spring -C Release --output-on-failure

cmake -S tests/physics_consumer -B build/physics-consumer-translation-spring-reader `
  -G 'Visual Studio 18 2026' -A x64 `
  "-DCMAKE_PREFIX_PATH=$env:PHYSICS_PREFIX;$env:USD_ROOT" -DMMD_PHYSICS_EXECUTION=OFF
cmake --build build/physics-consumer-translation-spring-reader --config Release
ctest --test-dir build/physics-consumer-translation-spring-reader -C Release --output-on-failure
```

With the fixture paths/plugin environment from the
[filter report](2026-10-18-phase8-collision-filter.md#reproduction):

```powershell
& build/physics-consumer-translation-spring/Release/mmd_physics_consumer_probe.exe @stagePaths `
  2> build/physics-consumer-translation-spring/diagnostics.log |
  Set-Content build/physics-consumer-translation-spring/results.tsv
& build/physics-consumer-translation-spring-reader/Release/mmd_physics_consumer_probe.exe @stagePaths `
  2> build/physics-consumer-translation-spring-reader/diagnostics.log |
  Set-Content build/physics-consumer-translation-spring-reader/results.tsv
```

Both corpus runs exit 0. No importer, fixture bytes, normal dependency,
published pin or coupling-adapter API changes. Angular springs, signed and
locked-axis source behavior, Bullet/MMD spring damping and trajectories,
instantaneous body damping, zero mass, angular-limit equivalence, hosted spring
acceptance and runtime bone synchronization remain open.
