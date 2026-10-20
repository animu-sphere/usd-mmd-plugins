# Phase 8 Stage Runner clock composition (2026-10-21)

## Scope and ownership

Extends the [Toon consumer evidence](2026-10-21-phase8-toon-consumer.md) with
an opt-in headless clock host in `usd-avatar-runtime`. The installed Stage
Runner `FrameClock` measures display elapsed time; avatar-runtime alone
accumulates time, chooses fixed ticks and owns the shared-world barrier.
MMD continuation/coupling, Jolt stepping and Toon snapshot consumption stay
with their existing owners. No importer or production API/ABI changes are
required. The runtime's
[capability matrix](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/reference/CAPABILITY_MATRIX.md#scoped-stage-runner-clock-evidence-2026-10-21)
owns the detailed assertions and results.

MMD base `0fafa5e`, runtime base `3d772a5` plus
[runtime PR #30](https://github.com/animu-sphere/usd-avatar-runtime/pull/30)
(commit `a695dd8`), Stage Runner `a81bb28`.
Windows x64, MSVC 19.51, Release, OpenUSD 26.08. MMD, physics and
Toon installations are those of the preceding report; `runtimeCore` is built
and installed separately from its owner. No sibling source enters the runtime
consumer build. Inputs remain generated `coupling-modes.pmx`, `sample.vmd`
and explicit renderer probe resources. These are local installed-package
results, without a published dependency or hosted CI claim.

## Results

All ten runtime tests pass, including the additional
`adapters.mmd_stage_runner`; the clock owner's three standalone tests pass.
The generated scene has two avatars and one non-avatar dynamic body in the
same solver. Display requests exercise zero/one/multiple ticks, bounded
catch-up with retained backlog, pause/resume without paused-time catch-up,
held source observations/revisions and full group reset. Final owner-pose
agreement precedes Toon transport. Three 24-tick replays reproduce poses
and the separate body's position/velocity on this build. Failure before Step
retries accepted time with zero elapsed; failure after Step requires reset
and publishes no partial snapshot. Previously retained output stays valid.

## Reproduction

Prepare installed MMD/physics packages and generated inputs as in the
[runtime consumer report](2026-10-21-phase8-runtime-consumer.md#reproduction),
and the ABI-4 Toon installation as in the
[Toon report](2026-10-21-phase8-toon-consumer.md#reproduction). Then run from
this MMD checkout, substituting the same SDK/renderer installation prefixes:

```powershell
$mmd = (Resolve-Path .).Path
$runtime = (Resolve-Path ../usd-avatar-runtime).Path
$runner = (Resolve-Path ../usd-stage-runner).Path
$physics = (Resolve-Path ../usd-physics-plugins).Path
$usd = '<OpenUSD-26.08-install>'
$toon = '<installed-Toon-ABI4-prefix>'
$clock = "$runtime/build/stage-runner-clock-install"
cmake -S "$runner/libs/runtimeCore" -B "$runtime/build/stage-runner-clock-owner" -A x64 `
  -DBUILD_TESTING=ON "-DCMAKE_INSTALL_PREFIX=$clock"
cmake --build "$runtime/build/stage-runner-clock-owner" --config Release
cmake --install "$runtime/build/stage-runner-clock-owner" --config Release
ctest --test-dir "$runtime/build/stage-runner-clock-owner" -C Release --output-on-failure
cmake -S $runtime -B "$runtime/build/mmd-stage-runner" -A x64 `
  -DAVATAR_BUILD_MMD_ADAPTER=ON -DAVATAR_MMD_JOLT_SMOKE=ON `
  -DAVATAR_MMD_TOON_SMOKE=ON -DAVATAR_MMD_STAGE_RUNNER_SMOKE=ON `
  "-DCMAKE_PREFIX_PATH=$mmd/build/runtime-mmd-prefix;$mmd/build/physics-ci-shared-prefix;$usd" `
  "-DruntimeCore_DIR=$clock/lib/cmake/runtimeCore" `
  "-DJolt_DIR=$physics/build/jolt-six-dof-sdk-install/lib/cmake/Jolt" `
  "-DToon_DIR=$toon/lib/cmake/Toon" `
  "-DTOON_AVATAR_RUNTIME_INCLUDE_DIR=$runtime/include" `
  "-DAVATAR_MMD_PMX=$mmd/plugins/usdMmdFileFormat/tests/fixtures/coupling-modes.pmx" `
  "-DAVATAR_MMD_VMD=$mmd/build/vmd-fixtures/sample.vmd"
cmake --build "$runtime/build/mmd-stage-runner" --config Release
$env:PATH = "$usd/bin;$usd/lib;$mmd/build/runtime-mmd-prefix/lib;" + $env:PATH
$env:PXR_PLUGINPATH_NAME = "$mmd/build/runtime-mmd-prefix/plugin/resources/mmdSchema;$mmd/build/runtime-mmd-prefix/plugin/resources/usdMmdFileFormat"
ctest --test-dir "$runtime/build/mmd-stage-runner" -C Release --output-on-failure
```

## Evidence boundary

This is a generated test host using Stage Runner's installed clock. It does
not connect `StageSession`, `stage_runner` or usdview application lifecycle,
and supplies no reusable production world binding. Rendering still uses
explicit triangle resources, without GPU rendering, resident PMX mesh/material
binding or Hydra/direct agreement. The non-avatar body has contact disabled;
contacts, joints and real-model L3 acceptance are outside this check.
P1-C and P1-2 remain open in the [current roadmap](../roadmap/current.md).
