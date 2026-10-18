# Phase 8 generated PMX regression and replay (2026-10-18)

## Scope and environment

P0-2 local automation evidence. MMD baseline
`57030cc7eae44c1379be17953a5c19e600339647` plus this working-tree change.
The shared installed prefix is the compatible deterministic build recorded in
the [package report](2026-10-18-phase8-package-compatibility.md); no shared
package, importer semantics or dependency pin changed. Windows x86_64,
MSVC 19.51.36256.0, CMake 4.4, Python 3.13.14, OpenUSD 26.08 and deterministic
Jolt 5.5.0, Release with the dynamic MSVC runtime. Importer and schema bundles
were rebuilt at the MMD baseline and installed into a separate prefix.

## Implementation and assertions

The isolated probe now creates two independent worlds from each accepted scene.
It runs each for 120 steps at 1/120 second and rejects a non-finite exposed
state or any numerically unequal transform or linear velocity at corresponding
body/frame samples. Body handles are deliberately excluded from comparison.
Output records `replay=identical` and `replaySamples`; an empty scene has zero
samples while both worlds still step. Internal angular velocity, contacts and
solver state are not exposed by the current shared interface and are not compared.

With `MMD_PHYSICS_MMD_PREFIX`, CTest registers a representative fixture check
through the installed importer/schema, with a plugin path that excludes source
bundles. The Python driver compares standard body/joint and metadata-reader
counts, format-only joints, source spring-fidelity counters and enabled
approximation axes to the committed generated `fixtures.json` expectations.
Spring-OFF configurations reject unexpected spring execution output.
Malformed/missing evidence fails, with five checker tests covering bad rows,
dropped bodies/source gaps, absent/incomplete replay and incorrect spring modes.

The bounded set is `sample-2.0-utf16.pmx`, `sample-2.1-utf8.pmx`,
`recoverable/physics-repairs.pmx`, `bones-reordered.pmx` and `minimal.pmx`.
It covers the three primitives/modes, both PMX versions, spring and plain 6-DOF
types, a preserved-only hinge, repaired values, reordered bone associations
and no physics. The wider generated corpus remains available as opt-in work.

## Results

| Check | Result |
| --- | --- |
| Reader-only suite, representative check and checker controls | 7/7 CTests pass |
| Execution with both spring options OFF | 7/7 CTests pass |
| Execution with both spring options ON | 11/11 CTests pass |
| Full opening corpus in each configuration | 28/28 PMX inputs accepted and asserted |
| Each execution corpus | 22 physics-bearing inputs, 360 state samples per input per world; six empty inputs with zero samples; both worlds run 120 steps |
| Source fixture generation, stage-open preservation and Unicode-path regression | 3/3 CTests pass |

The Unicode-path test initially encountered a sandbox temporary-directory
permission error; rerunning with `TMP`/`TEMP` in a writable build directory
passed. No product change was needed for that environment issue.

## Reproduction and limits

Follow the [optional physics consumer guide](../guides/building.md#optional-physics-consumer)
with compatible installed physics/SDK prefixes and `MMD_PREFIX` set to a current
MMD installation. Run its three configurations with
`MMD_PHYSICS_FULL_CORPUS=OFF` for the representative lane, then `ON` for the
opt-in full corpus. The same CTest entry handles either selection.

Local build/test evidence lives under `build/physics-package-{reader,execution,springs}`;
direct full-corpus logs are `regression-corpus.tsv` and
`regression-corpus-diagnostics.log` in those directories.

Exact numerical replay is bounded to the same process, build, fixed timestep
and configuration, without wall-clock input. It does not establish determinism
across platforms, compiler settings or SDK versions. Source spring gaps and
approximation labels remain visible; bone synchronization, feedback and avatar
L3 remain unverified. Hosted CI wiring and verification remain in
[P0-2](../roadmap/current.md#active-gates).
