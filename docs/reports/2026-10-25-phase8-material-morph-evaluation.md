# Phase 8 owner-side material morph evaluation (2026-10-25)

## Scope

This follows the [owner UV query](2026-10-25-phase8-uv-morph-evaluation.md)
and tracks [issue #98](https://github.com/animu-sphere/usd-mmd-plugins/issues/98).
MMD base `c13ed0f` plus the changes described here. Windows x64, MSVC 19.51
Release, Python 3.13 and OpenUSD 26.08 with the existing installed motion
packages. The query remains OpenUSD-free and changes no dependency pins,
stage authoring, solver, runtime ABI or renderer code.

`Evaluator::EvaluateMaterialMorphs` accepts raw pose channels and returns
owned per-material multiply/add modulation after owner group resolution.
The [material contract](../design/MATERIAL_POLICY.md#11-material-morphs)
owns the formula, target selection, ordering, reachability and refusal rules.
Separate multiplication/addition accumulation is consistent with Saba's
[`PMXModel` implementation](https://github.com/benikabocha/saba/blob/master/src/Saba/Model/MMD/PMXModel.cpp)
(`BeginMorphMaterial`, `MorphMaterial`, `EndMorphMaterial`, `MaterialFactor`).
This is a semantic reference, not an external differential or MMD pixel test.

## Generated checks

Focused units check all 28 modulated components with distinct numeric values,
mixed operation order, nested diamond groups, direct and duplicate channels,
all-material plus explicit overlapping targets, repeated offsets, negative
and above-one weights, cancellation, reached zero coefficients, sparse output
ordering, empty input and empty material tables. Prepared copies and moves
retain source data; prior results survive later calls and source destruction.

A PMX document stated as data passes through canonicalization and bound-key
sampling. Atomic and resumed pre/post-physics poses supply identical material
results; independent immutable-base expectations cover diffuse RGB/alpha,
and returning to zero weight restores identity without frame accumulation.
Tests distinguish addition before multiplication from sequential mutation of
the material value.

Invalid indices/operations, NaN/infinite source components and weights, and
effective-weight, weighted-term, multiplication-product and addition-sum
overflow refuse without a partial result, including malformed declarations at
zero weight and all-material offsets with no targets. Failure does not poison
later queries. The installed `control_probe` links the new symbol through
installed headers/archive and checks group-plus-direct multiply/add outputs
with simultaneous explicit/all-material targets.

The Release workspace build succeeds and all 52 configured CTests pass,
including owner units, robustness/boundary regressions, the clean installed
consumer and coupling smoke. Documentation selftest and relative-link/anchor
checks pass; the diagnostic catalog agrees with all 77 declared codes.

## Reproduction and remaining work

Use the existing [build setup](../guides/building.md). On this host, create
`build/test-temp` first and run:

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

Rebuild and reinstall the static owner and rebuild consumers together when
adopting this query. Consumers must validate their immutable bases, final
arithmetic, identities and bindings before publication. Texture/sphere/toon
tints need renderer support for sampled contributions; they cannot be
substituted with diffuse overrides. Runtime material publication, live/Hydra
output, rendered acceptance, representative PMX/VMD L3 and P1-C remain open.
