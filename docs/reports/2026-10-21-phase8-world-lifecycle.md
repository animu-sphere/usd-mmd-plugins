# Phase 8 host session world registration lifetime (2026-10-21)

## Scope and ownership

Extends the [host-session evidence](2026-10-21-phase8-host-session.md) with an
owned runtime world registration and explicit close in `usd-avatar-runtime`.
Its [session contract](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/architecture/STAGE_RUNNER_SESSION.md)
owns construction, close and resource lifetime. The existing borrowed-world
constructor remains available. Solver creation, body discovery and MMD coupling
retain their owners; the MMD libraries and physics-free importer are unchanged.

MMD base `4ec92b7`, runtime base `62b869e` plus
[runtime PR #32](https://github.com/animu-sphere/usd-avatar-runtime/pull/32)
(commit `1fd2403`).
Windows x64, MSVC 19.51, Release, OpenUSD 26.08. Installed Stage Runner clock,
MMD, physics/Jolt and ABI-4 Toon inputs match the preceding host-session report.
These are local source/install checks without publication or hosted CI claims.
Detailed runtime assertions belong to its
[capability matrix](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/reference/CAPABILITY_MATRIX.md#scoped-session-world-lifetime-evidence-2026-10-21).

## Results

The session-only configuration passes six tests, including a separate installed
consumer of the new API without MMD, OpenUSD, physics or renderer dependencies.
The MMD/Stage Runner/Jolt/Toon configuration passes all twelve tests.

The session creates a runtime registration from an explicit world descriptor.
Close removes that registration before external instance/body resources are
released; it does not step or reset the solver. Repeated close is harmless,
closed calls reject without clock/provider activity, and destruction also
removes registration. Two-avatar instance reset and registration under the same
world ID work, while another world in the runtime continues. Borrowed-world
close leaves the caller's registration intact.

The generated MMD host now uses this owning constructor. Its existing
two-avatar plus non-avatar shared Step, held input, owner continuation,
three 24-tick reset/replays and installed Toon transport assertions pass.
At teardown it closes the session, destroys both instances and releases the
Jolt solver and body maps. Retained CPU pose values remain readable after this
sequence, and the renderer's independently retained output survives producer
destruction. Neither close nor subsequent display invokes a solver callback.

## Reproduction

Prepare the installed packages, optional session and DLL/plugin environment as
in the [host-session report](2026-10-21-phase8-host-session.md#reproduction).
The commands run against the changed runtime tree are:

```powershell
$runtime = (Resolve-Path ../usd-avatar-runtime).Path
cmake --build "$runtime/build/stage-runner-session-only" --config Release
ctest --test-dir "$runtime/build/stage-runner-session-only" -C Release --output-on-failure
cmake --build "$runtime/build/mmd-stage-runner" --config Release
ctest --test-dir "$runtime/build/mmd-stage-runner" -C Release --output-on-failure
```

## Evidence boundary

This is whole-world registration shutdown with explicitly prepared resources.
It does not implement selective avatar removal while other members of that
world keep simulating. Stage discovery, automatic body/world construction,
`StageSession` and application/UI wiring, resident PMX rendering, Hydra/direct
agreement and licensed representative L3 scenes remain open. P1-C and P1-2
remain open in the [current roadmap](../roadmap/current.md).
