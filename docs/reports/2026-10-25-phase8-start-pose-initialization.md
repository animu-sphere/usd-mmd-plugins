# Phase 8 start-pose physics initialization (2026-10-25)

## Cause and scope

MMD base `4309a98`; runtime base `13668cf`, plus the changes described here.
Windows x64, MSVC 19.51 Release, Python 3.13.14 and OpenUSD 26.08 with usdview;
installed motion 0.5.4, physics/Jolt and Stage Runner packages from the preceding
[feedback correction](2026-10-25-phase8-dynamic-bone-feedback.md).

The live host constructed bodies at the authored rest pose. Its first tick
then moved only followBone bodies to the VMD pose through `moveKinematic`.
A large initial displacement became a large kinematic velocity while simulated
chain members still started at rest. This injected an unintended impulse into
the character, particularly visible in cloth chains.

The owner now exposes `InitialBodyTransforms`, using the same bound rest offsets
and evaluated pose conversion as the per-tick kinematic query, but including
all associated modes. The host evaluates source frame zero with `BeginPhysics`
and supplies those transforms during body construction, before constraints.
Fresh world construction gives zero linear/angular velocity on initial startup
and Reset. Unassociated bodies retain authored placement. Pause/resume does
not reseed. No hidden warm-up, Step, source edit, clock shift, solver API,
runtime ABI or importer change is introduced. See the
[coupling contract](../design/PHYSICS_INTEGRATION.md#54-coupling-lifecycle)
and [package capability](../architecture/PACKAGE_CONTRACT.md#mmdphysicsadapter).
Reinstall the owner static library and rebuild the live host together.

## Generated checks

Owner unit checks cover all three modes, unassociated omission, nonzero rest
offsets, root/child rotations, translated/rotated placement, reversed body
order, metre/centimetre stages, immutable inputs and stage-independent lifetime.
Repeated/reset queries match exactly; malformed/nonfinite poses refuse.
The installed consumer links the new query and verifies all three rest seeds.
All six physics-adapter CTests, VMD fixture inspection and the clean installed
consumer pass.

Three generated VMDs hold non-rest frame-zero poses: translation `(1,.3,-.2)` m,
that translation with a 90-degree root Y rotation, and those changes with a
30-degree child Z rotation. `hosts.usdview_mmd_startup` checks initial world
positions/rotations before any Step and exactly zero exposed linear velocities.
Play with zero elapsed time leaves the initial bodies and step count unchanged.
Each input runs 240 ticks at 1/120 s, three times, with both spring options OFF.
Pause retains state, Reset reproduces every body and published pose exactly,
and Close restores persistent/session layer texts. All values remain finite.

Maximum generated speed is `3.56205` m/s (bound 5 m/s). The translation-only
trajectory agrees with an independently run rest-input trajectory translated
by the same offset to `4.52731e-7` m (bound `2e-5` m). The complete live-host
runtime configuration passes all 20 CTests, including the existing dynamic,
morph, property, spring and installed-consumer checks.

## Before/after local measurements

Each row measures the maximum dynamic-body linear speed on the first 1/120 s
tick. The same diagnostic is run before and after rebuilding the native host.
The generated diagnostic uses a held 1 m translation along X; it is separate
from the three committed regression inputs above.

| Input | Before (m/s) | After (m/s) |
| --- | ---: | ---: |
| Generated rest input | 0.0610412 | 0.0610412 |
| Generated held 1 m start offset | 120.000008 | 0.0610412 |
| Anonymous model A, rest input | 0.0764843 | 0.0764843 |
| Anonymous model A, bone/IK motion preview | 102.025191 | 0.0776846 |

For the private motion, the largest first-tick followBone target displacement
falls from `1.21740645` m to `2.693609e-6` m. Its maximum dynamic speed over
the first 120 ticks falls from `102.025191` to `1.59633690` m/s. For comparison,
the same model at rest reaches `1.62160390` m/s over those ticks.

The private bone/IK preview also passes three exact 600-tick reset replays.
All 123 associated mode-1 bones change local rotation and agree with concrete
solver transforms after independent rest-offset removal. Maximum position
error is `2.03957331e-7` m and quaternion-dot error `6.21724894e-15`, against
bounds `2e-5` m and `1e-5`. All exposed body values stay finite and original
source/session layers are restored. Both spring options remain OFF.
The rebuilt host is also reopened in actual usdview; its Qt timer advances
the composed character pose and concrete world. This visual smoke is not a
pixel comparison or representative skirt-recovery measurement.

The private preview uses a separate derived VMD with morph tracks omitted;
original model/motion bytes remain unchanged. It is not full-motion acceptance.
Private names, paths, terms, raw diagnostics, assets and screenshots remain local
and are excluded from this change.

## Reproduction and remaining gates

Use the [build guide](../guides/building.md) for the installed owner setup:

```powershell
cmake --build build/windows-msvc --config Release
ctest --test-dir build/windows-msvc -C Release -R '^(mmdPhysicsAdapter_.*|vmd_inspect_fixtures|workspace_installed_consumer)$' --output-on-failure
python scripts/check_docs.py --selftest
python scripts/check_docs.py
```

Regenerate the VMD fixture directory, reinstall MMD and rebuild the runtime
host as described in its
[validation guide](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/guides/live-mmd-physics.md#validation).
Run `hosts.usdview_mmd_startup` for the generated regression and the complete
live-host configuration for integration coverage. Private diagnostic inputs
remain local and are not required by these tests.

This fixes the startup teleport impulse. Genuine initial collision overlap,
constraint residuals and contact recovery can still require separate treatment.
It does not establish representative skirt/hair spring behavior, native MMD
trajectory equivalence, arbitrary seeking, full morph execution, resident Toon
binding, Hydra/direct parity, P1-C or L3 acceptance.
