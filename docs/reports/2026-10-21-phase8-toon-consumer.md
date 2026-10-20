# Phase 8 final MMD/Jolt snapshot to Toon consumer (2026-10-21)

## Scope and ownership

Extends the [first runtime consumer](2026-10-21-phase8-runtime-consumer.md)
with an opt-in installed renderer transport check in `usd-avatar-runtime`.
Runtime base: `4d7279c` plus the transport tests
([runtime PR #29](https://github.com/animu-sphere/usd-avatar-runtime/pull/29),
commit `43e5f99`). MMD base: `a19f7d4`, using
the existing installed MMD 0.2 owner packages and physics descriptor-contract-1
packages from the preceding report. Windows x64, MSVC 19.51, Release,
OpenUSD 26.08, local installed `Toon::AvatarState` built for runtime ABI 4.
These are local installed-package results, not published or hosted evidence.

The runtime test host supplies explicit generated render resources and passes
the final retained snapshot to the renderer-owned adapter. The reusable MMD
adapter, runtime core and importer acquire no renderer dependency. MMD control
and bone/body semantics stay in their owners; no production API or ABI changes
are introduced. The runtime's
[output boundary](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/architecture/OUTPUT_PATHS.md#scoped-mmd-transport-check)
and [capability matrix](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/reference/CAPABILITY_MATRIX.md#scoped-mmdtoon-evidence-2026-10-21)
own the consumer contract and runtime evidence.

## Assertions and results

The fresh opt-in build passes all nine tests: four core tests, three existing
MMD/installed/Jolt tests and two renderer consumer tests.

- `adapters.mmd_toon`: distinct constructed feedback for two instances reaches
  separate renderer bindings, including post append, nonidentity placement and
  an unrelated runtime joint. A nonzero scalar morph and hidden/visible target
  exercise resolved channels. Pause, multiple ticks, failed post-Step publication
  and reset preserve the publication boundary.
- `adapters.mmd_jolt_toon`: generated `coupling-modes.pmx` and `sample.vmd`
  execute through owner continuation, installed Jolt handles and final snapshots
  into the installed Toon adapter for both instances. Each run covers 24 ticks
  at 120 Hz and three complete world reset/replays. Existing direct owner
  feedback/ResumePhysics comparisons still precede the renderer handoff.
- An independent double-precision USD matrix oracle compares all parent-composed
  palette entries, with reversed renderer joint order and centimetre scene
  units. Translation is compared in metres; the tolerance is
  `abs(actual-expected) <= 2e-6 * max(1, abs(expected))`. Scalar morph weights
  equal the expected float conversion; visibility and extracted draw values
  agree. A deliberately corrupted palette is rejected by the oracle.
- Frame, instance, generation, input revision, layout and binding epoch survive
  transport. Duplicate updates reuse dynamic arrays. Independently retained
  first/current snapshots remain readable after runtime destruction. An older
  generation is refused without changing the current palette.
- Separate installed MMD component lookup imports no physics/backend or renderer
  target. An older installed renderer header pinning runtime ABI 3 is refused
  during configuration with `AR_MMD_TOON_ABI`, before the test host builds.

## Reproduction

From this MMD checkout, first prepare the installed owner packages and generated
inputs as in the [preceding report](2026-10-21-phase8-runtime-consumer.md#reproduction).
Use a Toon installation containing the `AvatarState` component built against
runtime ABI 4; an older same-version installation is insufficient.

```powershell
$mmd = (Resolve-Path .).Path
$runtime = (Resolve-Path ../usd-avatar-runtime).Path
$physics = (Resolve-Path ../usd-physics-plugins).Path
$usd = '<OpenUSD-26.08-install>'
$toon = '<installed-Toon-ABI4-prefix>'
cmake -S $runtime -B "$runtime/build/mmd-toon" -A x64 `
  -DAVATAR_BUILD_MMD_ADAPTER=ON -DAVATAR_MMD_JOLT_SMOKE=ON `
  -DAVATAR_MMD_TOON_SMOKE=ON `
  "-DCMAKE_PREFIX_PATH=$mmd/build/runtime-mmd-prefix;$mmd/build/physics-ci-shared-prefix;$usd" `
  "-DJolt_DIR=$physics/build/jolt-six-dof-sdk-install/lib/cmake/Jolt" `
  "-DToon_DIR=$toon/lib/cmake/Toon" `
  "-DTOON_AVATAR_RUNTIME_INCLUDE_DIR=$runtime/include" `
  "-DAVATAR_MMD_PMX=$mmd/plugins/usdMmdFileFormat/tests/fixtures/coupling-modes.pmx" `
  "-DAVATAR_MMD_VMD=$mmd/build/vmd-fixtures/sample.vmd"
cmake --build "$runtime/build/mmd-toon" --config Release
$env:PATH = "$usd/bin;$usd/lib;$mmd/build/runtime-mmd-prefix/lib;" + $env:PATH
$env:PXR_PLUGINPATH_NAME = "$mmd/build/runtime-mmd-prefix/plugin/resources/mmdSchema;$mmd/build/runtime-mmd-prefix/plugin/resources/usdMmdFileFormat"
ctest --test-dir "$runtime/build/mmd-toon" -C Release --output-on-failure
```

## Evidence boundary

The renderer consumes an explicit triangle probe with identity inverse binds,
not the model's resident mesh/material resources. Visibility membership changes
use ordinary draw extraction. Constructed channels establish scalar morph and
visibility transport; the generated Jolt scene establishes final pose transport.
It still excludes contacts and joints. No GPU rendering, Hydra/direct parity,
Stage Runner composition, material/UV/impulse realization or real-model L3
acceptance is claimed. P1-C and P1-2 remain open under
[the current roadmap](../roadmap/current.md).
