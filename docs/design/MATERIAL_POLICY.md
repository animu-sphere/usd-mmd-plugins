# Material policy

> Status: **binding for Phase 3 and for the planned material-schema migration**.
> Phase 3 authors one `UsdShadeMaterial` per PMX material, the canonical
> values in §4.1, provenance in §4.2, and the two portable realizations.
> Stage-contract v1 authors those values as schema-less custom attributes named
> `mmd:material:*`. `MmdMaterialAPI` passed the schema admission test on
> 2026-09-22. On 2026-09-25 the canonical values became Material interface
> inputs, `inputs:mmd:material:*`, because UsdShade connects nothing else
> ([report](../reports/2026-09-25-phase8-material-inputs.md)). Phase 8 applies
> the API, authors those inputs, and connects the realizations to them, as
> stage-contract v2 (§4.3, §14). This document fixes the hierarchy, canonical
> MMD semantics, portable realizations and renderer boundary. It follows the
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

From stage-contract v2, "derive" means *connect* wherever the realization can
express the relation. A realization reads a canonical value through a
UsdShade connection from the Material's interface input, not through a copy
written into the graph. A value that changes at runtime, such as a material
morph's result, then reaches whichever realization the renderer selects, and
nothing is written into a graph (§11). Where the realization's node set cannot
express the relation, the graph holds a copy. That copy is documented as
static (§5, MAT-O6).

## 3. Hierarchy

```text
/Asset/mtl/<materialId>                  UsdShadeMaterial
    applied API: MmdMaterialAPI           (Phase 8, stage-contract v2)
    inputs:mmd:material:*                canonical MMD semantics (§4)
                                         (v1: schema-less mmd:material:*)
    customData: provenance (§4.2)
    outputs:surface       → preview.outputs:surface
    outputs:mtlx:surface  → mtlx.outputs:surface
    MaterialXConfigAPI, config:mtlx:version = "1.39"
    /preview                             UsdShadeNodeGraph
        inputs:*  → the Material's inputs:mmd:material:*   (v2)
        outputs:surface → surface.outputs:surface
        /surface                         UsdPreviewSurface
        /stReader                        UsdPrimvarReader_float2   (textured only)
        /baseTexture                     UsdUVTexture              (textured only)
    /mtlx                                UsdShadeNodeGraph
        inputs:*  → the Material's inputs:mmd:material:*   (v2)
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
  declares the canonical properties and identifies the prim as an MMD
  material. Bindings and render-context outputs continue to target the
  `UsdShadeMaterial`.
- A realization graph reaches a canonical value through its own NodeGraph
  interface input, connected to the Material's `inputs:mmd:material:*`
  property. Its interior shaders connect to that graph input, never to the
  Material directly, so the graph boundary stays the unit that tests assert on.

## 4. Canonical material semantics

### 4.1 Attributes

Every material carries its full MMD semantics, whether or not any realization
uses them. Each is a Material interface input, `inputs:mmd:material:<name>`,
declared by `MmdMaterialAPI`. UsdShade connects only `inputs:` and `outputs:`
attributes, so this is the only form a realization can connect to (§2).

| `<name>` | Type | Variability | PMX source | Authoring |
| --- | --- | --- | --- | --- |
| `diffuseColor` | `color4f` | varying | diffuse RGBA | required |
| `specularColor` | `color3f` | varying | specular | required |
| `specularPower` | `float` | varying | specular power | required |
| `ambientColor` | `color3f` | varying | ambient | required |
| `doubleSided` | `bool` | uniform | flag `0x01` | required |
| `groundShadow` | `bool` | uniform | flag `0x02` | required |
| `castSelfShadow` | `bool` | uniform | flag `0x04` | required |
| `receiveSelfShadow` | `bool` | uniform | flag `0x08` | required |
| `drawEdge` | `bool` | uniform | flag `0x10` | required |
| `vertexColor` | `bool` | uniform | flag `0x20` (2.1 meaning) | required; preserved in 2.0 without assigning 2.1 meaning |
| `drawPoints` | `bool` | uniform | flag `0x40` (2.1 meaning) | required; preserved in 2.0 without assigning 2.1 meaning |
| `drawLines` | `bool` | uniform | flag `0x80` (2.1 meaning) | required; preserved in 2.0 without assigning 2.1 meaning |
| `edgeColor` | `color4f` | varying | edge color | required even when `drawEdge = false` |
| `edgeSize` | `float` | varying | edge size | required even when `drawEdge = false` |
| `texture` | `asset` | varying | base texture | only when the slot names a safe asset path |
| `sphereTexture` | `asset` | varying | sphere texture | only when the slot names a safe asset path |
| `sphereMode` | `token` | uniform | `disabled`, `multiply`, `add`, `subTexture` | required |
| `toonSource` | `token` | uniform | `none`, `individual`, `shared` (§7) | required |
| `toonTexture` | `asset` | varying | individual toon texture | only when `toonSource = individual` and the path is safe |
| `sharedToonIndex` | `int` | uniform | shared toon slot 0–9 | only when `toonSource = shared` |

The values a material morph modulates (§11) are varying, so a runtime can
override or time-sample them and every connected realization follows. The
texture slots are varying too, as UsdShade inputs normally are. The drawing
flags, the two modes and the shared toon slot stay uniform: a material morph
never changes them.

RGBA colours stay one `color4f`. `usd-vrm-plugins` splits glTF's RGBA factor
into a colour and an alpha. Here the common MMD case, a base texture times
diffuse, folds the whole RGBA factor into `UsdUVTexture.scale`, a `float4` of
the same value type, and so connects without a conversion node
([report](../reports/2026-09-25-phase8-material-inputs.md)).

**Stage-contract v1** authored the same values under the same `<name>`s as
schema-less custom attributes, `mmd:material:<name>`, with every one uniform
except the three texture slots. A realization cannot connect to those names:
UsdShade ignores the connection without an error, and the graph draws its
shader's default instead. §14 says how a reader handles both versions.

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

A connected realization never sees a schema fallback. UsdShade resolves an
interface connection to an *authored* value only, so a graph connected to an
unauthored canonical input takes its own shader's default. That is one more
reason the importer authors every value a graph connects to. The fallbacks are
for readers, not for realizations.

Consumers use generated schema accessors rather than treating attribute-name
strings as their public interface. The accessors are C++ (`UsdMmdMaterialAPI`,
`UsdMmdTokens`), as `usd-vrm-plugins`' `vrmSchema` generates them. Python
reads the same schema through the registry: the prim definition, the
fallbacks and `prim.ApplyAPI("MmdMaterialAPI")` need no compiled module. A
Python binding is built only when a Python consumer needs typed classes. A
consumer that must also read stage-contract v1 follows §14.

## 5. UsdPreviewSurface realization

The broad compatibility fallback, and the easiest one to debug. To keep the
usdview result stable across scene lighting, it follows the VRM unlit pattern:
the surface's lit response is disabled and the source color is carried through
`emissiveColor`. This is a display realization only; the complete MMD values
remain on the Material's canonical inputs (§4.1).

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
- From stage-contract v2 the graph connects rather than copies (§2, §3). On a
  textured material, `UsdUVTexture.scale` connects to the graph input that
  carries `diffuseColor`, a `float4` from a `color4f`, and `file` connects to
  the input that carries `texture`. Storm follows a time-sampled diffuse
  through that connection frame by frame
  ([report](../reports/2026-09-25-phase8-material-inputs.md)).
- An untextured material has no connection this node set can express:
  `UsdPreviewSurface` cannot split a `color4f` into the `color3f` for
  `emissiveColor` and the `float` for `opacity`. Until MAT-O6 is answered, the
  untextured graph holds a static copy of diffuse RGB and alpha, as
  stage-contract v1 does.

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

From stage-contract v2 this graph connects every value it reads: the diffuse
factor into `ND_multiply_color4`, or into `ND_separate4_color4` when the
material is untextured, and the texture slot into the image node's `file`.
MaterialX has the nodes the preview graph lacks, so no copy remains.
`alpha_mode` is still decided once at import (MAT-O2). A runtime that animates
diffuse alpha below 1 on an opaque material does not make it blend.

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

A runtime evaluates the morphs; the importer does not. The runtime applies the
result as an override of the Material's varying canonical inputs (§4.1): a
stronger layer's opinion or time samples, never an edit of the imported layer.
From stage-contract v2 the connected realizations follow it without being
regenerated (§2). An MMD-aware renderer reads the same inputs through the
adapter (§12). The texture, sphere and toon tints have no canonical input:
they modulate a texture's contribution, which only an MMD-aware renderer
draws.

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
| MAT-O5 | How a changed canonical value reaches a realization | resolved 2026-09-25: the canonical values are Material interface inputs, `inputs:mmd:material:*`, and the realizations connect to them; RGBA stays `color4f` ([report](../reports/2026-09-25-phase8-material-inputs.md)) | stage-contract v2 |
| MAT-O6 | An untextured `/preview`: `UsdPreviewSurface` has no node that splits the `color4f` diffuse into `emissiveColor` and `opacity` | open. A direct `color4f` → `color3f` connection works in Storm but is not a UsdShade promise and gives no alpha; a fileless `UsdUVTexture` whose `fallback` is connected follows both, but Storm warns every frame. Until answered, the untextured graph copies the values | the importer migration (§14 step 3) |

## 14. Migration and implementation order

The policy is implemented in the following order:

1. **Inventory — complete in this document.** §4.1 fixes names, types,
   variability, required/conditional authoring and PMX mapping; §4.3 fixes
   fallbacks.
2. **Schema bundle.** Add `mmdSchema` with the single-apply
   `MmdMaterialAPI`, generated C++ accessors and token declarations (§4.3).
3. **Importer migration — stage-contract v2.** Apply the API and author the
   canonical inputs through its accessors, with the same values stage-contract
   v1 authored under `mmd:material:*`. Stamp version 2
   ([STAGE_CONTRACT.md §2](STAGE_CONTRACT.md#2-contract-version)), and author
   no v1 name beside the v2 one: two authoritative copies of a value are what
   §2 forbids.
4. **Realizations connect.** `/preview` and `/mtlx` read the canonical inputs
   through their graph interface inputs (§5, §6). Their boundaries and their
   static appearance stay as they are; the golden baselines change by the
   property renames and the new connections, reviewed as such.
5. **Hydra bridge.** Implement and test the UsdImaging adapter independently
   of the renderer's GPU representation.
6. **MMD renderer path.** Bring up diffuse/alpha, toon ramp, sphere
   multiply/add, sub-texture, outline, shadow flags, material morph runtime,
   then advanced UV and vertex-color behavior.
7. **Common-runtime review.** Compare the completed MMD and VRM adapters and
   extract only proven renderer-private common code. Revisit a USD-level
   `ToonMaterialAPI` only with evidence from both.

Compatibility follows the stamp. A reader that supports both versions reads
`/Asset.customData["mmd:stageContractVersion"]`: at 2 it reads
`inputs:mmd:material:<name>` through `MmdMaterialAPI`; at 1 it reads the
schema-less `mmd:material:<name>` with the same type and meaning, and its
graphs hold copies. A stage-contract v1 reader does not understand a v2
stage's material values. It still finds the same `/preview` and `/mtlx`
graphs, but they now read those values through connections. No migration turns
PreviewSurface or MaterialX into the source of truth.
