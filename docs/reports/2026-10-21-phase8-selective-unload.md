# Phase 8 selective shared-world avatar unload (2026-10-21)

## Scope and ownership

Extends the [world-lifetime evidence](2026-10-21-phase8-world-lifecycle.md) from
whole-world close to selective participant removal in `usd-avatar-runtime`.
Its [lifecycle contract](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/architecture/FRAME_LIFECYCLE.md#experimental-fixed-tick-requests)
owns scheduler membership; its
[session contract](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/architecture/STAGE_RUNNER_SESSION.md)
owns retained display output. External body/shape/command teardown remains host
policy. MMD control/coupling semantics and the physics-free importer are unchanged.

MMD base `617f4c0`, runtime base `8ee3170` plus
[runtime PR #33](https://github.com/animu-sphere/usd-avatar-runtime/pull/33)
(commit `6ff3183`). Windows x64, MSVC 19.51, Release,
OpenUSD 26.08. Installed clock, MMD, physics/Jolt and ABI-4 Toon inputs match the
preceding world-lifetime report. These are local source/install checks without
publication, hosted CI or representative L3 claims. Detailed runtime assertions
belong to its
[capability matrix](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/reference/CAPABILITY_MATRIX.md#scoped-selective-unload-evidence-2026-10-21).

## Results

The session-only configuration passes all six tests, including separately
compiled C11 core and C++ session installed consumers. The MMD/Stage Runner/
Jolt/Toon configuration passes all twelve tests. The independently negotiated
membership extension leaves both existing runtime API table layouts unchanged.

The generated host unregisters one avatar from the barrier, destroys its three
Jolt bodies and shapes, removes command/result/input rows and destroys its
runtime instance before continuing. Removal invokes no solver/reset/provider
callback, preserves the same solver and surviving handles, and changes neither
surviving body position nor the non-avatar body's state. Subsequent ticks run
only the surviving prefix, one shared Step and its suffix. The final pose
matches owner feedback/ResumePhysics exactly and passes the installed Toon
transport oracle; the non-avatar body continues falling. A subsequent group
reset rebuilds only the surviving avatar's body map. Independently retained
removed-avatar CPU and Toon output remains valid after producer destruction.

## Reproduction

Prepare installed packages, the optional session and DLL/plugin environment as
in the [host-session report](2026-10-21-phase8-host-session.md#reproduction).
Commands run against the changed runtime tree:

```powershell
$runtime = (Resolve-Path ../usd-avatar-runtime).Path
cmake --build "$runtime/build/stage-runner-session-only" --config Release
ctest --test-dir "$runtime/build/stage-runner-session-only" -C Release --output-on-failure
cmake --build "$runtime/build/mmd-stage-runner" --config Release
ctest --test-dir "$runtime/build/mmd-stage-runner" -C Release --output-on-failure
```

## Evidence boundary

This is selective scheduler removal plus explicit teardown in a generated,
contact-disabled, joint-free host. Cross-avatar constraints, automatic
body/constraint discovery and teardown, provider unregister, stage/application
wiring, resident PMX resources, GPU rendering and Hydra/direct agreement are
unverified. The host must finish resource cleanup before its next advance/reset;
removal is not a solver rollback or poisoned-group recovery path. P1-C and
representative P1-2 acceptance remain open in the
[current roadmap](../roadmap/current.md).
