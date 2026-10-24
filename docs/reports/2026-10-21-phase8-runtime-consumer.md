# Phase 8 first runtime coupling consumer (2026-10-21)

The mode-1 no-feedback interpretation recorded here is corrected by the [2026-10-25 follow-up](2026-10-25-phase8-dynamic-bone-feedback.md).

## Scope and ownership

Validates the installed `mmdPhysicsAdapter`/`mmdControl` boundary with the first
optional `usd-avatar-runtime` registration consumer. The runtime owns
`avatarMmdAdapter`, per-instance continuation lifetime and shared-world barriers;
MMD owns BeginPhysics/ResumePhysics and all bone/body semantics. The importer
and MMD libraries are unchanged by this validation. The host test alone links
the installed physics reader and Jolt backend.

MMD base: `2806fc4` (0.2.0). Runtime base: `b59f452` plus the registration
change ([runtime PR #28](https://github.com/animu-sphere/usd-avatar-runtime/pull/28)).
Windows x64, MSVC 19.51, Release, OpenUSD 26.08, 120 Hz. The local
physics install advertises package contract 1, available Jolt and six-DOF;
the smoke uses no constraints or spring approximation. These local installs
are not a published dependency or hosted CI claim.

The runtime's
[adapter boundary](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/architecture/MMD_ADAPTER.md)
owns configuration and lifetime. Its
[capability matrix](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/reference/CAPABILITY_MATRIX.md#scoped-mmdjolt-evidence-2026-10-21)
owns runtime support/evidence. P1-1's path-to-world-handle gate is satisfied;
remaining composition work stays in [current.md](../roadmap/current.md).

## Assertions and measurements

- Both callbacks use the existing constraints phase with a pre -> post
  dependency. No new core phase, C ABI or format branch is introduced.
- A constructed three-bone stage exercises all modes with separate 60/80-degree
  feedback for two instances, nonzero offsets and a rotated/translated placement.
  The final CPU snapshot contains post append, preserves bone translation and
  maps joints by identity despite an unrelated leading runtime joint.
- The generated `coupling-modes.pmx` and `sample.vmd` execute through real owner
  continuation/coupling, concrete installed Jolt body handles and retained final
  CPU pose. Identical body paths in the two instances use separate handle maps.
- All prefixes submit targets before one shared Step; all suffixes consume
  captured body states after it. Each final joint matches direct owner feedback
  and ResumePhysics at the runtime tick time. Follow-body commands reach
  kinematic handles; free dynamic bodies never drive the root; feedback rotation
  and post append remain nonzero.
- The Jolt run executes 24 ticks at 120 Hz per replay, three runs with full
  world reconstruction and fresh handles. Body/bone transforms stay finite;
  final poses repeat exactly on this build. Dynamic bodies start at a declared
  60-degree rotation with 0.1 m/s X velocity to expose feedback.
- Constructed tests cover zero/multiple ticks, pause, instance isolation,
  retained snapshots, three resets, pre failure retry, post failure invalidation
  and recovery. Generation-keyed continuations handle reset's overlap between
  old/new provider states. Owner diagnostic code/location/origin/severity and
  runtime instance/evaluator/tick context are retained.
- Explicit scalar morph/visibility mapping and unmapped-output refusal are
  tested. This does not establish UV/material/impulse morph realization.
- Separate installed adapter and core C consumers pass. Core-only lookup imports
  no MMD or physics target; the MMD registration component imports no backend.

## Reproduction

From this checkout, prepare generated inputs and a clean installed MMD prefix:

```powershell
$mmd = (Resolve-Path .).Path
$runtime = (Resolve-Path ../usd-avatar-runtime).Path
$physics = (Resolve-Path ../usd-physics-plugins).Path
$usd = '<OpenUSD-26.08-install>'
python tests/fixtures/generate_vmd_fixtures.py --out build/vmd-fixtures
cmake --install build/windows-msvc --config Release --prefix build/runtime-mmd-prefix
cmake -S $runtime -B "$runtime/build/mmd-physics" -A x64 `
  -DAVATAR_BUILD_MMD_ADAPTER=ON -DAVATAR_MMD_JOLT_SMOKE=ON `
  "-DCMAKE_PREFIX_PATH=$mmd/build/runtime-mmd-prefix;$mmd/build/physics-ci-shared-prefix;$usd" `
  "-DJolt_DIR=$physics/build/jolt-six-dof-sdk-install/lib/cmake/Jolt" `
  "-DAVATAR_MMD_PMX=$mmd/plugins/usdMmdFileFormat/tests/fixtures/coupling-modes.pmx" `
  "-DAVATAR_MMD_VMD=$mmd/build/vmd-fixtures/sample.vmd"
$env:PATH = "$usd/bin;$usd/lib;$mmd/build/runtime-mmd-prefix/lib;" + $env:PATH
$env:PXR_PLUGINPATH_NAME = "$mmd/build/runtime-mmd-prefix/plugin/resources/mmdSchema;$mmd/build/runtime-mmd-prefix/plugin/resources/usdMmdFileFormat"
cmake --build "$runtime/build/mmd-physics" --config Release
ctest --test-dir "$runtime/build/mmd-physics" -C Release --output-on-failure
```

The physics/Jolt prefixes above are the existing local installed packages;
build them under their owner's instructions before reproduction. All seven
runtime tests pass: four core tests, constructed MMD, installed MMD and Jolt
smoke. A separate core-only build and MMD-only build verify optional dependency
isolation. Documentation checks and whitespace validation are run separately.

## Evidence boundary

This scene has no joints and explicitly excludes body contact pairs. It checks
the first consumer's coupling/scheduling contract, not settling, contact,
spring/limit fidelity or L3. No stage-runner host, real model/motion acceptance,
Hydra/direct parity or resident-avatar rendering is claimed. The renderer has
not yet been shown to consume this physics result. P1-C and P1-2 stay open.
