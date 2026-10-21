# Phase 8 first live usdview physics path (2026-10-21)

## Scope and ownership

The optional runtime-owned `usdviewMmdPhysics` application composes installed
MMD control/coupling, runtime fixed ticks, Stage Runner's clock/session, whole-stage
physics resources and Jolt. A pose-only runtime USD consumer copies final joints
into an anonymous session sublayer, consumed by usdview's default Hydra path.
The importer remains physics-free. The runtime
[guide](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/guides/live-mmd-physics.md)
owns build/run steps; its
[output contract](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/architecture/USD_POSE_OUTPUT.md)
owns publication and lifetime.

MMD base `094927c` plus this generated-fixture change; runtime commit `af5e807`
in [runtime PR #36](https://github.com/animu-sphere/usd-avatar-runtime/pull/36)
(base `901fc3f`). Windows x64, MSVC 19.51, Release, Python 3.13.14 and Python/Qt-enabled
OpenUSD 26.08. Installed MMD/physics/Jolt/clock inputs are the existing
[stage-world composition](2026-10-21-phase8-stage-world-binding.md) inputs;
the USD binder uses installed motion owner 0.5.4 packages. No sibling source is
compiled into the runtime host. These are local build/install/application checks,
without publication or hosted CI claims.

## Generated fixture and results

`live-physics-pendulum.pmx`, authored by the committed fixture generator,
contains four BDEF1-skinned vertices, two bones, a kinematic anchor, a
dynamicWithBone box and one standard constrained pivot. The box's offset centre
of mass produces visible rotation under gravity; feedback rotates the bar's
driving bone. The fixture contains no third-party model or motion.

The runtime source/install suite passes 14 tests in the live-host configuration.
The native host runs three identical 120-tick replays, checks gravity-driven
rotation, live UsdSkel binding, pause/reset and cleanup. Publication has no time
samples, and no physics bake is produced. Source layers remain byte-equivalent
after Stop. Generic output tests cover placement/units, multiple roots, scale,
malformed/stale values, previous animation restoration and retained CPU values.
The separate installed USD consumer also executes the output contract.

A real usdview window additionally loads the menu and exercises Qt-timer
Play/Pause/Reset/resume/Stop. The composed UsdSkel query observes the final
physics-driven rotation. This establishes the first bounded live application
path; it is not a claim of representative avatar or renderer parity acceptance.
The fixture generator check and all 46 importer stage-opening fixtures pass.

## Reproduction and boundary

Use the runtime guide with `AVATAR_MMD_PREVIEW_PMX` set to
`plugins/usdMmdFileFormat/tests/fixtures/live-physics-pendulum.pmx` from this
repository. The native contract is normal CTest; `test_usdview.py` is a separate
opt-in actual-window check. Use **MMD Physics > Play**, not usdview's timeline
button, to advance live physics. A VMD is optional; this fixture validates
gravity-driven feedback without a motion file.

The first host intentionally disables contacts and body damping and reports
the preserved filter/contact/damping/spring attributes it does not apply.
It publishes joints only. Representative hair/skirt behavior, runtime morphs,
full PMX/VMD intake, automatic multi-avatar binding, source edits/seek,
resident hydra-toon resources, Hydra/direct agreement and licensed L3 scenes
remain open. P1-C and P1-2 remain open in the [current roadmap](../roadmap/current.md).
