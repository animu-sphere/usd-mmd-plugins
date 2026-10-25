# Phase 8 owner-side UV morph evaluation (2026-10-25)

## Scope

This follows the [resolved leaf-weight API](2026-10-24-phase8-resolved-morph-channels.md)
and [live vertex-group transport](2026-10-24-phase8-live-resolved-morph.md).
MMD base `2679e4a` plus the changes described here. Windows x64, MSVC 19.51
Release, Python 3.13 and OpenUSD 26.08 with the existing installed motion
packages. The new query remains OpenUSD-free and changes no dependency pins,
importer output, stage-contract version, solver or runtime code.

`Evaluator::EvaluateUvMorphs` accepts raw pose channels and resolves nested
groups on the MMD owner side. It accumulates weighted UV offsets into owned
sparse `(channel, vertex)` rows. Primary V is negated once for canonical `st`;
additional UV vec4s stay raw. The [motion contract](../design/MOTION_CONTRACT.md#113-morphs)
owns ordering, reachability, zero/absence, failure and application semantics.
Consumers add current deltas to immutable base UVs, validate final values and
bindings, and separately admit all other reached leaf types.

## Generated checks

The focused unit checks cover a nested diamond plus direct/duplicate channels,
duplicate offsets and overlapping UV morphs, negative/above-one weights,
all four additional channels, sorted sparse output, cancellation, zero member
coefficients, empty input, retained results, evaluator copy/move and source
destruction. A PMX document stated as data goes through canonicalization,
bound-key sampling and pre/post-physics continuation, checking the primary-V
conversion against independent base-plus-delta expectations. Returning to a
zero-weight frame does not accumulate previous output.

Absent additional channels, invalid vertices, NaN/infinite offsets or weights,
effective-weight overflow, weighted-offset overflow and sum overflow refuse
without a partial result, including reached malformed offsets at zero weight.
The installed `control_probe` calls the new symbol through installed headers
and archive, checking both primary and additional UVs through a group with
simultaneous direct input. These are generated numeric checks, with no
third-party assets or rendered-pixel evidence.

The Release workspace build succeeds and all 52 configured CTests pass,
including `mmdControl_unit`, the existing robustness/boundary regressions,
the clean installed consumer and coupling smoke. Documentation selftest,
relative-link/anchor checks and the 76-code diagnostic catalog check pass.

## Reproduction and remaining work

Use the existing [build setup](../guides/building.md). On this Windows host,
MSBuild runs with one node and node reuse disabled; a writable temporary
directory avoids the sandbox's default temporary-directory permissions.

```powershell
$env:TEMP = (Resolve-Path build/test-temp).Path
$env:TMP = $env:TEMP
$env:CMAKE_BUILD_PARALLEL_LEVEL = '1'
$env:MSBUILDDISABLENODEREUSE = '1'
cmake --build build/windows-msvc --config Release -- /m:1 /nr:false
ctest --test-dir build/windows-msvc -C Release --output-on-failure
python scripts/check_docs.py --selftest
python scripts/check_docs.py
```

Create `build/test-temp` first if it does not exist. Rebuild and reinstall the
static owner library and rebuild consumers together when adopting the query.
Live hosts still refuse UV tracks until runtime output and renderer bindings
are connected. Material/flip/impulse realization, composed renderer validation,
representative PMX/VMD L3 acceptance and P1-C remain open.
