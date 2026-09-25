# Phase 8 importer on stage-contract v2 (2026-09-25)

Dated evidence from real runs; append-only
([contributing/documentation.md](../contributing/documentation.md)).

## Question

[MATERIAL_POLICY.md §14](../design/MATERIAL_POLICY.md#14-migration-and-implementation-order)
steps 3 and 4 move the importer to stage-contract v2. Every material applies
`MmdMaterialAPI`, the canonical values become `inputs:mmd:material:*`, and
`/preview` and `/mtlx` connect to them instead of copying them. The policy
says both graphs keep their static appearance. Does a v2 stage draw as the v1
stage of the same model did? Does a runtime override of the canonical
diffuse reach the realizations, as
[§11](../design/MATERIAL_POLICY.md#11-material-morphs) needs? The untextured
`/preview` was still open (MAT-O6).

## What was run

- **v1 stages** came from the released v0.1.0 product
  (`usdMmdFileFormat` alone on the plugin path). **v2 stages** came from this
  change's build (`usdMmdFileFormat` and `mmdSchema`). Each was flattened with
  its asset paths anchored.
- **Contexts.** Storm prefers `/mtlx` when it has MaterialX, as OpenUSD 26.08
  here does. `/preview` was drawn from an over layer that blocks each
  material's `outputs:mtlx:surface` connection.
- **Renders.** `usdrecord` drew each stage in Storm at 384 px wide, and every
  image was compared pixel by pixel with its counterpart.
- **Override.** A stronger layer time-sampled `inputs:mmd:material:diffuseColor`:
  frame 1 the imported value, frame 2 red `(1, 0.1, 0.1)` with the imported
  alpha. One layer did this for the textured materials only, and another for
  the untextured ones.
- **Models.** The 15 local models (m01–m15, 237 materials). Only one of their
  materials is untextured. So the untextured case was also drawn from the
  fixture whose texture paths are all refused, `unsafe-texture-paths.pmx`, with
  its collinear fixture points moved apart and staggered in depth so that
  its triangles cover pixels.
- **Noise floor.** The same v2 stage was drawn twice (static, and frame 1 of an
  override layer that leaves the values as they are).

## Findings

**A connected texture loses its colour space.** The first v2 build drew
`/mtlx` visibly lighter. On m01 the mean RGB went from (209, 191, 187) to
(232, 222, 220), and every covered pixel changed. `/preview` did not change. v1
had put `colorSpace = "srgb_texture"` on the MaterialX image node's `file`.
UsdImaging reads the colour space from the attribute that *produces* the value
(`materialParamUtils.cpp`, `GetValueProducingAttributes`). Once `file` is
connected, that attribute is the Material's `inputs:mmd:material:texture`,
and its metadata is the one that counts. The shader input's metadata is
ignored. Without it, Storm's MaterialX path takes the image as raw.
`/preview` was unaffected because `UsdUVTexture` keeps an explicit
`sourceColorSpace = "sRGB"`.

On m01's canonical texture inputs, each colour-space name gave this result:

| `colorSpace` | `/mtlx` against v1 | `/preview` against v1 |
| --- | --- | --- |
| `srgb_rec709_scene` (OpenUSD's `GfColorSpaceNames->SRGBRec709`) | identical | identical |
| `srgb_texture` (MaterialX's name) | identical | identical |
| `sRGB` | lighter, as with none | identical |

The importer now authors `srgb_rec709_scene` on every safe texture slot. The
shader-side metadata, dead once connected, is no longer written.

**With that, the textured models are unchanged.** Over the 15 models, v1 and v2
are pixel-identical in both contexts. There are three exceptions, and each is
render noise. The same v2 stage drawn twice differs by the same amount:

| Model | Context | v1 → v2 (max, share of pixels) | v2 → v2 again |
| --- | --- | --- | --- |
| m05 | `/preview` | 30, < 0.01 % | 30, < 0.01 % |
| m09 | both | ≤ 4, < 0.01 % | ≤ 5, < 0.01 % |
| m14 | `/preview` | 13, 0.09 % | 13, 0.09 % |

m07 draws no texture in either version: black in `/mtlx` and grey in
`/preview`. This does not change with the version. Storm does not load its
images, which is outside this change.

**A runtime diffuse reaches both realizations of a textured material.** In
every model with a texture that draws, frame 2 tints what the textured
materials cover in both contexts. For example, m01 went from (209, 191, 187)
to (209, 65, 63) in `/mtlx`, and to (213, 73, 71) in `/preview`, on the same
14 % of pixels. Frame 1 equals the static render, so the connection adds
nothing while the value is unchanged.

**An untextured material: `/mtlx` follows, `/preview` copies (MAT-O6).**

| Untextured case | `/mtlx` | `/preview` |
| --- | --- | --- |
| override frame 1 → frame 2 | mean (231, 231, 231) → (255, 89, 89) | unchanged |
| v1 → v2, static | edges differ, 0.5 % of pixels | identical |

The `/mtlx` edges differ because Storm's rule for `gltf_pbr` changes the
draw pass (`materialXFilter.cpp`, `_GetGlTFSurfaceMaterialTag`). With
`alpha_mode = 2`, Storm keeps a constant `alpha` of 1 in the opaque pass, but
draws a *connected* `alpha` as translucent. The untextured v1 graph wrote
alpha 1. The v2 graph connects it, and so the material now draws in the
translucent pass. Its silhouette then has no partial coverage in alpha. The
v1 image has alpha 64, 128 and 191 along the edges, and the v2 image only 0
and 255. Only one kind of material is affected. MAT-O2 gives it `alpha_mode
= 2`, because it names a base texture or its diffuse alpha is below 1, but it
has no drawable texture and its alpha is 1: in practice, a material whose
texture path was refused. A textured material had a connected alpha in v1
already. The translucent pass is also what lets a runtime alpha below 1
blend on such a material, which v1's opaque pass would not.

## Decision

- The importer authors stage-contract v2. Every material applies
  `MmdMaterialAPI` and authors the §4.1 values through the generated
  accessors. `/mtlx` connects diffuse for every material, and the texture
  when the material has one. `/preview` connects diffuse and the texture
  when the material is textured
  ([MATERIAL_POLICY.md §5, §6](../design/MATERIAL_POLICY.md#5-usdpreviewsurface-realization)).
- **A texture slot states its encoding.** A safe slot authors `colorSpace =
  "srgb_rec709_scene"` on the canonical input, because a connected shader
  reads the colour space from there
  ([§4.1](../design/MATERIAL_POLICY.md#41-attributes)).
- **MAT-O6 is resolved.** The untextured `/preview` keeps a static copy of
  diffuse. Neither measured alternative is sound, and the realization it would
  serve is the fallback of a renderer without MaterialX. A runtime diffuse
  reaches `/mtlx` and the MMD-aware path
  ([§13](../design/MATERIAL_POLICY.md#13-open-questions)).
- **The schema bundle is a runtime dependency of the importer.** Its
  `plugInfo.json` names `UsdMmdMaterialAPI` as a plugin dependency. On Windows
  OpenUSD opens a plugin with a plain `LoadLibraryW`, which does not search the
  plugin's own directory. So without the dependency, the importer did not
  load unless the schema's `lib/` was on `PATH`, even when both libraries
  shared one `lib/`. With it, Plug loads the schema's library first, by the
  path its own registration gives. A host that leaves the schema out gets
  `Load failed: unknown dependent class 'UsdMmdMaterialAPI'`, not a stage
  without it
  ([PACKAGE_CONTRACT.md](../architecture/PACKAGE_CONTRACT.md#usdmmdfileformat)).
