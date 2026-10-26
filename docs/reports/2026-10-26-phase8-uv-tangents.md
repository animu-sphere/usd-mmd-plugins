# Phase 8 renderer UV tangent regeneration (2026-10-26)

## Scope

Continues [resident UV mapping](2026-10-26-phase8-resident-uv-mapping.md)
from MMD base `46aefef` and renderer base `0666297` plus this change.
Implementation is renderer-owned. No MMD importer, evaluator, runtime ABI,
dependency pin or third-party asset changed.

The renderer's
[report 104](https://github.com/animu-sphere/hydra-toon/blob/main/docs/reports/renderer/104-2026-10-26-avatar-uv-tangents.md)
owns implementation, environment and reproduction commands. Tangent-bearing
resident UV bindings now regenerate coherent rest frames and expand new seams
across geometry, skin, morph ranges and auxiliary streams. Unchanged UVs retain
authored frames; Release restores the immutable baseline. Unusable frames use
the derivative fallback, and incomplete seam streams refuse atomically.

## Acceptance boundary

Generated renderer checks compare automatically matched runtime UV snapshots
with the same values authored through USD/Hydra. Vertex streams and
normal-mapped Vulkan pixels agree through mirrored seams, reflection,
collapsed UV fallback and return to base, including 1x/4x MSAA. The no-Vulkan
suite, installed consumers, direct CPU/GPU and focused Hydra checks pass.

This supplies the tangent regeneration prerequisite for composed MMD resident
consumption. The new pixel checks do not include the MMD owner/evaluator.
Composed MMD-to-GPU playback and its runtime/Hydra agreement, additional runtime
UV channels, material realization and representative PMX/VMD L3 acceptance
remain open. P1-C, P1-2 and P1-3 remain open.

MMD documentation selftest and relative-link, anchor, version and diagnostic
catalog checks pass.
