# Phase 8 stage validation before coupling (2026-10-20)

Follow-up: [Windows USDC lifetime correction](2026-10-20-phase8-stage-validation-windows.md).

## Scope

This verifies PR-A under [STAGE_CONTRACT §13.5](../design/STAGE_CONTRACT.md#135-consumer-validation-and-round-trip-preservation)
and [PHYSICS_INTEGRATION §9](../design/PHYSICS_INTEGRATION.md#9-implementation-sequence).
The source baseline is MMD `26d0bb3`, with the working-tree changes described
below, on Windows x86_64 / MSVC 19.51 / Python 3.13.14 / OpenUSD 26.08.
Shared physics and Jolt use the installed prefixes from the
[host Python SDK validation](2026-10-20-phase8-physics-ci-python.md).

## Preservation

The existing stage checker now requires exact Sdf types, uniform variability,
authored values and no time samples for every preserved physics attribute.
Existing fixture assertions cover body/joint counts, child/source order,
provenance, canonical bone associations, all three modes, shapes, filters,
converted dimensions and frames, local joint frames, limits and springs.

[test_physics_roundtrip.py](../../tests/integration/test_physics_roundtrip.py)
exports every opening generated fixture with physics to both USDA and USDC,
reopens each file, and repeats the physics and skeleton assertions. It also
checks Y-up and meters and derives body/bone offsets by concatenating the
skeleton's parent-local rest transforms. These offsets are compared against
independent fixture body orientations and body-minus-bone positions, then
compared again after save/reopen. Existing parent/child and reordered-bone
fixtures supply nonzero offsets; `-1` is excluded from pose association.
All 22 physics fixtures pass both output formats. No new offset attribute,
third-party asset or source repair is introduced.

## Strict consumer declarations

The isolated probe validates canonical `/Asset/physics` declarations before
the shared reader and before optional execution. Required preservation values
must have their contract types, uniform variability and constant readable
values. Joint source types and schema mappings must agree; both source body
indices must resolve and agree with standard joint endpoint relationships.

The test-only bone-binding reader accepts `followBone`, `dynamic` and
`dynamicWithBone`, checks agreement with the standard kinematic flag, and
resolves canonical indices against the supplied skeleton. It distinguishes
`-1` from invalid negative and out-of-range indices. Missing, blocked, wrongly
typed, varying or time-sampled declarations reject with prim/field diagnostics.
An absent skeleton rejects an associated body; unassociated bodies remain
readable and emit `unboundBone` with the limitation. Conventional USD bodies
without MMD declarations retain their generic behavior.

Focused C++ tests cover positive modes, sentinel behavior, missing and invalid
bone/joint references, malformed attributes, source-only joint types and
standard relationship mismatches. A CLI smoke proves that a malformed
associated body rejects while a subsequent `-1` control remains accepted and
reports its limitation. Existing filter, coefficient, damping and spring
negative controls remain in all applicable lanes.

These helpers live only in `tests/physics_consumer`; they establish no public
component, installed coupling API or runtime pose feedback. Fixed canonical
paths are appropriate for this importer-contract probe, not an API for
arbitrarily composed avatars.

## Commands and results

From the repository root, using the existing configured build directories:

```powershell
ctest --test-dir build/windows-msvc -C Release `
  -R 'usdMmdFileFormat_(stage_open|physics_roundtrip)$' --output-on-failure

$env:PATH = "$env:USD_ROOT/bin;$env:USD_ROOT/lib;$env:PATH"
$env:PYTHONPATH = "$env:USD_ROOT/lib/python"
$env:PXR_PLUGINPATH_NAME = ''
foreach ($configuration in @('reader', 'execution', 'springs')) {
  $directory = "build/physics-ci-python-fixed/$configuration"
  cmake --build $directory --config Release
  if ($LASTEXITCODE) { throw 'Consumer build failed' }
  ctest --test-dir $directory -C Release --output-on-failure --timeout 30
  if ($LASTEXITCODE) { throw 'Consumer tests failed' }
}
$env:TEMP = (Resolve-Path build/docs-check-temp).Path
$env:TMP = $env:TEMP
python scripts/check_docs.py --selftest
python scripts/check_docs.py
```

Stage checks pass 2/2; installed reader passes 10/10, execution 10/10 and
experimental springs 14/14. The latter two retain finite-state and identical
same-build replay assertions over five representative inputs. Documentation
checks and their selftest pass.

A broader `cmake --build build/windows-msvc --config Release` fails in the
existing motion adapter/skeleton test links: installed `motionSource` refers
to unresolved `ApplyBasisToPosition` and `ApplyBasisToRotation` symbols.
The importer and affected-scope physics builds succeed; this run therefore
does not establish a passing full-workspace build. An initial direct consumer
CTest invocation without the OpenUSD DLL paths also failed; the commands above
include the required runtime environment, as the existing CI driver does.
The docs selftest uses a workspace scratch directory because this session's
default sandbox temporary directory denied writes.

PR-A establishes static preservation and bounded declaration validation.
Coupling, placement/non-unit-scale conversion, runtime ordering and avatar L3
acceptance remain the subsequent roadmap gates.
