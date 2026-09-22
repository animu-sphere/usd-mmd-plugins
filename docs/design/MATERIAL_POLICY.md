# Material policy

> Status: **binding for Phase 3 and for the planned material-schema migration**.
> Phase 3 authors one `UsdShadeMaterial` per PMX material, the
> `mmd:material:*` attributes in §4.1, provenance in §4.2, and the two portable
> realizations. Stage-contract v1 authors those attributes as schema-less custom
> attributes. `MmdMaterialAPI` passed the schema admission test on 2026-09-22;
> Phase 8 applies it while retaining the same property names, types and meanings
> as a backward-compatible stage-contract v1 addition (§4.3, §14). This
> document fixes the hierarchy, canonical MMD semantics, portable realizations
> and renderer boundary. It follows the
> shape of `usd-vrm-plugins`' material architecture without treating MMD as
> MToon, and on material questions it wins over
> [STAGE_CONTRACT.md](STAGE_CONTRACT.md). Existing section numbers are stable.

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
    ↓
MmdMaterialAPI                        formal USD contract (Phase 8 target)
    ├→ UsdPreviewSurface               unlit-compatible fallback /preview
    ├→ MaterialX gltf_pbr              unlit-compatible portable path /mtlx
    └→ UsdImaging adapter              hydra-toon, elsewhere      (not authored here)
```

The cost is that the same source parameter is read by several generators. What
it buys is that deleting a realization destroys no information, and adding one
requires reading nothing but the semantics.

## 3. Hierarchy

```text
/Asset/mtl/<materialId>                  UsdShadeMaterial
    applied API: MmdMaterialAPI           (Phase 8 target)
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
- Applying `MmdMaterialAPI` does not create a new child or realization. It
  declares the existing canonical properties and identifies the prim as an MMD
  material. Bindings and render-context outputs continue to target the
  `UsdShadeMaterial`.

## 4. Canonical material semantics

### 4.1 Attributes

Every material carries its full MMD semantics, whether or not any realization
uses them. The shipped stage-contract v1 implementation authors them as plain
custom attributes. Phase 8 declares the same names through `MmdMaterialAPI`; no
consumer has to translate an old property name to a new one.

| Attribute | Type | PMX source | Authoring |
| --- | --- | --- | --- |
| `mmd:material:diffuseColor` | `color4f` | diffuse RGBA | required |
| `mmd:material:specularColor` | `color3f` | specular | required |
| `mmd:material:specularPower` | `float` | specular power | required |
| `mmd:material:ambientColor` | `color3f` | ambient | required |
| `mmd:material:doubleSided` | `uniform bool` | flag `0x01` | required |
| `mmd:material:groundShadow` | `bool` | flag `0x02` | required |
| `mmd:material:castSelfShadow` | `bool` | flag `0x04` | required |
| `mmd:material:receiveSelfShadow` | `bool` | flag `0x08` | required |
| `mmd:material:drawEdge` | `bool` | flag `0x10` | required |
| `mmd:material:vertexColor` | `bool` | flag `0x20` (2.1 meaning) | required; preserved in 2.0 without assigning 2.1 meaning |
| `mmd:material:drawPoints` | `bool` | flag `0x40` (2.1 meaning) | required; preserved in 2.0 without assigning 2.1 meaning |
| `mmd:material:drawLines` | `bool` | flag `0x80` (2.1 meaning) | required; preserved in 2.0 without assigning 2.1 meaning |
| `mmd:material:edgeColor` | `color4f` | edge color | required even when `drawEdge = false` |
| `mmd:material:edgeSize` | `float` | edge size | required even when `drawEdge = false` |
| `mmd:material:texture` | `asset` | base texture | only when the slot names a safe asset path |
| `mmd:material:sphereTexture` | `asset` | sphere texture | only when the slot names a safe asset path |
| `mmd:material:sphereMode` | `token` | `disabled`, `multiply`, `add`, `subTexture` | required |
| `mmd:material:toonSource` | `token` | `none`, `individual`, `shared` (§7) | required |
| `mmd:material:toonTexture` | `asset` | individual toon texture | only when `toonSource = individual` and the path is safe |
| `mmd:material:sharedToonIndex` | `int` | shared toon slot 0–9 | only when `toonSource = shared` |

Colors are authored as stored: MMD specifies them without a declared color
space, and the importer does not reinterpret them.

### 4.2 Provenance

`customData` on the material: `mmd:sourceName`, `mmd:sourceEnglishName`,
`mmd:sourceIndex` (which is also MMD's draw order, §10), `mmd:sourceMemo`, and
the verbatim decoded texture strings `mmd:sourceTexturePath`,
`mmd:sourceSphereTexturePath` and `mmd:sourceToonTexturePath` — kept even when
the path is unsafe or does not resolve
([TEXT_ENCODING_POLICY.md §7](TEXT_ENCODING_POLICY.md#7-texture-paths)).

### 4.3 `MmdMaterialAPI`

`MmdMaterialAPI` is a **single-apply API schema** on `UsdShadeMaterial`. It is
the formal USD contract for §4.1, not a renderer implementation and not a copy
of the PMX record layout. It passed
[DESIGN_POLICY.md §6](DESIGN_POLICY.md#6-the-schema-admission-test) because an
MMD-aware renderer needs to discover and read these values from a composed USD
stage without PMX access.

The generated API exposes every §4.1 property, tokens for `sphereMode` and
`toonSource`, and schema fallbacks that are neutral when a property is absent:

| Property family | Schema fallback |
| --- | --- |
| diffuse | `(1, 1, 1, 1)` |
| specular, ambient | `(0, 0, 0)` |
| specular power | `0` |
| drawing and shadow flags | `false` |
| edge color, edge size | `(0, 0, 0, 0)`, `0` |
| asset-valued texture slots | empty asset path |
| sphere mode | `disabled` |
| toon source, shared toon index | `none`, `-1` |

The importer still authors every scalar and flag that exists in a PMX material;
fallbacks define robust reads, not permission to discard source values. Asset
slots remain conditional as §4.1 states. Provenance stays in `customData` and
is deliberately outside the API.

Consumers use generated schema accessors rather than treating attribute-name
strings as their public interface. During migration they must also accept a
stage-contract v1 material with the same schema-less properties (§14).

## 5. UsdPreviewSurface realization

The broad compatibility fallback, and the easiest one to debug. To keep the
usdview result stable across scene lighting, it follows the VRM unlit pattern:
the surface's lit response is disabled and the source color is carried through
`emissiveColor`. This is a display realization only; the complete MMD values
remain on `mmd:material:*` attributes.

| `UsdPreviewSurface` input | From |
| --- | --- |
| `diffuseColor` | black; the portable display path uses `emissiveColor` |
| `emissiveColor` | base texture RGB × diffuse RGB (factor folded into `UsdUVTexture.scale`), or diffuse RGB when untextured |
| `opacity` | base texture alpha × diffuse alpha, or diffuse alpha |
| `useSpecularWorkflow` | `0` |
| `metallic` | `0` |
| `roughness` | `1` |

- The texture reader uses `primvars:st`, `wrapS`/`wrapT = repeat`, and
  `sourceColorSpace = "sRGB"` — authored explicitly, not left to a renderer's
  guess.
- **Ambient, specular, sphere, toon and edge are not mapped into this generic
  realization.** They remain available as MMD semantics for an MMD-aware
  consumer; the unlit display path deliberately avoids scene-light response.
- Sphere, toon and edge are omitted: `UsdPreviewSurface` has nothing that
  holds them.
- Double-sidedness is the mesh's, not the material's
  ([STAGE_CONTRACT.md §8.5](STAGE_CONTRACT.md#85-double-sidedness)).

The canonical `specularPower` remains available for an MMD-aware realization;
this portable path does not convert it because it intentionally has no lit
specular response. A future lit realization must make its own documented
conversion from the canonical value rather than changing this fallback.

## 6. MaterialX `gltf_pbr` realization

The preferred portable approximation — the shading model already chosen
across the avatar family, well supported, and renderer-independent. Its
terminal remains `gltf_pbr`, but it follows the same unlit display policy as
the preview graph so usdview and MaterialX-aware consumers do not introduce a
different lighting model.

| `gltf_pbr` input | From |
| --- | --- |
| `base_color` | black; the display color is carried by `emissive` |
| `emissive` | base texture RGB × diffuse RGB |
| `alpha` | base texture alpha × diffuse alpha |
| `alpha_mode` | per MAT-O2 |
| `metallic` | `0` |
| `roughness` | `1` |
| `specular` | `0` |

It is a closest useful unlit display path, not an emulation of MMD's toon
lighting. Sphere, toon, edge, ambient and specular semantics are not discarded;
they stay on the material prim for an MMD-aware renderer.
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
([STAGE_CONTRACT.md §11.2](STAGE_CONTRACT.md#112-every-other-morph-type)):
from Phase 4 each one authors `mmd:morph:materialIndices` (with `−1` keeping
PMX's "every material"), `mmd:morph:materialOperations`, and one parallel
array per modulated value — diffuse, specular, specular power, ambient, edge
color and size, and the texture, sphere and toon tints. The stage never
contains a precomputed material per morph, and a morph never edits the
material prims.

## 12. Rendering and integration belong elsewhere

`usd-mmd-plugins` defines and authors MMD semantics; it contains no toon
renderer, outline pass, shadow algorithm or GPU shader. `hydra-toon` consumes
the composed USD contract and never parses PMX. The portable `gltf_pbr` path
stays the default realization; MMD-aware rendering is an additional path, not a
replacement.

An applied API schema alone does not make arbitrary prim properties appear in
a Hydra material network. The fixed integration boundary is therefore:

```text
UsdShadeMaterial + MmdMaterialAPI
                ↓
       UsdImaging adapter
                ↓
  Hydra material representation
                ↓
          hydra-toon
```

The adapter exposes the API's semantics through the Hydra data-source or
material-data mechanism appropriate to the OpenUSD version. It does not add a
third `toon` realization graph, and `hydra-toon` does not depend on
`usdMmdFileFormat`, `mmdModel` or `mmdPmx`.

`hydra-toon` may normalize `MmdMaterialAPI` and the independent
`VrmMtoonMaterialAPI` into a renderer-private runtime representation. That is
an implementation convenience, not a claim that MMD and MToon have the same
material model. MMD-only concepts — sphere multiply/add, sub-texture UVs,
shared toon slots, ground/self-shadow flags and vertex edge scale — remain
explicit extensions where they cannot be shared faithfully.

No USD-level `ToonMaterialAPI` is introduced now. A common API may be
reconsidered only after both MMD and VRM paths have been implemented and stable
common semantics are demonstrated. Shared abstractions are extracted from
working concrete adapters, not designed ahead of them.

## 13. Open questions

| Id | Question | Proposed answer | Resolve by |
| --- | --- | --- | --- |
| MAT-O1 | Roughness from specular power | resolved for the current portable realizations: they are unlit and author `roughness = 1`; `specularPower` remains canonical for an MMD-aware realization | Phase 3 |
| MAT-O2 | Alpha mode without decoding images: the importer never reads texture pixels, so it cannot know whether a texture has alpha | resolved: blend when the material names a base texture or diffuse alpha < 1; opaque otherwise | Phase 3 |
| MAT-O3 | Missing individual toon texture | resolved: preserve the source path and provenance, author a safe asset path when available so USD validation can report an unresolved file, and never fall back to a shared ramp | Phase 3 |
| MAT-O4 | How `hydra-toon` reads MMD semantics | resolved: applied `MmdMaterialAPI` + UsdImaging adapter; no toon realization graph | design fixed 2026-09-22; implementation in Phase 8 |

## 14. Migration and implementation order

The policy is implemented without rewriting the already shipped Phase 3
contract:

1. **Inventory — complete in this document.** §4.1 fixes names, types,
   required/conditional authoring and PMX mapping; §4.3 fixes fallbacks.
2. **Schema bundle.** Add `mmdSchema` with the single-apply
   `MmdMaterialAPI`, generated C++/Python accessors and token declarations.
3. **Importer migration.** Apply the API and author through its accessors while
   keeping the §4.1 property values byte-for-byte equivalent. The applied-schema
   metadata is an additive stage-contract v1 change, not a semantic version bump.
4. **Fallback regression.** Keep `/preview` and `/mtlx` graph boundaries and
   appearance unchanged; they remain generic fallbacks, not canonical data.
5. **Hydra bridge.** Implement and test the UsdImaging adapter independently
   of the renderer's GPU representation.
6. **MMD renderer path.** Bring up diffuse/alpha, toon ramp, sphere
   multiply/add, sub-texture, outline, shadow flags, material morph runtime,
   then advanced UV and vertex-color behavior.
7. **Common-runtime review.** Compare the completed MMD and VRM adapters and
   extract only proven renderer-private common code. Revisit a USD-level
   `ToonMaterialAPI` only with evidence from both.

Compatibility is asymmetric and explicit: a schema-aware reader accepts an
earlier contract-v1 material by reading the same names even when
`MmdMaterialAPI` is not applied; an existing reader can ignore `apiSchemas` and
continues to see the same custom properties and fallback graphs. No migration
renames a property or turns PreviewSurface or MaterialX into the source of
truth.
