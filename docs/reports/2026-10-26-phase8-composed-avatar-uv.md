# Phase 8 generated composed avatar deformation and UV (2026-10-26)

## Scope

Continues [UV-only composition](2026-10-26-phase8-composed-resident-uv.md)
from MMD base `22219c6`, runtime checkout `a76aaf4` and renderer base `19e1a3d`
plus this change. Implementation belongs to the renderer's opt-in test host;
no MMD importer, evaluator, runtime API or dependency pin changed.

The renderer's
[report 106](https://github.com/animu-sphere/hydra-toon/blob/main/docs/reports/renderer/106-2026-10-26-mmd-avatar-uv.md)
owns implementation, environment and reproduction commands. A generated
two-joint canonical model and bound motion run installed owner pre/post
continuations, with Jolt stepped only at the runtime world barrier. A keyed
root and dynamic child publish final joint feedback, a sparse vertex morph
and primary UVs in the same retained snapshot. Automatic resident bindings
match a reversed skin palette and reordered UV slots; tangent seams retain
skin, morph and auxiliary streams.

## Evidence and acceptance boundary

Independent keys, expected group weights/UV arrays and direct completed solver
readback check owner/runtime values. Authored SkelAnimation and `st` provide
the USD/Hydra oracle. Geometry, skin/morph/UV/tangent and auxiliary streams
agree; palettes agree within `2e-6`. Generated textured MMD and normal-mapped
MToon Vulkan colour/depth agree exactly at 128 by 128, 1x/4x MSAA. Skin and
vertex-morph removal controls change pixels. Three world reset generations
reproduce completed solver transforms and pixels. Duplicate arrays are reused,
retained snapshots survive producer destruction and Release restores immutable
pose, morph, UV and tangent bases.

This extends generated composition to simultaneous deformation, physics and
UV publication. It does not read PMX/VMD, validate representative contacts or
constraints, bind application participants, realize material morphs or publish
additional runtime UV channels. Full-asset playback, resident application
lifecycle and representative L3 acceptance remain open; P1-C, P1-2 and P1-3
remain open.

The renderer's seven focused CPU/GPU CTests pass. MMD documentation selftest
and relative-link, anchor, version and diagnostic checks pass.
