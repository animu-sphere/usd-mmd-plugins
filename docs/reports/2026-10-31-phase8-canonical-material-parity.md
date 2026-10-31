# Generated canonical material Hydra/direct parity (2026-10-31)

Continues the [live material checkpoint](2026-10-31-phase8-live-material-integration.md)
with a composed renderer comparison. The renderer's
[report 113](https://github.com/animu-sphere/hydra-toon/blob/main/docs/reports/renderer/113-2026-10-31-mmd-material-parity.md)
owns the implementation, build identities, reproducible commands and measured
results. No MMD owner API, schema, dependency pin or third-party asset changes
are part of this checkpoint.

The generated direct-PMX session's canonical material values reach both the
resident snapshot consumer and runtime-authored USD read by UsdImaging and
mmdImaging. The comparison found a renderer material-structure discrepancy:
diffuse alpha below one was Blend in the direct path and Opaque through
Hydra. Renderer normalization now agrees, including opaque evaluated results
from translucent bases and immutable-base release.

Two new checks compare exact canonical normalized materials across 720 fixed
ticks and three reset generations, plus exact colour/depth at 1x/4x MSAA for
selected ticks. They verify automatic binding, pause, reset/replay, zero
restoration and retained values after close. GPU products deliberately hold
geometry at the imported baseline, isolating material delivery. Visible pixel
changes and reset replay prevent a vacuous equal-image result. All ten selected
renderer state, material, GPU, resident and viewport regressions passed.

Sampled texture/sphere/toon coefficients remain direct runtime identities
without canonical USD inputs; their realization is OFF for this comparison.
Their [earlier direct GPU evidence](2026-10-31-phase8-live-material-integration.md)
remains separate. This establishes generated canonical material path agreement,
not representative appearance, private Hydra/direct parity or combined
joint/vertex/UV/physics rendering. P1-C/P1-2/P1-3 and licensed fixed-scene L3
acceptance remain open in the [roadmap](../roadmap/current.md).
