# Phase 8 live keyed input and private intake (2026-10-24)

## Scope and environment

This follows the [live spring evidence](2026-10-21-phase8-live-springs.md).
MMD base `c796a5c` plus a generated VMD fixture, runtime base `c990824` plus
a native acceptance test; Windows x64, MSVC 19.51 Release, Python 3.13.14,
OpenUSD 26.08 with usdview. The runtime host consumes the existing installed
MMD, motion 0.5.4, physics/Jolt and Stage Runner packages from that configuration.
No runtime/provider implementation or dependency pin changes in this increment.
Both experimental spring options are OFF. Contacts retain the owner's filter
and product-mixing policy; the generated pendulum excludes every collision pair.

The generated check closes a bounded VMD-input coverage gap. Private-asset
checks identify intake limitations. Neither closes P1-C, P1-2 or L3.

## Redistributable generated input

The existing `live-physics-pendulum.pmx` has two bones/bodies and one pivot.
`tests/fixtures/generate_vmd_fixtures.py` now writes `live-physics-anchor.vmd`:
four anchor keys at frames 0, 15, 30 and 60. The pivot moves 1.25 MMD units
(10 cm under the owning 0.08 m scale), returns and holds. The fixture has no
morph, visibility or third-party content; ordinary VMD reader/tool assertions
cover its counts and track metadata.

Runtime `hosts.usdview_mmd_motion` compares a motion-free control with three
240-tick keyed replays, at a fixed 1/120 s. Display elapsed is rounded up to
0.0083334 s to avoid rounding below a fixed tick on the Windows steady clock;
the test asserts exactly one simulation tick per display. Bounds are declared
in the test before execution: finite body/bone transforms and velocities,
body quaternion squared-norm error below `1e-4`, absolute body position below
3 m, joint pivot separation below 2 cm, keyed/publication translation error
below `1e-5`/`2e-5` m and final skinning error below `2e-5` m.

| Assertion | Observed |
| --- | --- |
| VMD drives concrete kinematic pivot and published anchor | 10 cm at 0.5 s; returned at 1 s; held through 2 s |
| Dynamic feedback differs from motion-free gravity control | Maximum joint orientation difference 0.410687 rad |
| Maximum joint pivot separation | 0.000857388 m |
| Maximum composed UsdSkel vertex error against concrete solver rotation and animated pivot | 3.50935e-7 m |
| Reset replay | Body state and published joint arrays exactly equal at every tick, all three runs |
| Pause / cleanup | No paused ticks; original persistent/session layer text restored after Close |

No animation time samples or physics bake are produced. The test consumes
the composed UsdSkel skinning query, without opening a window or checking
rendered pixels. The earlier actual-window evidence is not extended here.

## Private inputs: anonymous smoke and refusals

The user supplied a local dataset and explicitly excluded source files and
filenames from commits. Models A/B and motion C are anonymous local IDs;
their identifying paths, names, terms and raw diagnostics remain local.
No third-party bytes, screenshots, readmes or asset names enter this change.
Model terms were inspected locally; motion C has no accompanying terms in
its local folder. Acquisition provenance and permission documentation for a
licensed L3 scene set remain incomplete. There is no redistribution claim.

| Input / scenario | Result |
| --- | --- |
| Model A, rest input, 177 bodies / 319 joints | 600 ticks x 3; every exposed body/bone transform finite; body and published joint arrays exactly repeat after Reset |
| Model A + full motion C | First advance refuses with `MMD_ADAPTER_MORPH`: non-bone morph has no output binding in the pose-only host |
| Model B, rest input | Construction refuses with owner `MMD_PHYSICS_METADATA_INVALID`: damping coefficient declaration outside the accepted domain |

Model A's smoke predeclares absolute body position below 10 m, body speed
below 100 m/s and body quaternion squared-norm error below `1e-4`. Observed
maxima are 1.59459233 m, 1.62160393 m/s and `2.31419057e-7`, respectively.
Each run covers 5 s, with 462 reported unapplied spring attributes. Pause
admits no simulation ticks and Close restores source/session layer text.
These loose bounds detect gross instability; contact penetration, per-joint
limits, settling, recovery and rendered output were not asserted on this model.
It is a finite-state/reset smoke, not representative L3 acceptance.

No source values were clamped, morph tracks stripped or inputs edited to
bypass refusals. Morph binding/publication belongs to runtime integration;
damping admission remains the MMD owner's contract. A full input run must
resolve those boundaries before making an avatar acceptance claim.

## Reproduction and remaining gates

Use the runtime [live-host guide](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/guides/live-mmd-physics.md#validation)
to generate the VMD fixtures, set `AVATAR_MMD_PREVIEW_VMD` alongside the
generated `AVATAR_MMD_PREVIEW_PMX`, then run `hosts.usdview_mmd_motion`.
All 17 configured runtime source/installed/live-host tests pass, including
this new test. MMD `vmd_inspect_fixtures`, docs checks and docs selftest pass.
Private raw logs stay under the ignored runtime build directory and do not
form a redistributable acceptance corpus.

Remaining work follows [PHYSICS_INTEGRATION section 12.3](../design/PHYSICS_INTEGRATION.md#123-representative-scene-acceptance):
document usable acquisition/permission boundaries, connect required non-bone
outputs, select admitted representative declarations, declare meaningful
constraint/contact/recovery bounds and verify actual rendered final output.
Resident Toon resources and Hydra/direct agreement remain separate gates.
