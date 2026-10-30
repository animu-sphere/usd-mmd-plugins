# Source damping runtime admission (2026-10-31)

## Scope and ownership

The owner now exposes `ReadSourceBodyProperties`, preserving the original
coefficients and the independently resolved typed damping responses. Structural
USD admission stays strict: missing/partial, blocked, sampled, varying, wrong-type
and nonfinite declarations refuse with the owner code and body path. Filtering
and contact conversion share the existing implementation. The original
`ReadBodyProperties` API and its finite-rate-only refusal behavior remain intact.
The [physics contract](../design/PHYSICS_INTEGRATION.md#72-body-damping) owns
the semantics; the [package contract](../architecture/PACKAGE_CONTRACT.md#mmdphysicsadapter)
owns feature discovery.

The runtime-owned native MMD session explicitly selects this source policy.
It maps finite rates or independent prior-velocity clearing into neutral physics
descriptors, without interpreting coefficients in generic physics. The session
returns owned normalization records containing the attribute path, original
value and effective coefficient. Successful Reset rereads properties and replaces
the records; malformed Reset admission can be repaired and retried. The Python
wrapper returns copies, and usdview reports normalization on construction/Reset.
No importer metadata or source file is rewritten.

Runtime details and evidence are retained in its
[capability matrix](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/reference/CAPABILITY_MATRIX.md#source-damping-runtime-evidence-2026-10-31)
and [session contract](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/architecture/MMD_SESSION.md).

## Generated verification

Verified on Windows x86-64, MSVC 19.51.36256, CMake 4.4, Ninja, Release,
OpenUSD 26.08 with Python 3.13.14 and deterministic Jolt 5.5.0 / six-DOF enabled.
MMD base: `167b3f15c05195d79ceb9fb806d2f41da6c8e7c4`; runtime base:
`1d870a9679414e884c452588b919734bdc531f4b`, each plus this working-tree change.
The physics dependency is the local backend checkpoint linked from the
[preceding report](2026-10-31-phase8-damping-backend-boundary.md).
Core, Jolt backend and USD bridge were rebuilt together in a new installed
prefix against the same OpenUSD installation, then all runtime adapters rebuilt.
This is local package composition, without a publication or pin claim.

- Owner adapter CTest: 4/4, including 25 source coefficient pairs, retained
  originals, independent typed responses, malformed declarations, nonmutation
  and unchanged strict-reader refusals.
- Runtime CTest: all 24 source/installed/core/live tests passed, including the
  new source damping test; a separate installed native-session consumer adds
  package lookup/linkage and native normalization/Reset/nonmutation coverage.
- Rebuilt physics CTest: 26/26, including its installed-consumer compatibility
  negative controls. The first installed-consumer attempt lacked Developer
  PowerShell environment and failed before the expected package refusal;
  rerunning that test in the documented compiler environment passed.

The drop check asserts `v_y = -9.81/120` after every clearing substep, and
`y = 2 - 240*9.81/120^2` after 240 ticks, tolerance `2e-5`. Thus new gravity
survives clearing; a huge post-force drag substitution fails this assertion.
Coefficients 1 and 2 yield identical trajectories, and -2 matches zero.
The keyed pendulum check runs eight policies through three generations of
120 ticks each: all independent zero/unit pairs, normalized counterparts and
finite-rate coexistence. It asserts active constraint-driven motion, unchanged
kinematic commands, finite state, exact reset replay, Pause, layer restoration,
changed-source re-admission and repair after nonfinite refusal. Queued-force
and contact torque clearing are covered by the neutral backend test, not by
a newly added force-injection API in this host.

Use Visual Studio Developer PowerShell, with `$env:_CL_ = '/Z7'` to avoid the
local compiler-probe PDB issue. Build/install the owner and complete physics
closure first, then configure the runtime using its
[live-host guide](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/guides/live-mmd-physics.md)
and those installed prefixes. Configure generated pendulum and anchor VMD
fixtures, then run:

```powershell
ctest --test-dir <owner-build> -C Release --output-on-failure
ctest --test-dir <physics-build> -C Release --output-on-failure
ctest --test-dir <runtime-build> -C Release --output-on-failure
```

## Bounded private intake without damping overrides

The same private pair used by the earlier preview has these SHA-256 identities:

- PMX: `b37ed18115891c296b0955515edd004067e446acd9268dec8a548215afb30403`.
- VMD: `737de0562a388d7ec91492826de995e6f3dde6ac8d5ddca784785029c2388c92`.

Unmodified direct PMX/VMD session construction refuses with
`six-DOF needs at least one dynamic body`. A second run authors anonymous
session-layer opinions disabling exactly 10 joints whose two endpoints are
kinematic, matching the inert-joint accommodation of the earlier preview.
No damping opinions, motion filtering or spring opt-ins are applied. The
original PMX/VMD files are hash-checked unchanged; assets, paths and screenshots
remain local and are not redistributed.

The scoped run has 461 skeleton joints, 265 bodies and 58 normalization records,
all for original coefficient 2 resolving to 1. It executes 120 fixed ticks,
pauses, resets and executes the same 120 ticks again. All exposed transforms
and velocities are finite; every body state replays exactly. Source/session
layers are unchanged after Close, except for the declared inert-joint opinions
already present before construction. The 240 physics ticks take 9.52 seconds
on this machine, excluding session construction. Existing unmatched bone/morph
diagnostics remain visible; no full-motion coverage is inferred.

## Limits and next gate

This resolves the scoped source damping consumer boundary without the temporary
0.99 layer. The raw private stage still requires an owner admission decision for
inert joints. This is numeric headless evidence over one second of motion per
generation, not renderer output, real-time performance, Bullet trajectories,
representative L3 or final interactive acceptance. Existing strict readers and
isolated probes do not automatically acquire the source-policy behavior.
