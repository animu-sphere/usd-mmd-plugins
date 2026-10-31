# Generated combined MMD Hydra/direct parity (2026-10-31)

Continues the [material-only comparison](2026-10-31-phase8-canonical-material-parity.md)
with simultaneous final joints, vertex morphs, primary UVs, Jolt feedback and
canonical materials from a directly opened generated PMX/VMD. The renderer's
[report 114](https://github.com/animu-sphere/hydra-toon/blob/main/docs/reports/renderer/114-2026-10-31-mmd-combined-parity.md)
owns implementation, environment, reproduction and detailed measurements.
No MMD owner API, importer, schema or dependency pin changes are involved.

The runtime fixture generator combines existing owner-authored pendulum,
group/direct UV/vertex and material motion channels in an additional VMD.
Automatic resident bindings consume the same retained final snapshot while
the session independently publishes canonical USD for UsdImaging/mmdImaging.
Both products keep their evaluated avatar surface geometry. The unrelated
UsdSkelImaging skeleton diagnostic mesh has no runtime resident binding and
is excluded from both products.

The generated comparison covers 720 ticks and three reset generations.
Surface streams and canonical materials agree exactly; joint palettes use the
existing `2e-6` bound. Selected ticks have exact 128x128 colour/depth agreement
at 1x/4x MSAA and exact reset replay. Independently removing skin, vertex morph,
UV or material evaluation changes the combined image. Pause, zero restoration,
Release, retained values after Reset/Close and unchanged source/session layers
are checked.

All twelve selected renderer regressions and both focused runtime material/USD
output tests passed, with no skips. A byte comparison against the previous
runtime generator confirmed the original material PMX/VMD/texture are unchanged.
MMD documentation selftest and repository link/anchor checks passed; the
renderer documentation checks passed as well. Local runtime tests required
the documented OpenUSD/runtime/plugin DLL directories on PATH; the MMD docs
selftest used a workspace temporary directory because the sandbox's default
temporary directory refused writes.

Sampled tint realization is OFF because the owner defines no canonical USD
inputs for those coefficients. This does not establish private-asset parity,
additional runtime UV channels, representative appearance/contact acceptance,
broader lifecycle or the final interactive visual gate. P1-C/P1-2/P1-3 and the
licensed fixed L3 scene set remain open in the [roadmap](../roadmap/current.md).
