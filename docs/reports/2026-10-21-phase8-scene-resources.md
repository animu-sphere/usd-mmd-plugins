# Phase 8 host-owned stage physics resources (2026-10-21)

## Scope and ownership

Extends the [selective-unload evidence](2026-10-21-phase8-selective-unload.md)
with the optional `avatarPhysicsUsdResources` target in `usd-avatar-runtime`.
Its [resource contract](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/architecture/PHYSICS_USD_RESOURCES.md)
owns host construction, path/handle maps, rollback and ordered teardown.
`physicsUsd` still reads declarations and converts joints without retaining a
world. The backend still owns execution. MMD coupling and the physics-free
importer are unchanged.

MMD base `7a8681a`, runtime base `9cb59d9` plus
[runtime PR #34](https://github.com/animu-sphere/usd-avatar-runtime/pull/34)
(commit `f05cc65`). Windows x64, MSVC 19.51, Release, OpenUSD 26.08; installed physics/Jolt,
MMD, clock and ABI-4 Toon inputs match the preceding selective-unload report.
No sibling source enters the consumer build. These are local source/install
results, without publication, hosted CI or representative L3 claims. The
runtime [capability matrix](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/reference/CAPABILITY_MATRIX.md#scoped-physics-scene-resource-evidence-2026-10-21)
owns detailed assertions and counts.

## Results

The resource-only and physics-free session-only configurations each pass six
tests. The composed MMD/Stage Runner/Jolt/Toon configuration passes fourteen.
A separate installed resource consumer imports no MMD, backend, host clock or
renderer. Core/session-only consumers import no physics resource target.

Generated resource tests verify endpoint mapping, all backend creation failure
positions, rollback leaving another set intact, teardown refusal/retry and
constraint -> body -> shape destruction. The composed host replaces manual
body loops with that reusable resource set and retains its previous owner-pose,
held-input, reset/replay, selective-unload and Toon transport checks.

An additional unload case authors a bounded standard world joint in the
generated stage's in-memory session layer. The installed reader discovers it
on construction and reset. After one avatar's membership and resources are
removed, the survivor advances another 120 fixed ticks and remains within the
declared joint bound/tolerance while the non-avatar body continues. Final
owner feedback/continuation and retained CPU/Toon output checks pass. Closing
all resource sets precedes solver replacement/destruction.

## Reproduction

Prepare installed packages and the DLL/plugin environment as in the
[host-session report](2026-10-21-phase8-host-session.md#reproduction).
Enable the new optional component in the existing composed configuration:

```powershell
$runtime = (Resolve-Path ../usd-avatar-runtime).Path
cmake -S $runtime -B "$runtime/build/mmd-stage-runner" `
  -DAVATAR_BUILD_STAGE_RUNNER_SESSION=ON -DAVATAR_BUILD_PHYSICS_USD_RESOURCES=ON
cmake --build "$runtime/build/mmd-stage-runner" --config Release
ctest --test-dir "$runtime/build/mmd-stage-runner" -C Release --output-on-failure
cmake -S $runtime -B "$runtime/build/physics-usd-resources-only" -A x64 `
  -DAVATAR_BUILD_PHYSICS_USD_RESOURCES=ON `
  "-DCMAKE_PREFIX_PATH=$PWD/build/physics-ci-shared-prefix;$usd"
cmake --build "$runtime/build/physics-usd-resources-only" --config Release
ctest --test-dir "$runtime/build/physics-usd-resources-only" -C Release --output-on-failure
cmake --build "$runtime/build/stage-runner-session-only" --config Release
ctest --test-dir "$runtime/build/stage-runner-session-only" -C Release --output-on-failure
```

`$usd` names the installed OpenUSD prefix prepared by the preceding recipe.

## Evidence boundary

Resource sets consume complete, explicit owner snapshots in a caller-created
world. The generated body policy retains contact-disabled settings and the
deliberate initial feedback rotation. The added world joint is a generated
lifecycle fixture, not a PMX joint/spring fidelity claim. Automatic application
discovery, world factory/settings selection, avatar configuration, cross-avatar
constraints and their teardown policy remain open. Renderer resources are still
explicit probes, without resident PMX rendering, GPU/Hydra agreement or licensed
representative L3 acceptance. P1-C and P1-2 remain open in the
[current roadmap](../roadmap/current.md).
