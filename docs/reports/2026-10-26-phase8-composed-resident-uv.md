# Phase 8 generated composed resident UV (2026-10-26)

## Scope

Continues [renderer tangent regeneration](2026-10-26-phase8-uv-tangents.md)
from MMD base `8c1e1eb`, runtime checkout `a76aaf4` and renderer base `ccae76b`
plus this change. Implementation is a renderer-owned opt-in integration test;
no MMD importer, evaluator, runtime ABI or dependency pin changed.

The renderer's
[report 105](https://github.com/animu-sphere/hydra-toon/blob/main/docs/reports/renderer/105-2026-10-26-mmd-resident-uv.md)
owns implementation, environment and reproduction commands. A generated MMD
canonical model and bound motion run the installed owner evaluator through
runtime pre/post callbacks and real retained snapshots. Automatic resident
bindings consume reordered UV slots and regenerate tangent seams.

## Evidence and acceptance boundary

Independent expected arrays check owner/runtime absolute UVs, including nested
groups, signed and above-one weights, collapsed UVs and base restoration.
Three reset generations agree with the same values authored through USD/Hydra
on geometry, UV, tangent and auxiliary streams. Generated textured MMD and
normal-mapped MToon Vulkan colour/depth agree exactly at 128 by 128, 1x/4x
MSAA. Pixels change with morphs and return to base at zero/reset. Retained
snapshots survive producer destruction and Release restores immutable bases.

The generated provider has no bones and uses an empty runtime physics world;
the new check does not read PMX/VMD or run a solver. This establishes UV-only
MMD owner/runtime-to-GPU composition, not full-avatar playback. Full composed
deformation/physics, application resident lifecycle, additional runtime UV
channels, material realization and representative PMX/VMD L3 acceptance remain
open. P1-C, P1-2 and P1-3 remain open.

The renderer's four focused CPU/GPU CTests pass. MMD documentation selftest
and link/anchor/version/diagnostic checks pass.
