# Material policy

> Status: **proposed**; authored from Phase 3. It fixes how a PMX material
> becomes a `UsdShadeMaterial`: the hierarchy, the canonical MMD semantics, and
> the two portable realizations. It follows the shape of `usd-vrm-plugins`'
> material architecture policy so that one renderer can read both families,
> and on material questions it wins over
> [STAGE_CONTRACT.md](STAGE_CONTRACT.md). Section numbers are stable.

---

## 1. Purpose

MMD shading is toon shading with a sphere map, a toon ramp and an outline; no
portable USD shading network reproduces it. So the importer answers two
different questions — *what did the source material say* and *how should this
look in a renderer that has never heard of MMD* — and the failure mode to avoid
is answering the first with the second.

## 2. Core principle

Separate **material semantics** from **render realizations**, and derive every
realization from the semantics — never one realization from another.

```text
PMX material
    ↓
canonical MMD material semantics      (mmdModel)
    ├→ UsdPreviewSurface               generic fallback          /preview
    ├→ MaterialX gltf_pbr              portable approximation     /mtlx
    └→ MMD toon realization            hydra-toon, elsewhere      (not authored here)
```

The cost is that the same source parameter is read by several generators. What
it buys is that deleting a realization destroys no information, and adding one
requires reading nothing but the semantics.

## 3. Hierarchy

```text
/Asset/mtl/<materialId>                  UsdShadeMaterial
    mmd:material:*                       canonical MMD semantics (§4)
    customData: provenance (§4.2)
    outputs:surface       → preview.outputs:surface
    outputs:mtlx:surface  → mtlx.outputs:surface
    MaterialXConfigAPI, config:mtlx:version = "1.39"
    /preview                             UsdShadeNodeGraph
        outputs:surface → surface.outputs:surface
        /surface                         UsdPreviewSurface
        /stReader                        UsdPrimvarReader_float2   (textured only)
        /baseTexture                     UsdUVTexture              (textured only)
    /mtlx                                UsdShadeNodeGraph
        outputs:surface → surface.outputs:surface
        /surface                         ND_gltf_pbr_surfaceshader
        …                                texture nodes (textured only)
```

> **Material** = identity, binding, and canonical semantics.
> **NodeGraph** = one rendering realization.
> **Shader** = an implementation node inside that realization.

- One `UsdShadeMaterial` per PMX material, in material-table order, named by
  its stable identifier.
- Bindings target `/Asset/mtl/<materialId>` and nothing below it.
- `preview` and `mtlx` are part of the contract (frozen,
  [DESIGN_POLICY.md §15](DESIGN_POLICY.md#15-decisions-frozen-early)). The
  shader node names *inside* a realization are not: structural tests assert on
  the graph boundary and its outputs, so a graph can be improved without a
  contract bump. Golden baselines still name interior nodes, and regenerate as
  a reviewed path move when they change.
- There is **no** `native` child graph. The implementation policy suggested
  one; the semantics are the material prim's own attributes instead, because a
  child graph reads as a third realization and the material prim is where
  identity and semantics already live
  ([DESIGN_POLICY.md §19](DESIGN_POLICY.md#19-where-this-document-departs-from-the-implementation-policy)).

## 4. Canonical material semantics

### 4.1 Attributes

Every material carries its full MMD semantics, whether or not any realization
uses them. They are plain custom attributes, so an `MmdMaterialAPI` admitted
later can declare the same names without changing the stage.

| Attribute | Type | Source |
| --- | --- | --- |
| `mmd:material:diffuseColor` | `color4f` | diffuse RGBA |
| `mmd:material:specularColor` | `color3f` | specular |
| `mmd:material:specularPower` | `float` | specular power |
| `mmd:material:ambientColor` | `color3f` | ambient |
| `mmd:material:doubleSided` | `bool` | flag `0x01` |
| `mmd:material:groundShadow` | `bool` | flag `0x02` |
| `mmd:material:castSelfShadow` | `bool` | flag `0x04` |
| `mmd:material:receiveSelfShadow` | `bool` | flag `0x08` |
| `mmd:material:drawEdge` | `bool` | flag `0x10` |
| `mmd:material:vertexColor` | `bool` | flag `0x20` (2.1) |
| `mmd:material:drawPoints` | `bool` | flag `0x40` (2.1) |
| `mmd:material:drawLines` | `bool` | flag `0x80` (2.1) |
| `mmd:material:edgeColor` | `color4f` | edge color |
| `mmd:material:edgeSize` | `float` | edge size |
| `mmd:material:texture` | `asset` | base texture (absent when none) |
| `mmd:material:sphereTexture` | `asset` | sphere texture (absent when none) |
| `mmd:material:sphereMode` | `token` | `disabled`, `multiply`, `add`, `subTexture` |
| `mmd:material:toonSource` | `token` | `none`, `individual`, `shared` (§7) |
| `mmd:material:toonTexture` | `asset` | individual toon texture (only when `individual`) |
| `mmd:material:sharedToonIndex` | `int` | shared toon slot 0–9 (only when `shared`) |

Colors are authored as stored: MMD specifies them without a declared color
space, and the importer does not reinterpret them.

### 4.2 Provenance

`customData` on the material: `mmd:sourceName`, `mmd:sourceEnglishName`,
`mmd:sourceIndex` (which is also MMD's draw order, §10), `mmd:sourceMemo`, and
the verbatim decoded texture strings `mmd:sourceTexturePath`,
`mmd:sourceSphereTexturePath` and `mmd:sourceToonTexturePath` — kept even when
the path is unsafe or does not resolve
([TEXT_ENCODING_POLICY.md §7](TEXT_ENCODING_POLICY.md#7-texture-paths)).

## 5. UsdPreviewSurface realization

The broad compatibility fallback, and the easiest one to debug. It does **not**
claim to reproduce MMD shading.

| `UsdPreviewSurface` input | From |
| --- | --- |
| `diffuseColor` | base texture RGB × diffuse RGB (factor folded into `UsdUVTexture.scale`), or diffuse RGB when untextured |
| `opacity` | base texture alpha × diffuse alpha, or diffuse alpha |
| `useSpecularWorkflow` | `0` |
| `metallic` | `0` |
| `roughness` | derived from specular power (MAT-O1) |

- The texture reader uses `primvars:st`, `wrapS`/`wrapT = repeat`, and
  `sourceColorSpace = "sRGB"` — authored explicitly, not left to a renderer's
  guess.
- **Ambient is not mapped.** MMD adds ambient as a constant term; as emission
  it would wash a model out under scene lights.
- Sphere, toon and edge are omitted: `UsdPreviewSurface` has nothing that
  holds them.
- Double-sidedness is the mesh's, not the material's
  ([STAGE_CONTRACT.md §8.5](STAGE_CONTRACT.md#85-double-sidedness)).

Proposed roughness mapping (MAT-O1): treat specular power *n* as a Blinn-Phong
exponent, convert with the Beckmann equivalence `α = sqrt(2 / (n + 2))`, and
author `roughness = sqrt(α)`, since `UsdPreviewSurface` squares its roughness.
Clamped to `[0.05, 1]`. It is monotonic, documented, and fixture-tested; it is
not claimed to match MMD's highlight.

## 6. MaterialX `gltf_pbr` realization

The preferred portable approximation — the shading model already chosen
across the avatar family, well supported, and renderer-independent.

| `gltf_pbr` input | From |
| --- | --- |
| `base_color` | base texture RGB × diffuse RGB |
| `alpha` | base texture alpha × diffuse alpha |
| `alpha_mode` | per MAT-O2 |
| `metallic` | `0` |
| `roughness` | the §5 mapping |

It is a closest useful approximation, not an emulation of MMD's lighting.
Sphere, toon and edge are omitted here too. The graph is authored for every
material, with `config:mtlx:version = "1.39"` on the material, as in
`usd-vrm-plugins`.

## 7. Toon ramps

A PMX material names its toon ramp in one of two ways: a texture index, or a
shared slot 0–9 that MMD resolves to `toon01.bmp`–`toon10.bmp` in its own data
folder. The canonical model has **one** semantic — `ToonRamp` — with the
variant as data, so a consumer never has to know which form the source used:

| `toonSource` | Authored | Meaning |
| --- | --- | --- |
| `none` | nothing else | no toon ramp |
| `individual` | `mmd:material:toonTexture` | a texture resolved like any other (§4.2) |
| `shared` | `mmd:material:sharedToonIndex` | MMD's shared ramp *N*; the consumer supplies the image |

The shared ramps belong to MMD, not to the model, and this project does not
redistribute them. So no asset path is invented for a shared slot: an asset
path that cannot resolve is worse than an explicit index. A toon-aware
renderer maps the index to its own ramp set.

An individual toon texture whose file is missing is not silently turned into a
shared slot, even where MMD's own behavior would fall back to one: the path is
preserved and the validator reports it as unresolved (MAT-O3).

## 8. Sphere textures

The mode is preserved explicitly as `disabled`, `multiply`, `add` or
`subTexture`. `subTexture` samples the sphere texture as an ordinary texture
through additional UV 1; that channel is `primvars:mmd:uv1`, authored raw
([STAGE_CONTRACT.md §8.3](STAGE_CONTRACT.md#83-uvs)). Both portable
realizations omit sphere mapping; an MMD-aware renderer has everything it needs
from §4.

## 9. Edges

The per-material flag, color and size (§4) and the per-vertex edge scale
(`primvars:mmd:edgeScale`) are preserved. Neither portable realization draws an
outline.

## 10. Alpha and draw order

MMD draws materials in material-table order with alpha blending, and models
are authored relying on that order. The order is preserved as
`mmd:sourceIndex`; a renderer that wants MMD's result draws in that order. The
portable realizations make the alpha decision in MAT-O2 and accept that a
generic renderer's transparency handling will differ from MMD's.

## 11. Material morphs

Material morphs stay declarative modulation on their morph prim
([STAGE_CONTRACT.md §11](STAGE_CONTRACT.md#11-morphs)). The stage never
contains a precomputed material per morph, and a morph never edits the
material prims.

## 12. Rendering belongs elsewhere

`usd-mmd-plugins` contains no MMD renderer. A toon renderer (`hydra-toon`)
consumes this contract; it never parses PMX, and the same renderer can serve
MToon from `usd-vrm-plugins` through its own semantic reading. The portable
`gltf_pbr` path stays the default realization; toon rendering is an additional
path, not a replacement.

A known gap for that consumer: custom attributes on a material prim do not
reach a Hydra render delegate by themselves — `UsdImaging` passes the material
*network*, not arbitrary prim properties. `hydra-toon` will need either an
admitted `MmdMaterialAPI` with an imaging adapter that exposes it, or a third
realization graph for its own render context. That choice is deferred to when
`hydra-toon` exists to be a real consumer (MAT-O4), and either answer keeps §4
unchanged.

## 13. Open questions

| Id | Question | Proposed answer | Resolve by |
| --- | --- | --- | --- |
| MAT-O1 | Roughness from specular power | §5 | Phase 3 |
| MAT-O2 | Alpha mode without decoding images: the importer never reads texture pixels, so it cannot know whether a texture has alpha | blend when the material is textured or diffuse alpha < 1; opaque otherwise | Phase 3 |
| MAT-O3 | Missing individual toon texture | preserve, report unresolved, no fallback | Phase 3 |
| MAT-O4 | How `hydra-toon` reads MMD semantics | `MmdMaterialAPI` + imaging adapter, or a toon realization graph | when `hydra-toon` consumes the stage |
