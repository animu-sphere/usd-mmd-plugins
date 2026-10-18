# Phase 8 joint spring metadata audit (2026-10-18)

Local Windows evidence after [contact mixing](2026-10-18-phase8-contact-mixing.md).
The isolated consumer now exposes the preserved spring coefficients omitted
from the generic snapshot and experimental execution. No spring force is added.

## Boundary

[Stage contract §13.2](../design/STAGE_CONTRACT.md#132-joints) preserves
`mmd:physics:translationSpring` and `mmd:physics:rotationSpring` as source
`vector3f` values. They cannot be copied into USD drive stiffness: their units
require a runtime conversion. The previous probe did not read either value.

The test-only `ReadJointSprings()` reads both constant values, rejecting a
partial pair, wrong type, absent/blocked value, time sample or non-finite
component. With neither attribute it returns zeros. Finite signed values are
preserved without scaling or clamping; this is a metadata audit, not a claim
that negative stiffness is physically supported. It audits format-only prims
as well as standard joints, and does not infer spring activation from type.

For each nonzero pair, stderr identifies the input, prim path and both vectors
as `unsupportedJointSprings`. Each accepted snapshot row includes
`unsupportedSpringJoints`, `unsupportedSpringAxes` and `jointFidelity`.
Nonzero coefficients or format-only joints mark fidelity `approximate`;
otherwise it remains `unverified`. `accepted` still describes snapshot
consumption and, when enabled, bounded experimental stepping, never complete
MMD simulation. Invalid metadata rejects that input with `consumerError`,
continues to subsequent inputs, and yields exit status 1.

## Results

Windows x86_64, MSVC 19.51, OpenUSD 26.08, deterministic Jolt 5.5.0, the
installed shared mixing packages used by the preceding report.

| Check | Result |
| --- | --- |
| Reader/execution CTests | All five pass in each mode |
| Spring metadata controls | Six independent axes; signed values; malformed types/values; partial pairs; samples; no stage edits |
| Probe regression | Nonzero vectors and approximation appear in output; malformed input rejects while the next zero control succeeds; a format-only joint marks approximation even with zero coefficients |
| Existing generic controls | All twelve accepted in both modes |
| Physics-bearing PMX fixtures | All 22 accepted, each explicitly approximate; three bodies/two standard joints, two constraints and 120 finite steps in execution mode |
| Spring audit | 23 prims with nonzero metadata, 115 nonzero components across the corpus, including one format-only prim in `recoverable/physics-repairs.pmx` |
| Physics-free fixtures | Six empty snapshots, zero spring counts; 120 empty steps in execution mode |

Most physics-bearing fixtures preserve translation `(0,10,20)` and rotation
`(5,6,7)` on one standard joint. The repair fixture also preserves those values
on a format-only joint, giving two prims/ten components there. These are source
coefficients, not SI stiffness measurements.

## Reproduction

Use the installed prefixes and configure both build directories as in the
[mixing report](2026-10-18-phase8-contact-mixing.md#reproduction), then run:

```powershell
cmake --build build/physics-consumer-mixing --config Release
cmake --build build/physics-consumer-mixing-reader --config Release
$env:PATH = "$env:USD_ROOT/bin;$env:USD_ROOT/lib;$env:PATH"
ctest --test-dir build/physics-consumer-mixing -C Release --output-on-failure
ctest --test-dir build/physics-consumer-mixing-reader -C Release --output-on-failure
```

With the same fixture paths and plugin environment as the
[filter report](2026-10-18-phase8-collision-filter.md#reproduction):

```powershell
& build/physics-consumer-mixing/Release/mmd_physics_consumer_probe.exe @stagePaths `
  2> build/physics-consumer-mixing/spring-diagnostics.log |
  Set-Content build/physics-consumer-mixing/spring-results.tsv
& build/physics-consumer-mixing-reader/Release/mmd_physics_consumer_probe.exe @stagePaths `
  2> build/physics-consumer-mixing-reader/spring-diagnostics.log |
  Set-Content build/physics-consumer-mixing-reader/spring-results.tsv
```

Both corpus runs exited 0. Importer output, generated fixtures, shared packages
and normal dependencies did not change. Spring unit conversion, neutral
spring execution and source response evidence remain open, as do the existing
damping, angular/zero-mass, format-only joint and bone-coupling fidelity gaps.
