# Phase 8 live MMD spring opt-ins (2026-10-21)

## Scope and ownership

Extends the [live body-properties path](2026-10-21-phase8-live-body-properties.md)
with the existing experimental approximations from
[PHYSICS sections 7.3–7.5](../design/PHYSICS_INTEGRATION.md#73-joint-spring-metadata-audit).
The installed optional `mmdPhysicsAdapter::ReadJointSprings` validates uniform,
unsampled source declarations and returns owned signed SI coefficients and
source-type activation. Translation retains its source number; rotation scales
by `0.08^2`. It introduces no physics/backend/runtime dependency. The importer
and generic physics reader remain unchanged.

Runtime's generic scene resource adapter adds an optional neutral joint policy,
prepared and validated once before any backend resources are created. The live
application uses it to map owner results to positive free/limited standard
joint axes, with zero equilibrium and zero viscous damping. Translation and
rotation are independent default-OFF switches. Applied axes are explicitly
reported as approximate; disabled, negative, locked and format-only meanings
remain visible as unapplied attributes. This is generated live-host evidence,
without native MMD solver equivalence or representative hair/skirt acceptance.

MMD base `45b3827`, runtime base `f757535`, plus local changes in each owner.
Windows x64, MSVC 19.51 Release, Python 3.13.14 and Python/Qt-enabled OpenUSD
26.08; installed physics/Jolt/clock/motion inputs match the preceding live-host
configuration. MMD is built and installed before runtime compilation; runtime
compiles no sibling owner source. No publication or hosted CI claim.

## Validation

The standalone owner package passes four CTests: coupling, body properties,
joint springs and dependency boundaries. Spring coverage includes optional
pairs/type, source-type activation, signed coefficients, dimensional conversion,
partial/missing/blocked/wrong-type/sampled/varying declarations, nonfinite
components, owned storage and stage immutability. Invalid input retains
`MMD_PHYSICS_METADATA_INVALID` and the joint path.

The composed MMD configuration passes eight selected coupling/property/spring,
boundary, generated-fixture and clean-prefix installed-consumer CTests. The
installed `physics_probe` reads the generated pendulum's spring declaration
through installed headers and symbols, alongside its existing three-mode
coupling/continuation check.

The runtime live-host configuration passes all sixteen CTests, including its
installed consumers. Its native spring check edits only anonymous session opinions over the
same redistributable pendulum. At 120 Hz for 0.5 seconds:

- A free Y slide ends at 0.353313 m without springs and 1.523628 m with
  source translation stiffness 100. Rotation is locked to isolate translation.
- The pendulum's quaternion Z ends at -0.656457 without springs and -0.0156242
  with source rotation stiffness 10000 (SI stiffness 64).
- Final solver rotation agrees with live UsdSkel animation, pause retains state,
  reset reproduces identical body-state sequences and close restores preceding
  session-layer text. Root-layer text remains unchanged.
- Independent flags activate only positive free/limited axes of enabled standard
  `spring6Dof` joints. Missing/other types, disabled joints and signed/locked
  coefficients do not acquire execution; remaining gaps stay inspectable.
- Malformed springs refuse construction and Reset even with both opt-ins OFF.
  Original owner code/path and displayed output survive refusal. Repair and
  Reset reread changed stiffness, matching a fresh session's body sequence.

The generic joint-policy source and installed tests additionally check one
callback per enabled joint, disabled-joint omission, actual endpoint remapping,
descriptor validation and callback failure before backend mutation. Existing
construction rollback and ordered teardown tests remain in that suite.

The actual usdview window also passes Play/Pause/Reset/resume/Stop and stage
replacement with the rotation opt-in and session-authored stiffness. The Qt
controller reports one approximate rotation axis, and composed UsdSkel rotation
agrees with the live body state. This validates the menu/Qt integration, not
resident Toon integration or Hydra/direct renderer parity.

## Reproduction and remaining scope

The runtime [live-host guide](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/guides/live-mmd-physics.md)
owns complete dependency, environment, build and launch commands. Rebuild,
test and install `libs/mmdPhysicsAdapter` as in the preceding body-properties
report, then rebuild the runtime live-host configuration. The installed owner
must export `mmdPhysicsAdapter_JOINT_SPRINGS_VERSION=1`.

```powershell
ctest --test-dir build/windows-msvc -C Release `
  -R 'mmdPhysicsAdapter|workspace_fixtures|workspace_installed_consumer' --output-on-failure
```

In runtime, run the full live-host CTest suite including
`hosts.usdview_mmd_springs`; use `-TranslationSprings` and/or `-RotationSprings`
on its launcher for explicit UI opt-in. The actual-window test accepts
`AVATAR_MMD_ROTATION_SPRINGS=1` and authors its test coefficient in memory.
No fixture bytes, licensed assets or source layers change.

Finite-angle/multi-axis and native MMD spring equivalence remain unverified.
Representative PMX/VMD intake, runtime non-bone morphs, multiple avatars,
seek/source edits, resident Toon resources and Hydra/direct agreement remain
open. P1-C and P1-2 remain open in the [roadmap](../roadmap/current.md).
