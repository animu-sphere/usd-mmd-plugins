# Phase 8 typed runtime material snapshots (2026-10-27)

## Scope

Continues [owner material evaluation](2026-10-25-phase8-material-morph-evaluation.md)
with runtime base `a76aaf4`, implementation commit `9f39438` in
[runtime PR #47](https://github.com/animu-sphere/usd-avatar-runtime/pull/47),
and MMD checkout `7fb9fea`.
Windows x64, MSVC 19.51 Release, OpenUSD 26.08 and compatible locally installed
MMD packages. Implementation belongs to `usd-avatar-runtime`'s optional MMD
registration adapter. No importer, owner algorithm, dependency pin, core ABI
or renderer implementation changed.

The runtime's
[adapter contract](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/architecture/MMD_ADAPTER.md#configuration-and-ownership)
owns explicit material leaf admission, immutable bases and typed field bindings.
Post-physics publication consumes installed `EvaluateMaterialMorphs` on raw
pose channels and applies its multiply/add results to immutable bases. All
nine material fields, including distinct texture/sphere/toon contribution
tints, publish through the existing typed material state writer. Reached
materials require complete bindings even at zero weight; identities, types
and final arithmetic are validated before output writers run. Configure and
installed-package lookup probe both owner headers and linked query symbols.

## Generated evidence

Source and installed-consumer checks cover all 28 components with distinct
binary-fraction inputs for two materials and two runtime instances. Group and
direct weights combine through owner evaluation. All-material multiplication
and explicit addition are checked against independent expected values, with
reordered runtime slots and an unrelated material input preserved.

Repeated active frames do not accumulate, zero and absent weights restore
immutable bases, and three whole-world reset generations reproduce values.
Retained snapshots survive producer destruction. A final float overflow after
a successful tick leaves the prior retained frame intact, publishes no new
snapshot and recovers after whole-world reset. Missing/mistyped input bindings,
unbound reached-zero materials, nonfinite owner offsets, overflowing final
values, incomplete/duplicate fields and invalid bases are refused.

The material fixture uses caller-synthesized completed body transforms through
the actual runtime pre/Step/post barrier. It does not claim material-bearing
solver or rendered-asset evidence. The existing generated PMX/VMD/Jolt and
primary-UV Toon transport checks pass as regressions; they do not consume
the new material fields in a renderer.

## Reproduction and limits

Use the runtime's
[MMD build setup](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/architecture/MMD_ADAPTER.md#build-and-installation)
with compatible installed packages. From the runtime checkout, the configured
local MMD-only build and existing opt-in Jolt/Toon build were checked with:

```powershell
cmake --build build/mmd-only --config Release -- /m:1 /nr:false
ctest --test-dir build/mmd-only -C Release --output-on-failure
cmake --build build/mmd-toon-uv --config Release -- /m:1 /nr:false
ctest --test-dir build/mmd-toon-uv -C Release --output-on-failure
```

Both builds require owner dependency DLL directories on `PATH`. The opt-in
PMX/Jolt checks additionally require installed `mmdSchema` and
`usdMmdFileFormat` resource directories on `PXR_PLUGINPATH_NAME` as described
by the runtime's live-host setup. The six MMD-only checks and nine extended
checks pass. MMD documentation selftest and documentation checks pass.

Automatic material discovery, live USD material publication, renderer tint
realization and composed material pixels remain open. No private PMX/VMD,
additional runtime UV, representative L3 or final dedicated-viewport live
physics acceptance is established. P1-C, P1-2 and P1-3 remain open.
