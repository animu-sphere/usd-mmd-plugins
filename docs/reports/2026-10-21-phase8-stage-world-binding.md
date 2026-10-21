# Phase 8 whole-stage physics world binding (2026-10-21)

## Scope and ownership

Extends the [scene-resource evidence](2026-10-21-phase8-scene-resources.md)
with `avatarPhysics::StageWorld` in the optional runtime
`avatarPhysicsUsdResources` target. Its
[owning contract](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/architecture/PHYSICS_USD_RESOURCES.md#whole-stage-world-binding)
defines stage reading, caller-selected world factories, whole-stage resource
ownership and checked close. The installed physics reader owns validation and
scene settings; the selected backend owns execution. MMD coupling and the
physics-free importer are unchanged.

MMD base `9113124`, runtime base `7f8858b` plus
[runtime PR #35](https://github.com/animu-sphere/usd-avatar-runtime/pull/35)
(commit `908c1cd`).
Windows x64, MSVC 19.51, Release, OpenUSD 26.08. Installed physics/Jolt, MMD,
clock and ABI-4 Toon inputs match the preceding scene-resource report. No sibling
source enters the consumer build. These are local source/install results,
without publication, hosted CI or representative L3 claims. The runtime
[capability matrix](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/reference/CAPABILITY_MATRIX.md#scoped-stage-world-binding-evidence-2026-10-21)
owns detailed assertions and counts.

## Results

The physics-resource-only configuration passes six tests and the composed
MMD/Stage Runner/Jolt/Toon configuration passes fourteen. Both source and
separately built installed consumers validate factory settings, absent scene
defaults, owner refusal before factory invocation, construction rollback,
checked teardown refusal/retry and world destruction after resource cleanup.
A generated cross-root joint shares the complete stage's resource lifetime.
The installed resource consumer still imports no MMD, backend, clock or renderer.

The additional Jolt smoke copies the generated PMX stage, authors a standard
PhysicsScene into that copy and constructs all bodies through the reusable
scope. The host runs 120 fixed ticks, closes and reconstructs the complete scope,
and reproduces a selected dynamic body's final position exactly on this build.
The declared gravity reaches the backend factory. Existing two-avatar barrier,
held-input, reset/recovery, selective-unload and retained CPU/Toon checks pass.
The scope itself never steps, registers participants, runs controls or edits USD.

## Reproduction

Prepare the installed dependencies, DLL/plugin environment and two runtime build
configurations from the [scene-resource recipe](2026-10-21-phase8-scene-resources.md#reproduction).
From this MMD checkout, the commands run for this change are:

```powershell
$runtime = (Resolve-Path ../usd-avatar-runtime).Path
cmake --build "$runtime/build/physics-usd-resources-only" --config Release
ctest --test-dir "$runtime/build/physics-usd-resources-only" -C Release --output-on-failure
cmake --build "$runtime/build/mmd-stage-runner" --config Release
ctest --test-dir "$runtime/build/mmd-stage-runner" -C Release --output-on-failure
```

## Evidence boundary

The installed reader admits at most one active PhysicsScene; the host selects
the backend factory and supplies a stage. The entire stage is one resource
lifetime. Cross-root joints therefore need no selective-unload policy inside
this scope; selective avatar removal with cross-avatar constraints remains open.
The existing separate resource-set path still excludes cross-set joints.

Application event wiring, automatic runtime instance/participant configuration,
stop/seek policy, resident PMX resources, rendering/Hydra agreement and licensed
representative L3 scenes remain open. The new Jolt case is stage/factory/resource
composition beside the existing explicitly configured scheduler fixture.
P1-C and P1-2 remain open in the [current roadmap](../roadmap/current.md).
