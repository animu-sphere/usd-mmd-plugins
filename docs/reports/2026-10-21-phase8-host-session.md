# Phase 8 reusable Stage Runner host session (2026-10-21)

## Scope and ownership

Extends the [clock composition evidence](2026-10-21-phase8-stage-runner-clock.md)
by replacing its test-private clock/playback wrapper with the optional installed
`avatarStageRunnerSession` target in `usd-avatar-runtime`. Its
[session contract](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/architecture/STAGE_RUNNER_SESSION.md)
owns API, clock transitions and snapshot lifetime. Runtime alone owns fixed
ticks and shared-world barriers; MMD coupling, physics and rendering retain
their existing owners. MMD code and its physics-free importer are unchanged.

MMD base `098f448`, runtime base `afbff13` plus
[runtime PR #31](https://github.com/animu-sphere/usd-avatar-runtime/pull/31)
(commit `e96e993`), Stage Runner `a81bb28`.
Windows x64, MSVC 19.51, Release, OpenUSD 26.08.
Installed clock, MMD, physics/Jolt and ABI-4 Toon packages and generated inputs
are the same as in the preceding clock report. No sibling source enters the
runtime consumer build. These are local source/install results, without a
published package, release or hosted CI claim. The runtime's
[capability matrix](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/reference/CAPABILITY_MATRIX.md#scoped-stage-runner-session-evidence-2026-10-21)
owns detailed assertions and measurements.

## Results

All twelve tests in the MMD/clock/Toon configuration pass. The session-only
configuration passes six tests using only installed Stage Runner `runtimeCore`;
its installed consumer builds and executes without MMD, OpenUSD, physics or
renderer dependencies. Core-only lookup imports no session/Stage Runner target.

Session controls preserve active elapsed time under repeated play, discard
paused wall time, retain catch-up backlog and compose whole-world reset with
clock reanchoring. Reset refusal/failure and post-Step failure keep the previous
displayable snapshot set. Pre-Step recovery explicitly retries with zero new
elapsed, even after a ten-second wait. Independently retained output survives
session and runtime destruction. No new core C ABI or fixed-step loop is added.

The generated two-avatar plus non-avatar Jolt scene now uses this reusable
session for playback and requests. Existing per-tick all-pre/one-Step/all-post,
owner final-pose, held-input, three 24-tick reset/replay and installed Toon
matrix/morph/visibility checks pass. The session retains its last successful
display output when a suffix fails and delegates invalidation to runtime.

## Reproduction

Prepare the installed packages and `build/mmd-stage-runner` configuration from
the [clock report](2026-10-21-phase8-stage-runner-clock.md#reproduction), enabling
`AVATAR_BUILD_STAGE_RUNNER_SESSION=ON` in its runtime configure command as well.
With the same DLL/plugin environment set, the commands run for this change are:

```powershell
$runtime = (Resolve-Path ../usd-avatar-runtime).Path
cmake -S $runtime -B "$runtime/build/mmd-stage-runner" -DAVATAR_BUILD_STAGE_RUNNER_SESSION=ON
cmake --build "$runtime/build/mmd-stage-runner" --config Release
ctest --test-dir "$runtime/build/mmd-stage-runner" -C Release --output-on-failure
cmake -S $runtime -B "$runtime/build/stage-runner-session-only" -A x64 `
  -DAVATAR_BUILD_STAGE_RUNNER_SESSION=ON `
  "-DruntimeCore_DIR=$runtime/build/stage-runner-clock-install/lib/cmake/runtimeCore"
cmake --build "$runtime/build/stage-runner-session-only" --config Release
ctest --test-dir "$runtime/build/stage-runner-session-only" -C Release --output-on-failure
```

## Evidence boundary

The reusable session borrows an explicitly created runtime world. Automatic
Stage/world construction, `StageSession`, usdview/standalone application wiring,
stop/single-step UI policy, unload and resident PMX mesh/material matching remain
open. Renderer resources remain generated triangle probes without GPU rendering
or Hydra/direct agreement. Contacts/joints and licensed representative L3 scenes
are outside this check. P1-C and P1-2 remain open in the
[current roadmap](../roadmap/current.md).
