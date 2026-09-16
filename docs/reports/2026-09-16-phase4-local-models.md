# Phases 3 and 4 against distributed models (2026-09-16)

Dated evidence from real runs; append-only
([contributing/documentation.md](../contributing/documentation.md)).

## What was run

The importer at Phase 4 (Windows 11, MSVC, OpenUSD 26.08) opened three PMX
files held locally — distributed MMD character models, none of them a
fixture, none committed or redistributed, and no image of one kept
([DESIGN_POLICY.md §13](../design/DESIGN_POLICY.md#13-testing-policy)). For
each one, `/Asset/morph` was read back with `Usd.Stage.Open`, the blend
shapes the mesh names were resolved with `UsdSkel.BlendShapeQuery`, every
validator OpenUSD registers was run over the stage, and the model was
rendered with `usdrecord` through Storm — which is Phase 3's outstanding
visual review.

## Morphs

| | Character A | Character B | Character C |
| --- | --- | --- | --- |
| Points, faces | 33,782, 50,446 | 29,511, 36,588 | 53,179, 60,060 |
| Joints | 319 | 444 | 633 |
| Morph prims | 131 | 65 | 70 |
| Vertex morphs | 94 | 65 | 62 |
| Other morphs | 23 group, 11 material, 2 bone, 1 UV | — | 8 material |
| Blend shapes named / resolved | 94 / 94 | 65 / 65 | 62 / 62 |
| Blend-shape offsets | 54,775 | 25,731 | 47,228 |
| Panels (hidden/brow/eye/mouth/other) | 24/24/29/27/27 | 0/24/19/21/1 | 0/22/18/21/9 |
| Opened in | 0.37 s | 0.26 s | 0.44 s |
| `usdchecker` validators | Success | Success | Success |

- **Every morph is authored.** One prim per source morph, in morph-table
  order, and every type these models use appears: character A alone carries
  group, material, bone and UV morphs beside its 94 vertex morphs.
- **Blend shapes resolve.** `UsdSkel.BlendShapeQuery` finds every blend shape
  the mesh names, on all three models — the binding is complete, not just
  authored.
- **Blend shapes deform.** Driving character A's first three blend shapes to
  weight 1 through `UsdSkel`'s own `ComputeDeformedPoints` moved 3,207, 3,147
  and 1,571 points, by at most 33 mm, 27 mm and 27 mm — facial morphs at the
  scale STAGE-O1 sets. The importer authors no weight; a consumer supplies
  them ([STAGE_CONTRACT.md §11](../design/STAGE_CONTRACT.md#11-morphs)).
- **Panels survive.** Every model's morphs land in the panel the source gave
  them, and none needed `MMD_MORPH_UNKNOWN_PANEL`.
- **No new diagnostics.** The three raised only what Phase 2 already raised
  for them — identifier collisions (16, 17 and 31) and, on two, the SDEF
  approximation. No group cycle, no unknown panel, and no morph offset
  dropped.
- **Validation is unchanged.** Adding `/Asset/morph` and the mesh's
  `skel:blendShapes` / `skel:blendShapeTargets` leaves every OpenUSD
  validator passing, `UsdSkel`'s included.

## Portable colors (Phase 3's visual review)

All three rendered through `usdrecord` with Storm, from the skinned rest pose
and with no light in the scene:

- Each model is upright, filling the frame at human scale, and faces the
  camera on +Z — the Z mirror and the winding reversal hold on real geometry
  as they did in [Phase 2](2026-09-15-phase2-local-models.md).
- Base textures appear with their source colors: hair, skin, cloth and eye
  materials are distinguishable, and each model reads as itself rather than
  as the renderer's fallback grey. Emission carries the color, so an unlit
  scene renders it at full value — that is what the `preview` and `mtlx`
  graphs are for ([MATERIAL_POLICY.md §5](../design/MATERIAL_POLICY.md#5-usdpreviewsurface-realization)).
- Alpha behaves: cut-out geometry (hair strands, ribbons, eyelashes) shows no
  opaque card, which is the alpha-mode rule MAT-O2 settled.
- The result is flat, with no toon ramp, no sphere-map highlight and no
  outline. That is the intended portable path, not a defect: MMD-specific
  shading stays declarative on the material prim for an MMD-aware renderer
  ([MATERIAL_POLICY.md §12](../design/MATERIAL_POLICY.md#12-rendering-belongs-elsewhere)).

## What it did not cover

Non-vertex morph semantics were read back as authored, not executed: no group
was expanded, no bone morph applied, no material morph composited. That is the
contract ([STAGE_CONTRACT.md §11.3](../design/STAGE_CONTRACT.md#113-nothing-is-evaluated)),
and a consumer that does any of it is the thing that will test it.
