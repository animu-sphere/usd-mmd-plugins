# Phase 8 explicit Toon primary UV consumer (2026-10-26)

## Scope and environment

Follows [live primary UV transport](2026-10-25-phase8-live-primary-uv.md).
MMD base `ee78dad`, runtime base `7da1356` plus consumer checks, and renderer
base `e54022f` plus explicit UV bindings. Windows x64, MSVC 19.51 Release,
Python 3.13.14, OpenUSD 26.08 and the existing installed MMD, motion and
physics/Jolt packages. Renderer GPU checks use NVIDIA RTX A5000, driver 597.16,
Vulkan, 96 by 96 pixels and the default 4x MSAA. No third-party assets,
importer/evaluator algorithm changes, dependency pins or stage-contract changes.

## Ownership and transport

The renderer-owned `AvatarUvBinding` explicitly maps each resident vertex to
an ABI-5 `ArVertexUv` slot. Dense source identities may be reordered, and split
resident vertices may share a source. Complete coverage, finite float narrowing,
source identity and resident topology/baseline compatibility are validated
before publication. Coordinates are absolute `st`; no group expansion, V flip,
base accumulation or physics evaluation occurs in the renderer.

Only changed UV arrays advance their rendering revisions; identical updates
reuse arrays. Reset generations retain the binding, and Release restores the
unoverridden scene stream. Failures leave output and adapter history intact.
UV changes require ordinary extraction and geometry-stream upload. The late
pose/material path still refuses UV changes. Meshes with resident tangents
refuse pending coherent tangent regeneration. Automatic Hydra target matching
reports UV layouts as unmatched until its resident description retains source
vertex mapping. The public experimental C++ binding layout changes: rebuild
the renderer library and consumers together with matching runtime C headers.

## Evidence

- Renderer no-Vulkan configuration: all 17 CTests pass, including installed
  component/core consumers. Focused Vulkan CPU/GPU tests pass. Explicit generated
  MMD/MToon textures change with weights `0 -> 1.5 -> -0.5 -> 0`, match an
  independently updated scene's colour/depth exactly and restore the original
  pixels at zero; validation-message counts remain zero.
- Renderer CPU checks cover reordered slots/split vertices, missing/overlapping
  or sparse bindings, source-mesh mismatch, topology/baseline changes, resident
  tangent refusal, nonfinite/overflow values, duplicate array/revision reuse,
  retained output, reset generations and Release.
- Runtime fresh installed-Toon configuration: all nine CTests pass, including
  ordinary installed MMD lookup without a renderer dependency and MMD/Jolt
  consumers. The UV owner fixture resolves nested group/direct weight 1.875,
  including primary-V conversion, through runtime snapshots into explicit
  renderer UV bindings for two instances and three reset generations.
  Pose, scalar morph and visibility checks run alongside UV comparisons.
  Duplicate arrays/revisions and old/current snapshots remain valid after
  producer destruction. The configure-time probe now requires UV binding
  fields as well as the current ABI.

The rebuilt Hydra skinning check passes, including explicit unmatched-UV
diagnostics. MMD documentation selftest and all relative links/anchors,
version mirrors and the 77-code diagnostic catalog pass. Renderer documentation
and its checker tests pass, and changed runtime Markdown links/anchors pass.

The renderer's [report 102](https://github.com/animu-sphere/hydra-toon/blob/main/docs/reports/renderer/102-2026-10-26-avatar-primary-uv.md)
owns renderer implementation details. Runtime's
[output contract](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/architecture/OUTPUT_PATHS.md#scoped-mmd-transport-check)
owns the composed consumer boundary.

## Reproduction

Prepare the installed MMD/physics and generated PMX/VMD inputs as in the
[earlier Toon consumer report](2026-10-21-phase8-toon-consumer.md#reproduction).
From the renderer checkout, build current ABI-5 sources and install to a local
prefix (the full build supplies the installed-consumer test's required targets):

```powershell
cmake -S . -B build/avatar-state-no-vulkan -A x64 `
  -DTOON_ENABLE_AVATAR_STATE=ON -DTOON_ENABLE_VULKAN=OFF `
  -DTOON_AVATAR_RUNTIME_INCLUDE_DIR=<runtime-checkout>/include
cmake --build build/avatar-state-no-vulkan --config Release -- /m:1 /nr:false
ctest --test-dir build/avatar-state-no-vulkan -C Release --output-on-failure
cmake --install build/avatar-state-no-vulkan --config Release --prefix <toon-prefix>
```

Use the earlier report's runtime configure recipe with `build/mmd-toon-uv`,
the new Toon prefix and current runtime headers. With the same dependency
DLL/plugin environment:

```powershell
cmake --build build/mmd-toon-uv --config Release -- /m:1 /nr:false
ctest --test-dir build/mmd-toon-uv -C Release --output-on-failure
```

Renderer GPU reproduction is in report 102. These are two distinct evidence
slices: composed MMD owner-to-renderer CPU transport, and renderer-only generated
textured GPU response. They do not establish one MMD-to-GPU playback path,
automatic resident UV mapping, tangent regeneration, additional UV/material
realization, Hydra/direct agreement or representative PMX/VMD L3 acceptance.
P1-C, P1-2 and P1-3 remain open.
