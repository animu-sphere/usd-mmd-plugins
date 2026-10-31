# Inert MMD joint admission (2026-10-31)

## Policy and scope

The owner now exposes `ReadJointAdmission` and feature discovery
`mmdPhysicsAdapter_JOINT_ADMISSION_VERSION=1`. The
[physics contract](../design/PHYSICS_INTEGRATION.md#76-inert-joint-admission)
owns its semantics. Enabled standard `spring6Dof` joints between two validated
`followBone` bodies are retained in USD and omitted from solver allocation.
At least one `dynamic`/`dynamicWithBone` endpoint retains execution admission.
Missing/other well-formed type metadata does not opt into omission. World and
non-MMD endpoints are outside this scoped classification API.

The native runtime session selects that owner policy. Its generic resource
adapter accepts a caller admission predicate after conversion and descriptor
validation, before any resource creation; it has no PMX branches. Spring
metadata remains validated for omitted joints, and unexecuted coefficients
remain visible in unapplied-metadata records. The neutral backend's dynamic
endpoint requirement is unchanged. No importer or source-layer edits occur.

Native `InertJoints()` returns owned path records from the latest successful
world build. Python `inertJoints` returns copies; usdview reports the paths on
construction and successful Reset. Reset rereads admission; invalid metadata
refuses and repaired declarations can be retried. Coupling-layout edits still
require session reconstruction.

## Generated verification

Windows x86-64, MSVC 19.51.36256, Release/Ninja, CMake 4.4, OpenUSD 26.08,
Python 3.13.14 and deterministic Jolt 5.5.0 with six-DOF enabled. Bases:

- MMD: `0ce7e72840f9a447c9f03d4e02e07b784bb9e04d` plus this change.
- Runtime: `24c0ad7ac6e50259cf30cb74a362715a5c3f5632` plus this change.
- Physics: `c45e3181201bc9e9a62569892604f6a31b225f61`, using the rebuilt local
  source-damping package closure from the
  [previous checkpoint](2026-10-31-phase8-source-damping-runtime.md).

No published package, release or dependency-pin update is claimed.

- Owner CTest: 5/5. The admission unit checks all nine endpoint mode pairs,
  malformed endpoints/mode/enablement/type declarations, missing/other type
  compatibility, independent returned storage and unchanged USD.
- Runtime CTest: 25/25, including installed consumers. Generic resource tests
  check selective omission, teardown, no-six-DOF all-omitted construction,
  validation of omitted descriptors and pre-allocation callback refusal.
- `hosts.usdview_mmd_admission`: generated pendulum plus an anonymous-layer
  second anchor and inert joint; 120 ticks in each of two generations, finite
  states and exact replay, Pause, copied records, changed-endpoint re-admission,
  invalid inert spring refusal/repair, unapplied coefficients and root/session
  restoration after Close. Existing missing/other spring type behavior passes.
- The installed native session consumer checks public inert paths, Reset,
  three live ticks and nonmutation for a generated fully kinematic pair.

Use Developer PowerShell and the installed prefixes from the
[live-host guide](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/guides/live-mmd-physics.md).
Build/install the owner before rebuilding runtime. Configure generated pendulum
and anchor VMD inputs as in that guide; admission coverage joins the live tests.

```powershell
cmake --build <owner-build> --config Release
ctest --test-dir <owner-build> -C Release --output-on-failure
cmake --install <owner-build> --config Release
cmake --build <runtime-build> --config Release
ctest --test-dir <runtime-build> -C Release --output-on-failure
```

Set OpenUSD `bin`/`lib` and the installed MMD `lib` on `PATH`, OpenUSD
`lib/python` on `PYTHONPATH`, and the installed `mmdSchema`/`usdMmdFileFormat`
resource directories on `PXR_PLUGINPATH_NAME`. The initial runtime invocation
omitted this outer environment and failed DLL/plugin discovery; the configured
rerun passed. The local MSVC build uses `_CL_=/Z7` for its compiler-probe PDB
workaround.

## Unmodified private intake

The same private pair as the preceding report has SHA-256 identities:

- PMX: `b37ed18115891c296b0955515edd004067e446acd9268dec8a548215afb30403`.
- VMD: `737de0562a388d7ec91492826de995e6f3dde6ac8d5ddca784785029c2388c92`.

Direct session construction now succeeds with zero damping or joint overrides,
no motion filtering and both experimental spring options OFF. There are 461
skeleton joints, 265 bodies, 10 owner-classified inert joints and 58 damping
normalization records (original 2 resolving to 1). The inert joint source
indices are 74, 79, 80, 82, 84, 85, 87, 88, 90 and 93.

The local check opens the original PMX, constructs the session with the original
PMX/VMD paths, retains both layer strings, and asserts 120 fixed ticks, Pause,
Reset and the same 120 ticks. Every exposed transform/linear velocity is finite
and every body state replays exactly. Reset returns the same inert paths.
Close empties body state and restores both layer strings; source-file hashes
remain unchanged. The 240 ticks took 6.39 seconds excluding construction; this
is a scoped timing, not a controlled performance comparison. Assets, local
paths and screenshots are not redistributed.

Unmatched source bone/morph diagnostics remain visible. This is two one-second
headless generations, not full-motion coverage, real-time rendering, native MMD
trajectory equivalence, a licensed fixed L3 scene set or final interactive
acceptance. Continue representative scene and application/renderer gates.
