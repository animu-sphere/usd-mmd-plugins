# Phase 8 mode-1 bone feedback correction (2026-10-25)

## Cause and scope

MMD base `4403f1f`; runtime base `20e2e42`, plus the changes described here.
Windows x64, MSVC 19.51 Release, Python 3.13.14 and OpenUSD 26.08 with usdview;
the existing installed motion 0.5.4, physics/Jolt and Stage Runner packages.
This follows a local character preview in which chain roots moved but most
descendants did not receive their simulated transforms.

The owner contract and implementation incorrectly treated PMX mode 1
(`dynamic`) as a simulated body that never writes its associated bone.
Only mode 2 (`dynamicWithBone`) produced rotation feedback. The observed model
uses mode 2 at several hair/cloth chain roots and mode 1 on their descendants.
The source mode/association preservation was correct; the missing runtime
coupling suppressed visible deformation even while those bodies simulated.

The corrected [physics contract section 5](../design/PHYSICS_INTEGRATION.md#5-rigid-body-modes)
matches the reference consumer's
[`updateBone` distinction](https://github.com/mrdoob/three.js/blob/r168/examples/jsm/animation/MMDPhysics.js#L815-L839):
mode 1 feeds position and rotation, while mode 2 retains pre-step local bone
translation and feeds rotation. This replaces the no-feedback interpretation
in the [earlier coupling report](2026-10-21-phase8-coupling-adapter.md).

The MMD adapter removes the existing bone/body rest offset and uses updated
parent transforms before converting each child. Missing associated mode-1
snapshots now refuse, as mode 2 already did. Multiple associated simulated
bodies driving one bone refuse across both modes. Unassociated bodies still
emit the existing diagnostic and produce no feedback. No shared solver,
runtime ABI, authored-stage token, importer behavior or dependency pin changes.
Rebuild and reinstall the owner static library and rebuild consumers together.

## Generated checks

The owner unit suite adds a mode-2 parent/mode-1 child with nonzero rest offset,
noncommuting rotations, rotated/translated placement, reversed body order and
both metre and centimetre stage units. It checks exact expected child-local
translation/rotation, retained mode-2 translation, immutable pose/snapshots,
repeat/reset equality and missing mode-1 snapshot refusal. A separate mode-1
root verifies position/rotation and placement/offset removal. Cross-mode
duplicate-writer refusal and non-driven intermediate coverage remain explicit.

The installed coupling consumer now expects two feedback rows and verifies
mode-1 root position `(3,4,5)`, root rotation 170 degrees, parent-relative
mode-2 rotation -110 degrees and the resulting post append -55 degrees.
The generated fixture bytes are unchanged. Control unit, all four physics
adapter checks, fixture smoke and clean installed-consumer checks pass.

Runtime source/installed checks update the former animation-owned-root
expectation and retain post append and bone-morph composition. The generated
shared-world Jolt check uses distinct initial mode-1/mode-2 rotations so the
child-local rotation and post append remain observable after root feedback.
It retains two participants, one Step per tick and three exact 24-tick replays.

The new `hosts.usdview_mmd_dynamic` uses the existing generated pendulum/anchor
inputs and changes the composed session's swing mode to `dynamic`, with
the joint's Y translation limit set to +/-0.2 m to expose position output
independently of solver constraint residuals.
With both spring options OFF, three 240-tick replays compare final UsdSkel
position/rotation against the concrete body after rest-offset removal, check
pause and restore the original persistent/session layers. Maximum position
error is `2.01303e-7` m against a declared `2e-5` m bound; local translation
reaches `0.203899` m (required above `0.01` m), proving mode-1 position output
is used. All 19 tests in the live-host runtime configuration pass. A separate
installed Jolt/Toon/Stage Runner configuration passes all 14 tests, including
renderer transport, scene-resource and host-session regression checks.

## Anonymous local preview

The same local character and bone/IK-only motion preview are replayed for
600 ticks at 1/120 s, three times. A separate derived local VMD omits morph
tracks because complete non-bone execution remains unsupported; original model
and motion bytes are untouched. Names, source paths, terms, raw diagnostics,
assets and screenshots remain local and are not committed.

All 123 associated mode-1 bones change their local rotation. For every tick,
each published mode-1 bone world transform agrees with the concrete solver
body after independent rest-offset removal. Maximum position error is
`4.82841284e-7` m (bound `2e-5` m); maximum quaternion-dot error is
`3.10862447e-14` (bound `1e-5`). All exposed body values remain finite.
Body state and published joint arrays repeat exactly after Reset, and Close
restores the original source/session layers. Both spring options are OFF.
The updated actual usdview window also displays the same preview through the
Qt timer and composed live animation; this is not a quantitative pixel test.

## Reproduction and limits

Use the [MMD build guide](../guides/building.md) for installed dependencies:

```powershell
cmake --build build/windows-msvc --config Release
ctest --test-dir build/windows-msvc -C Release -R "^(mmdPhysicsAdapter_|mmdControl_unit|workspace_installed_consumer)" --output-on-failure
python scripts/check_docs.py --selftest
python scripts/check_docs.py
```

Reinstall the rebuilt MMD owner and rebuild the runtime host, using its
[live-host guide](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/guides/live-mmd-physics.md#validation).
Run `hosts.usdview_mmd_dynamic` for the generated regression. Private inputs
remain a separate local preview, not a redistributable acceptance fixture.

This closes the missing mode-1 bone-feedback defect. It does not establish
full-motion, representative hair/skirt spring/contact/recovery acceptance,
native MMD trajectory equivalence, resident Toon binding, Hydra/direct parity,
P1-C or L3. Spring tuning remains a separate increment.
