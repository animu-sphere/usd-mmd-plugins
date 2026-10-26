# Phase 8 resident primary UV mapping (2026-10-26)

## Scope

Continues the [explicit Toon UV transport](2026-10-26-phase8-toon-primary-uv.md)
from MMD base `9574607` and renderer base `70811e4` plus this change.
Implementation is renderer-owned; no importer, MMD evaluator, runtime ABI,
dependency pin or third-party asset changed.

The renderer's
[report 103](https://github.com/animu-sphere/hydra-toon/blob/main/docs/reports/renderer/103-2026-10-26-avatar-resident-uv.md)
owns the implementation, environment and reproduction commands. Its generated
Hydra checks retain source-point identity through corner and tangent-seam
splitting and automatically map reordered runtime primary UV slots. Invalid
or incomplete mappings produce per-mesh diagnostics. Generated meshes without
resident tangents also bind, publish absolute values and restore their baseline
through Release. Tangent-bearing bindings continue to refuse.

## Acceptance boundary

This supplies the automatic renderer mapping prerequisite for the next
composed MMD consumer slice. It does not extend the earlier installed MMD
transport result to automatic MMD-to-GPU playback. Coherent tangent
regeneration, composed resident binding, runtime/Hydra agreement, additional
UV/material realization and representative PMX/VMD L3 acceptance remain open.
P1-C, P1-2 and P1-3 remain open.

MMD documentation selftest and link/anchor/version/diagnostic checks pass.
The renderer's focused CPU mapping and primvar/skinning checks pass; its
report records rendering regression evidence separately from MMD acceptance.
