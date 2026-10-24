# Phase 8 owner-resolved non-bone morph channels (2026-10-24)

## Scope

This follows the [direct vertex publication check](2026-10-24-phase8-live-vertex-morph.md),
which identified raw group channels as a remaining owner interface need.
MMD base `f261430` plus this change; Windows x64, MSVC Release and the existing
OpenUSD 26.08 build configuration. No dependency pins, authored stage contracts,
importer behavior, runtime code or third-party assets change.

`Evaluator::ResolveMorphChannels` accepts raw `Pose::channels` from either
offline or split physics evaluation. The MMD owner resolves nested group
contributions, sums direct contributions and excludes bone/group outputs.
The pose layout and existing raw channels remain unchanged; shared-motion
export retains its source-channel contract. The implementation shares the
same group traversal as bone morph evaluation.

The normative semantics are in [MOTION section 11.3](../design/MOTION_CONTRACT.md#113-morphs).
The output is a scalar transport interface, not vertex/UV/material/impulse
realization or a new expression semantic. Consumers must preserve morph
identity, reject unsupported leaf types and validate finite output weights.

## Generated checks

The unit fixture contains a nested diamond with the outer group after the
inner group in canonical index order, direct and group contributions to one
vertex, bone targets, UV/material/flip/impulse leaves and unrelated leaves.
Its known vertex result is `2 * (0.5 * 2 + 0.25) - 0.25 * 2 - 0.5 = 1.5`.
The checks assert:

- exact known weights, ascending unique indices and unchanged raw input;
- additive duplicate inputs and unmodified negative/above-one weights;
- reached leaves retained at zero, including cancelling contributions and
  zero member coefficients, while unrelated leaves stay absent;
- no bone/group output and no expansion of flip members;
- invalid/out-of-model and bone inputs ignored, non-finite values unmodified;
- identical resolved weights before and after physics continuation and replay;
- existing bone morph pose behavior unchanged.

The robustness lane additionally checks resolved channel identity, ordering,
bitwise replay and finite weights for tame generated inputs across 20,000
models and five times each: 100,000 evaluations, zero failures.
The installed consumer resolves a generated mixed bone/vertex group through
installed headers and the static library, with an exact expected vertex weight
of 1.25. It retains the existing bound-VMD continuation/pose check.

All 13 selected control, motion-adapter, physics-adapter and installed-consumer
CTest checks pass. The complete Release build, documentation checker and its
selftest also pass.

## Reproduction and boundary

```powershell
cmake --build build/windows-msvc --config Release --target mmdControl_tests mmdControl_robustness_tests
ctest --test-dir build/windows-msvc -C Release -R "^(mmdControl_|mmdMotionAdapter_|mmdPhysicsAdapter_|workspace_installed_consumer)" --output-on-failure
python scripts/check_docs.py --selftest
python scripts/check_docs.py
```

Use the dependency and installed-package setup in the
[build guide](../guides/building.md). The installed-consumer lane installs to
a clean temporary prefix and builds its copied consumer against that prefix.
On restricted Windows hosts, use a writable workspace directory for `TEMP`
and `TMP`, and disable MSBuild node reuse if required by the environment.

Runtime must adopt the new owner API and publish its resolved vertex outputs
before claiming live group execution. UV/material consumers, representative
asset permission records and damping admission, resident Toon resources and
Hydra/direct agreement remain open. This owner increment does not close P1-C,
private full-motion intake or representative L3 acceptance.
