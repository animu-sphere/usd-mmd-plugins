# Stage contract

> Status: stage-contract version **1**. Each section becomes binding when the
> Phase that first authors it lands with a fixture (Phase numbers are
> [DESIGN_POLICY.md §14](DESIGN_POLICY.md#14-phases)); until then it may be
> corrected here without a version bump. **Binding** since Phase 2: §2–§10
> as far as they describe `/Asset`, `geo`, `mtl` and `skel` — the stage
> metadata and every key of §5, the coordinate conversion, identifiers, the
> mesh with its UVs, per-vertex data and material subsets, double-sidedness,
> the skeleton, joint provenance, skinning and weight normalization, and the
> material prims with the part of their semantics §10 names — and the §14
> checklist, which the integration tests assert on every fixture. **Binding**
> since Phase 4: §11, the morph prims with every property it names.
> **Binding** since Phase 5: §12, the control rig with every property it
> names, and the §6.3 rows for fixed axes and local axes.
> **Binding** since Phase 6: §13, the rigid bodies and joints with every
> property it names, and the §6.3 rows for Euler angles, lengths and
> translation limits. The material semantics and `preview`/`mtlx`
> graph boundaries are binding from Phase 3; their interior shader node names
> remain realization-local.
>
> `MmdMaterialAPI` is admitted but not yet authored by the current contract-v1
> implementation. Its planned application is additive within contract v1: the
> existing `mmd:material:*` names, types and meanings remain unchanged, and
> readers have an explicit compatibility path
> ([MATERIAL_POLICY.md §14](MATERIAL_POLICY.md#14-migration-and-implementation-order)).
>
> This document fixes the exact USD that `usdMmdFileFormat` authors from a PMX:
> stage metadata, prim hierarchy, types, names, the coordinate conversion, and
> the layout of skeleton, materials, morphs, control rig and physics. Material graphs are detailed in
> [MATERIAL_POLICY.md](MATERIAL_POLICY.md) and identifier rules in
> [TEXT_ENCODING_POLICY.md](TEXT_ENCODING_POLICY.md); on those topics they win.
>
> Section numbers are stable.

---

## 1. Scope

The contract covers the anonymous layer the importer returns for a `.pmx`
file. It is what `hydra-toon`, `usd-avatar-runtime` and any tool may rely on
without reading PMX. It covers nothing a runtime produces (§15).

## 2. Contract version

```text
/Asset.customData["mmd:stageContractVersion"] = 1      (int)
```

The version is stamped on `/Asset`, not in `customLayerData`, because layer
metadata does not compose: once the asset is referenced into another stage, the
layer's `customLayerData` is no longer visible from the composed stage, while
`/Asset`'s customData travels with the reference. This matches
`/Asset.customData["vrm:schemaContractVersion"]` in `usd-vrm-plugins`.

The version increments **only** when downstream interpretation changes
incompatibly: a path, type, name, unit, basis, or property meaning a consumer
could depend on. It does not change for parser refactors, diagnostic wording,
build-system changes, performance work, the addition of a property a consumer
can ignore, or applying `MmdMaterialAPI` while its existing properties retain
the same names, types and meanings.

## 3. Authoring conventions

- **Standard schemas first** ([DESIGN_POLICY.md §2.1](DESIGN_POLICY.md#21-openusd-first)).
- **Provenance of a prim is `customData`** on that prim: `mmd:sourceName`,
  `mmd:sourceEnglishName`, `mmd:sourceIndex`. Provenance is for tools and
  debugging; nothing should need it to render or evaluate.
- **A `customData` key written `mmd:x` is a USD key path**, as it is in
  `usd-vrm-plugins`: it is stored as key `x` in an `mmd` sub-dictionary
  (`customData = { dictionary mmd = { … } }` in `.usda`) and read with
  `GetCustomDataByKey("mmd:x")`. `customData["mmd:x"]` in this document means
  that key path.
- **Provenance of an element that is not a prim** — a joint, a vertex — is a
  parallel array attribute on the prim that owns the elements
  (`mmd:bone:sourceName` on the Skeleton, for example).
- **Semantics a consumer evaluates are namespaced attributes**:
  `mmd:material:*`, `mmd:morph:*`, `mmd:rig:*`, `mmd:physics:*`. They are
  attributes rather than customData so they can be queried and overridden in a
  stronger layer. The current implementation authors all of them as custom
  attributes; Phase 8 applies `MmdMaterialAPI` and declares the existing
  material names verbatim, while the other families remain schema-less
  ([DESIGN_POLICY.md §6](DESIGN_POLICY.md#6-the-schema-admission-test)).
- **Per-vertex MMD data is a primvar** in the `mmd:` namespace
  (`primvars:mmd:uv1`, `primvars:mmd:edgeScale`, …), so it follows the mesh
  through any primvar-aware pipeline.
- **Nothing time-varying.** A PMX stage has no time samples and no
  `timeCodesPerSecond`.
- **Deterministic order.** Prims are authored in canonical order (§7), arrays
  in canonical element order, and dictionaries in key order. No timestamp,
  host name, absolute path or importer version is written into the stage
  (build metadata lives in the bundle's `buildInfo.json`).

## 4. Prim hierarchy

```text
/Asset                          UsdSkelRoot (Xform when the model has no bones), kind = component
│   customData: mmd:stageContractVersion, provenance (§5), mmd:diagnostics
├─ geo                          Scope
│  └─ Mesh                      UsdGeomMesh + UsdSkelBindingAPI
│     └─ <materialId>           UsdGeomSubset (familyName = materialBind), one per non-empty material
├─ mtl                          Scope
│  └─ <materialId>              UsdShadeMaterial  (Phase 8: + MmdMaterialAPI; graphs: MATERIAL_POLICY.md)
├─ skel                         Scope
│  └─ Skeleton                  UsdSkelSkeleton
├─ morph                        Scope                                       (Phase 4)
│  └─ <morphId>                 UsdSkelBlendShape for vertex morphs; typeless otherwise
├─ rig                          Scope — MMD control semantics               (Phase 5)
│  ├─ Bones                     typeless: every joint's control semantics, parallel to the Skeleton's joints
│  └─ ik                        Scope
│     └─ <boneId>               typeless: one IK chain, named by its IK bone
└─ physics                      Scope — rigid bodies and joints             (Phase 6)
   ├─ rigidBodies                Scope
   │  └─ <rigidBodyId>           Xform + PhysicsRigidBodyAPI, PhysicsMassAPI
   │     └─ collider             Sphere, Cube or Capsule + PhysicsCollisionAPI, purpose = guide
   └─ joints                     Scope
      └─ <jointId>               PhysicsJoint; typeless for a type UsdPhysics has none for
```

A scope is authored only when it has children: a PMX with no morphs has no
`/Asset/morph`. The minimal stage for a model with geometry and bones is
`/Asset`, `/Asset/geo`, `/Asset/mtl`, `/Asset/skel`, `/Asset/rig`.

`/Asset` is the default prim in every case: it gives a predictable
`defaultPrim`, references and payloads that need no target path, one layout
across the VRM, MMD and other `animu-sphere` format plugins, and a natural home
for model-level metadata.

### 4.1 Why `/Asset` is the SkelRoot

`UsdSkel` applies skinning and blend shapes only to geometry **beneath** a
`UsdSkelRoot`. If the SkelRoot were a sibling scope (`/Asset/rig/SkelRoot`),
meshes under `/Asset/geo` would be outside it and would render in their rest
pose in every `UsdSkel` consumer, `usdview` included. Making `/Asset` itself
the SkelRoot keeps every mesh, blend shape and the skeleton inside one skel
scope while leaving the geometry where the `/Asset/geo` vocabulary puts it.
`UsdSkelRoot` is a `UsdGeomXformable`, so `/Asset` still behaves as an
xformable model root.

The deformation skeleton lives at `/Asset/skel/Skeleton`, and `/Asset/rig`
holds the control semantics (IK chains, append transforms, axes; §12). That is
the `usd-vrm-plugins` layout — `skel/Skeleton` beside `rig/Humanoid` — and it is
the deformation/control split
[DESIGN_POLICY.md §7.3](DESIGN_POLICY.md#73-the-deformation-skeleton-is-not-the-control-rig)
requires. The implementation policy's `/Asset/rig/SkelRoot/Skeleton` layout is
superseded for this reason
([DESIGN_POLICY.md §19](DESIGN_POLICY.md#19-where-this-document-departs-from-the-implementation-policy)).

A PMX with **no bones** authors `/Asset` as a `UsdGeomXform`, no
`/Asset/skel`, an unskinned mesh, and vertex morphs preserved without
`UsdSkelBlendShape` (they need a SkelRoot), with `MMD_MORPH_NO_SKELETON`.

## 5. Model metadata and provenance

On `/Asset`:

| Key (`customData`) | Type | Value |
| --- | --- | --- |
| `mmd:stageContractVersion` | `int` | `1` (§2) |
| `mmd:sourceFormat` | `string` | `"PMX"` |
| `mmd:sourceVersion` | `string` | `"2.0"` or `"2.1"` |
| `mmd:sourceName` | `string` | model name, as decoded |
| `mmd:sourceEnglishName` | `string` | English model name, as decoded |
| `mmd:sourceComment` | `string` | comment, as decoded |
| `mmd:sourceEnglishComment` | `string` | English comment, as decoded |
| `mmd:diagnostics` | `string[]` | every recoverable diagnostic raised while importing, as `CODE: message`, in emission order ([DIAGNOSTICS.md](../reference/DIAGNOSTICS.md#4-surfacing)); not authored when there are none |

The original file is never embedded. Per-object provenance is the source index
plus source names (§3).

## 6. Coordinate conversion

All source-space conversion happens **once**, in `mmdModel`'s canonicalization.
Geometry, skeleton, morph and physics code never flip an axis or scale a
value; they receive canonical values already in the USD basis and in meters.

### 6.1 Bases

| | Handedness | Up | Model faces | Unit |
| --- | --- | --- | --- | --- |
| PMX (source) | left-handed | +Y | −Z | MMD unit |
| USD (authored) | right-handed | +Y | +Z | meter (`metersPerUnit = 1`) |

A left-handed basis is converted to a right-handed one by mirroring one axis.
Mirroring Z keeps +Y up and turns the model to face +Z, which is the front
direction `usd-vrm-plugins` normalizes VRM avatars to, so MMD and VRM assets
face the same way on a stage.

### 6.2 Unit scale

```text
s = 0.08 meters per MMD unit        (STAGE-O1, decided in Phase 2)
```

MMD has no declared unit. The de facto convention across MMD tooling is that one
unit is about 8 cm, which makes a typical ~20-unit model about 1.6 m tall. The
stage is authored in meters (`metersPerUnit = 1`, frozen by
[DESIGN_POLICY.md §15](DESIGN_POLICY.md#15-decisions-frozen-early)), so every
length is multiplied by `s` during canonicalization, and the source unit is
never expressed through `metersPerUnit`. The value of `s` is part of the
contract: changing it after release bumps §2.

A value authored as `float` (points, SDEF parameters) is converted in double
and rounded to `float` once; a value authored as `double` (a joint's
translation) is converted in double from the source floats and never passes
through `float`. The mirror of a zero is `+0`, never `−0`. Canonicalization is
compiled without floating-point contraction, so the same bytes author the same
stage on every platform.

### 6.3 Conversion functions

With `S = diag(1, 1, −1)`:

| Quantity | Conversion |
| --- | --- |
| point / position | `(x, y, z) → s · (x, y, −z)` |
| direction, normal | `(x, y, z) → (x, y, −z)`, renormalized |
| displacement (morph offset, translation) | `(x, y, z) → s · (x, y, −z)` |
| unit quaternion `(x, y, z, w)` | `→ (−x, −y, z, w)` |
| axial vector (an impulse morph's torque) | `(x, y, z) → (−x, −y, z)`, unscaled — a mirror reverses rotation about X and Y |
| rotation matrix `R` | `→ S · R · S` |
| affine matrix | rotation part `S · R · S`, translation part as a point |
| fixed axis (the one axis a bone rotates about) | `(x, y, z) → (−x, −y, z)`, as an axial vector; unscaled and not normalized |
| local axes (a bone's X and Z) | the frame as a rotation, `S · R · S`: X `→ (x, y, −z)`, Z `→ (−x, −y, z)`; unscaled and not normalized, and `Y = Z × X` in both bases |
| Euler angles `(x, y, z)` (rigid bodies, joints) | compose as `R = Ry · Rx · Rz` acting on column vectors — Z first, then X, then Y ([PMX_CONTRACT.md §13](PMX_CONTRACT.md#13-rigid-bodies-and-joints), PMX-O1) — convert as `S · R · S`, and keep it as a quaternion with `w ≥ 0`; computed in double and rounded once |
| rotation limits about X, Y (IK links, joints) | `[min, max] → [−max, −min]` — a Z mirror reverses rotation about X and Y |
| rotation limits about Z | unchanged |
| translation limits along Z | `[min, max] → s · [−max, −min]`; along X and Y, `× s` |
| triangle winding | `(i0, i1, i2) → (i0, i2, i1)` |
| UV | `(u, v) → (u, 1 − v)` for `primvars:st` only (§8.3) |
| scalar lengths (rigid-body sizes, edge offsets in units) | `× s`, never mirrored |
| dimensionless values (weights, ratios, colors) | unchanged |

Winding is reversed because the Z mirror changes handedness: a face that was
front-facing in the source would otherwise face away under USD's default
`orientation = "rightHanded"`. The contract reverses indices rather than
authoring `orientation = "leftHanded"`, because not every consumer honors
`orientation`.

Every function in this table is verified by one fixture that carries the same
asymmetric transform through mesh vertices, normals, bones, morph deltas,
rigid-body transforms and joint transforms, and asserts that they still agree
after import.

## 7. Names and identifiers

Every prim authored from a PMX element (material, morph, rigid body, joint) and
every joint token is named by that element's **stable identifier**, never by
its source name. The algorithm — ASCII-safe sanitized English name when
available and unique, otherwise `<kind>_<NNNN>` — and the collision rule are
fixed in
[TEXT_ENCODING_POLICY.md §6](TEXT_ENCODING_POLICY.md#6-stable-identifiers).
Source names are preserved per §3, so a tool can always show `左腕`, `センター`,
`まばたき` even though the paths read `LeftArm`, `bone_0003`, `morph_0031`.

Canonical order for prims is source-table order, except joints (§9.1).

## 8. Geometry

### 8.1 One mesh, material subsets

A PMX holds one vertex table and one index table, partitioned into
per-material face ranges. Contract v1 authors that as **one** `UsdGeomMesh`,
`/Asset/geo/Mesh` (STAGE-O2, decided in Phase 2), whenever the vertex table is
not empty, with one `UsdGeomSubset` per material that owns at least one face:

```text
/Asset/geo/Mesh                     UsdGeomMesh
    subdivisionScheme = "none"
    orientation       = "rightHanded"   (default; winding already converted, §6.3)
    points, faceVertexCounts (all 3), faceVertexIndices, extent
    normals            (vertex)
    primvars:st        (vertex)
    material subsets:
    /<materialId>                   UsdGeomSubset
        elementType = "face", familyName = "materialBind"
        indices = the material's face range
        material:binding → /Asset/mtl/<materialId>
```

The `materialBind` family type is `partition` when the material ranges cover
every face exactly once — which a valid PMX always does — and
`nonOverlapping` when a repaired source does not. A material with a zero-length
range still gets a `UsdShadeMaterial` but no subset.

One mesh preserves the source topology, keeps vertex morphs as single blend
shapes, and avoids fragmenting a model along source-format lines. Splitting
into several logically named meshes (body, hair, face) would need heuristics
this contract does not want to freeze.

`subdivisionScheme = "none"` is mandatory: PMX meshes are final polygons, and
USD's default (`catmullClark`) would smooth them in any viewer that honors it.

### 8.2 Normals

PMX stores one normal per vertex; it is authored as the mesh's `normals`
attribute with `vertex` interpolation, converted per §6.3. Normals are never
regenerated — smooth normals cannot be assumed to reproduce the source's.

### 8.3 UVs

| Primvar | Type | Interpolation | Source |
| --- | --- | --- | --- |
| `primvars:st` | `texCoord2f[]` | vertex | PMX UV, with `v → 1 − v` |
| `primvars:mmd:uv1` … `uv4` | `float4[]` | vertex | PMX additional vec4 1–4, **raw** |

PMX UVs have their origin at the top-left (the Direct3D convention); USD `st`
has it at the bottom-left, so `st` is flipped. Additional vec4 channels carry
model-defined data — a sphere sub-texture UV, an effect parameter, anything —
so they are authored unmodified and their meaning stays with the consumer.
Channels the header does not declare are not authored.

### 8.4 Other per-vertex data

| Primvar | Type | Authored when | Content |
| --- | --- | --- | --- |
| `primvars:mmd:edgeScale` | `float[]` | always | PMX per-vertex edge (outline) scale |
| `primvars:mmd:deformType` | `int[]` | always, with bones | 0 BDEF1, 1 BDEF2, 2 BDEF4, 3 SDEF, 4 QDEF |
| `primvars:mmd:sdefC`, `sdefR0`, `sdefR1` | `point3f[]` | with bones, when any vertex is SDEF | SDEF parameters, converted per §6.3; zero for non-SDEF vertices |

PMX has no generic vertex color. If a later source provides colors with
matching semantics, they go to `primvars:displayColor` / `displayOpacity`.

### 8.5 Double-sidedness

PMX culls per material (drawing flag `0x01`, no-cull); `UsdGeomMesh.doubleSided`
is per mesh. The mesh is authored `doubleSided = true` when **any** material
that draws at least one face disables culling, which is the common case for
hair, skirts and accessories and avoids holes in generic viewers; single-sided
materials then render their back faces too, which is usually invisible on
closed surfaces. Otherwise `doubleSided` is not authored, and USD's fallback
(`false`) applies. The per-material flag is preserved exactly as
`mmd:material:doubleSided`
([MATERIAL_POLICY.md §4](MATERIAL_POLICY.md#4-canonical-material-semantics)),
and the capability matrix calls this *approximated* (STAGE-O3, decided in
Phase 2).

## 9. Skeleton and skinning

### 9.1 Canonical joint order

The joint order is the PMX bone-table order **if** every bone's parent precedes
it — `UsdSkel` requires parents before children. If not, the canonical order is
the stable topological order that repeatedly emits the **lowest-source-index**
bone whose parent has already been emitted (or that has none). The result is
unique for a given bone table, equals the source order whenever the source is
already valid, and raises `MMD_SKEL_JOINTS_REORDERED` (info) when it is not.

- A parent index out of range, or a bone that is its own parent, makes that
  bone a root: `MMD_SKEL_INVALID_PARENT`. The parser has already read an
  out-of-range parent as none and reported it
  ([PMX_CONTRACT.md §4](PMX_CONTRACT.md#4-indices)), so from a file this
  code means a bone that is its own parent.
- A parent cycle is broken by making the lowest-source-index bone in the cycle a
  root: `MMD_SKEL_PARENT_CYCLE`. Both repairs happen before the order is
  computed, self-parents first.

Joint indices everywhere on the stage (skinning, morphs, rig) refer to the
canonical order. The source index of each joint stays recoverable (§9.3).

### 9.2 The Skeleton prim

```text
/Asset/skel/Skeleton            UsdSkelSkeleton
    joints          token[]     hierarchical paths of stable identifiers, e.g. "AllParent/Center/UpperBody"
    bindTransforms  matrix4d[]  world space: identity rotation, translation = bone position (§6.3)
    restTransforms  matrix4d[]  parent-relative: identity rotation, translation = position − parent position
```

Joint paths are built from stable identifiers of each bone's ancestors, never
from display names. PMX bones carry a position and no orientation; MMD poses
bones in a world-aligned frame, so every joint's rest and bind rotation is the
identity. A rest translation is the source offset from the parent's position,
taken from the source floats and converted once (§6.2), so it is exact rather
than a difference of two converted positions. Local axes, fixed axes and
bone tails are control or display semantics, preserved under `/Asset/rig`
(§12), not joint orientations.

### 9.3 Joint provenance

Parallel to `joints`, on the Skeleton prim:

| Attribute | Type | Content |
| --- | --- | --- |
| `mmd:bone:sourceIndex` | `int[]` | PMX bone-table index |
| `mmd:bone:sourceName` | `string[]` | decoded bone name |
| `mmd:bone:sourceEnglishName` | `string[]` | decoded English bone name |

### 9.4 Skinning

On `/Asset/geo/Mesh`, with `UsdSkelBindingAPI` applied:

```text
skel:skeleton               → /Asset/skel/Skeleton
primvars:skel:jointIndices  int[]    vertex, elementSize = N
primvars:skel:jointWeights  float[]  vertex, elementSize = N
```

`N` is the largest influence count the mesh needs: 1 if every vertex is BDEF1,
2 if the largest is BDEF2 or SDEF, 4 if any vertex is BDEF4 or QDEF. Shorter
influence lists are padded with weight 0 on joint 0.

| PMX deform | Authored influences | Status |
| --- | --- | --- |
| BDEF1 | `(b0, 1)` | exact |
| BDEF2 | `(b0, w), (b1, 1 − w)` | exact |
| BDEF4 | four pairs, as stored | exact after §9.5 |
| SDEF | `(b0, w), (b1, 1 − w)` + SDEF primvars (§8.4) | linear-blend approximation, `MMD_SKEL_SDEF_APPROXIMATED` once per import |
| QDEF | four pairs, as BDEF4 + deform type | unverified, `MMD_SKEL_QDEF_APPROXIMATED` once per import |

### 9.5 Weight normalization

PMX does not guarantee that BDEF4/QDEF weights sum to 1. Canonicalization
rescales a vertex whose weights sum to a positive value other than 1 (beyond a
tolerance of `1e-5`), raising one `MMD_SKEL_WEIGHTS_NORMALIZED` (info) with the
count of affected vertices. A vertex whose weights sum to 0 is bound fully to its
first bone, raising `MMD_SKEL_ZERO_WEIGHTS` (warning). Negative weights are
clamped to 0 before either rule (STAGE-O5, decided in Phase 2).

Precisely, per vertex and in this order:

1. The influences are those the deform type stores (BDEF2's and SDEF's second
   weight is `1 − weight₁`).
2. An influence whose bone names nothing is dropped, whatever its weight — the
   parser reported the weighted ones
   ([PMX_CONTRACT.md §5](PMX_CONTRACT.md#5-vertices-and-deform)). The rest keep
   their stored order, weight-0 ones included.
3. A negative or non-finite weight becomes 0.
4. If the weights sum to 0, the vertex is bound fully to the first stored bone
   that names a bone, or to joint 0 when none does. Otherwise, a sum beyond the
   tolerance is rescaled to 1; a sum within it is kept as stored.

Both diagnostics are raised once per import, located at the first affected
vertex.

## 10. Materials

One `UsdShadeMaterial` per PMX material at `/Asset/mtl/<materialId>`, in
material-table order, carrying the MMD source semantics as `mmd:material:*`
attributes and two realization graphs, `preview` and `mtlx`. Bindings target
the material prim, never a node inside it. The current implementation authors
schema-less custom attributes; Phase 8 applies the single-apply
`MmdMaterialAPI` and authors the same properties through its generated
accessors. Fully specified in
[MATERIAL_POLICY.md](MATERIAL_POLICY.md).

Each material carries its provenance (`mmd:sourceName`,
`mmd:sourceEnglishName`, `mmd:sourceIndex`, and the verbatim path of each
texture slot that names a texture), `mmd:material:doubleSided`, and the three
texture slots — `mmd:material:texture`, `sphereTexture`, and `toonTexture`
for an individual toon ramp — each authored only when its path is safe. The
full MMD semantics are authored as `mmd:material:*` attributes, and both
portable realization graphs are present from Phase 3 as specified by
MATERIAL_POLICY.md.

`MmdMaterialAPI` is canonical identification and declaration, not a third
realization. A schema-aware consumer uses its generated accessors when the API
is applied and accepts the same schema-less property names on an earlier
contract-v1 asset. PreviewSurface and MaterialX remain generic fallbacks and are
never used to reconstruct canonical MMD values.

The material-table index is also MMD's **draw order**, which alpha-blended
MMD rendering depends on; it is preserved as `mmd:sourceIndex` and consumers
that need draw order read it from there.

## 11. Morphs

Authored from Phase 4. Every PMX morph becomes one prim under `/Asset/morph`,
named by its stable identifier, in morph-table order. The scope exists only
when the model has a morph.

Every morph prim carries:

| Property | Kind | Content |
| --- | --- | --- |
| `mmd:morph:type` | attribute, `uniform token` | `group`, `vertex`, `bone`, `uv`, `uv1`–`uv4`, `material`, `flip`, `impulse` |
| `mmd:morph:panel` | attribute, `uniform token` | `hidden`, `eyebrow`, `eye`, `mouth`, `other` |
| `mmd:sourceName`, `mmd:sourceEnglishName`, `mmd:sourceIndex` | customData | provenance |

### 11.1 Vertex morphs

A vertex morph is a `UsdSkelBlendShape` with the schema's own `offsets`
(§6.3 displacement) and sparse `pointIndices`, and the mesh lists it in
`skel:blendShapes` / `skel:blendShapeTargets` under the same identifier, in
the same order as the prims it targets. An offset whose vertex the parser
rejected is dropped rather than repaired.

A model with **no bones** authors no SkelRoot, and a blend shape outside one
deforms nothing (§4.1); a model with **no mesh** has nothing to name one, and
a blend shape nothing names deforms nothing either. In both cases the morph
is a typeless prim carrying the same two arrays as `mmd:morph:offsets`
(`vector3f[]`) and `mmd:morph:pointIndices` (`int[]`), and the import records
`MMD_MORPH_NO_SKELETON`.

### 11.2 Every other morph type

A morph of any other type is a **typeless** prim carrying its declarative
semantics as `mmd:morph:*` properties (STAGE-O4). Each is `uniform`, and each
list is parallel to the others of its type, in the source's offset order. An
offset whose target the parser rejected is dropped; a material morph's
`−1` keeps its source meaning, **every material**.

| Type | Properties |
| --- | --- |
| `group`, `flip` | `mmd:morph:members` (`rel`, to morph prims) and `mmd:morph:weights` (`float[]`) |
| `bone` | `mmd:morph:joints` (`int[]`, canonical joint indices), `mmd:morph:translations` (`vector3f[]`, §6.3 displacement), `mmd:morph:rotations` (`quatf[]`, §6.3 quaternion) |
| `uv`, `uv1`–`uv4` | `mmd:morph:pointIndices` (`int[]`) and `mmd:morph:uvOffsets` (`float4[]`, **raw**: a primary-UV delta is against source `v`, so a consumer applying it to `primvars:st` negates the `v` component) |
| `material` | `mmd:morph:materialIndices` (`int[]`, `−1` = every material), `mmd:morph:materialOperations` (`token[]`: `multiply`, `add`), and one array per modulated value: `diffuseColors` (`color4f[]`), `specularColors` (`color3f[]`), `specularPowers` (`float[]`), `ambientColors` (`color3f[]`), `edgeColors` (`color4f[]`), `edgeSizes` (`float[]`), `textureTints`, `sphereTints`, `toonTints` (`float4[]`) |
| `impulse` | `mmd:morph:rigidBodyIndices` (`int[]`, indices into `/Asset/physics/rigidBodies`, which keeps the source order, §13), `mmd:morph:impulseLocal` (`bool[]`), `mmd:morph:velocities` (`vector3f[]`, §6.3 displacement), `mmd:morph:torques` (`vector3f[]`, §6.3 axial vector) |

### 11.3 Nothing is evaluated

No group or flip morph expands into geometry, no bone morph moves the rest
skeleton, no material morph becomes a material variant or edits a material
prim, and no impulse is ever applied. A group or flip member that reaches its
own morph — directly or through other group morphs — is dropped with
`MMD_MORPH_GROUP_CYCLE`, so a consumer that does expand one terminates; that
is a validation, not an evaluation.

## 12. Control rig

Authored from Phase 5, whenever the model has bones. `/Asset/rig` holds MMD's
**control** semantics — everything a PMX bone carries beyond the position and
parent the deformation skeleton is built from (§9) — declaratively, and
without a second joint hierarchy
([DESIGN_POLICY.md §7.3](DESIGN_POLICY.md#73-the-deformation-skeleton-is-not-the-control-rig)).
Every joint index on it is a canonical joint index (§9.1) into
`/Asset/skel/Skeleton`'s `joints`, as a bone morph's are (§11.2), and `−1`
names none. Every property is a `uniform` custom attribute, and no API schema
is applied (STAGE-O6).

### 12.1 Per-joint control semantics

`/Asset/rig/Bones` is a typeless prim whose arrays are each parallel to the
Skeleton's `joints`: element `j` describes joint `j`.

| Attribute | Type | Content |
| --- | --- | --- |
| `mmd:rig:transformLayers` | `int[]` | PMX transform layer, as stored |
| `mmd:rig:deformAfterPhysics` | `bool[]` | flag `0x1000` |
| `mmd:rig:rotatable`, `translatable`, `visible`, `operable` | `bool[]` | flags `0x0002`, `0x0004`, `0x0008`, `0x0010` |
| `mmd:rig:tailJoints` | `int[]` | the tail joint when the tail is a bone (flag `0x0001`), else `−1` |
| `mmd:rig:tailOffsets` | `vector3f[]` | the tail offset when it is not a bone, §6.3 displacement; else zero |
| `mmd:rig:appendSources` | `int[]` | the joint whose transform the bone appends (flag `0x0100` or `0x0200`), else `−1` |
| `mmd:rig:appendRatios` | `float[]` | the append ratio; `0` where `appendSources` is `−1` |
| `mmd:rig:appendRotation`, `appendTranslation`, `appendLocal` | `bool[]` | flags `0x0100`, `0x0200`, `0x0080`; all `false` where `appendSources` is `−1` |
| `mmd:rig:hasFixedAxis`, `mmd:rig:fixedAxes` | `bool[]`, `vector3f[]` | flag `0x0400`, and the axis (§6.3 fixed axis); zero where there is none |
| `mmd:rig:hasLocalAxes`, `mmd:rig:localAxesX`, `mmd:rig:localAxesZ` | `bool[]`, `vector3f[]`, `vector3f[]` | flag `0x0800`, and the X and Z axes (§6.3 local axes); zero where there are none |
| `mmd:rig:hasExternalParent`, `mmd:rig:externalParentKeys` | `bool[]`, `int[]` | flag `0x2000`, and the key; `0` where there is none |

The transform layer and the after-physics flag are MMD's **evaluation order**
and are preserved exactly; the canonical joint order is not an evaluation
order and must not be read as one
([PMX_CONTRACT.md §9](PMX_CONTRACT.md#9-bones)).

### 12.2 IK chains

One typeless prim per IK chain at `/Asset/rig/ik/<boneId>`, named by its IK
bone's stable identifier, in bone-table order, carrying that bone's
provenance (`mmd:sourceName`, `mmd:sourceEnglishName`, `mmd:sourceIndex`) as
`customData`. The `ik` scope exists only when there is a chain.

| Attribute | Type | Content |
| --- | --- | --- |
| `mmd:rig:joint` | `int` | the IK bone: the goal |
| `mmd:rig:effector` | `int` | PMX's IK target: the joint brought to the goal |
| `mmd:rig:loopCount` | `int` | as stored |
| `mmd:rig:limitAngle` | `float` | radians per iteration, unchanged |
| `mmd:rig:linkJoints` | `int[]` | the links, in source order |
| `mmd:rig:linkHasLimits` | `bool[]` | whether each link is limited |
| `mmd:rig:linkLowerLimits`, `mmd:rig:linkUpperLimits` | `vector3f[]` | radians about X, Y and Z, §6.3 rotation limits; zero for an unlimited link |

### 12.3 Repairs

A relation that names a bone the parser rejected (`MMD_PMX_INDEX_OUT_OF_RANGE`,
[PMX_CONTRACT.md §9](PMX_CONTRACT.md#9-bones)) is dropped rather than
repaired: a tail becomes `−1`, an append clears every append field, an IK link
leaves its chain. An IK bone whose effector names no bone — rejected, or `−1`
in the source — authors no chain: it brings nothing anywhere. The bone's other
semantics are kept.

### 12.4 Nothing is evaluated

No IK chain is solved, no append is applied, no axis constrains a rotation, no
external parent is resolved, and nothing reorders the skeleton. A consumer
reconstructs every IK chain and append relation from these properties alone,
which is Phase 5's acceptance
([DESIGN_POLICY.md §14](DESIGN_POLICY.md#14-phases)).

## 13. Physics

Authored from Phase 6, whenever the model has a rigid body. `/Asset/physics`
holds PMX's rigid bodies and joints: as standard `UsdPhysics` where its
semantics match
([DESIGN_POLICY.md §8](DESIGN_POLICY.md#8-physics-policy)), and with every PMX
value as a `uniform` `mmd:physics:*` attribute beside it, so a consumer that
reads only `mmd:physics:*` recovers every body and joint. No MMD API schema
is applied (STAGE-O6). Rigid-body indices are indices into
`/Asset/physics/rigidBodies`' children, which keep the source table's order,
and joint indices are canonical joint indices (§9.1); `−1` names none.

### 13.1 Rigid bodies

One `UsdGeomXform` per rigid body at `/Asset/physics/rigidBodies/<id>`, in
source order, with `PhysicsRigidBodyAPI` and `PhysicsMassAPI` applied and the
body's provenance as `customData`.

| Property | Content |
| --- | --- |
| `xformOp:translate` (`double3`), `xformOp:orient` (`quatf`) | the rest frame: §6.3 point, and the Euler angles through §6.3 (`w` never negative) |
| `physics:kinematicEnabled` | `true` exactly when the body follows its bone |
| `physics:mass` | the source's mass, only when it is positive: `UsdPhysics` reads 0 as "not given" |
| `mmd:physics:bone` (`int`) | the canonical joint of the bone the body is attached to |
| `mmd:physics:shape` (`token`) | `sphere`, `box`, `capsule` |
| `mmd:physics:size` (`float3`) | meters, `× s` and never mirrored: a sphere's radius in x; a box's half extents; a capsule's radius in x and cylinder length in y |
| `mmd:physics:collisionGroup`, `mmd:physics:collisionMask` (`int`) | as stored: the body is in group `g` (0–15), and collides with group `h` when bit `h` of the mask is set |
| `mmd:physics:mass`, `linearDamping`, `angularDamping`, `restitution`, `friction` (`float`) | as stored |
| `mmd:physics:mode` (`token`) | `followBone` (kinematic), `dynamic`, `dynamicWithBone` (simulated, the bone's position kept) |

Its child `collider` is the shape — `UsdGeomSphere` (`radius`),
`UsdGeomCube` (`size = 2` scaled by the half extents), or `UsdGeomCapsule`
(`radius`, `height` the cylinder length, `axis = "Y"`) — with `extent`,
`PhysicsCollisionAPI`, and `purpose = "guide"`: collision geometry, not
something to render. Its lengths are converted in double (§6.2).

Collision filtering is **not** authored as `PhysicsCollisionGroup`: a PMX body
filters by its own group against the other body's mask, and bodies of one
group may carry different masks — which `UsdPhysics`' group-to-group filter
cannot say. Damping, restitution and friction have no body-level
`UsdPhysics` counterpart and are preserved only.

### 13.2 Joints

One prim per joint whose two bodies both exist at `/Asset/physics/joints/<id>`,
in source order, with the joint's provenance as `customData`. A `spring6Dof`
or `sixDof` joint is a `PhysicsJoint` — the generic 6-DOF joint — with:

| Property | Content |
| --- | --- |
| `physics:body0`, `physics:body1` | the rigid-body prims A and B |
| `physics:localPos0`, `localRot0`, `localPos1`, `localRot1` | the joint's frame in each body's rest frame |
| `PhysicsLimitAPI:transX` … `rotZ` | `physics:low` and `physics:high` per degree of freedom, meters or **degrees**, authored only where the source's lower limit is not above its upper one |

A lower limit above the upper one leaves that axis free in PMX (Bullet's
convention) and would lock it in `UsdPhysics`, so a free axis authors no
limit; equal limits lock it in both. Spring constants have no `UsdPhysics`
counterpart: `PhysicsDriveAPI` stiffness is in different units and the
conversion is a runtime's.

A PMX 2.1 `pointToPoint`, `coneTwist`, `slider` or `hinge` joint has no
documented mapping of its limits onto a `UsdPhysics` joint, so it is a
typeless prim carrying only the properties below, and the import raises
`MMD_PHYSICS_JOINT_UNMAPPED`, once, with the count. Every joint carries:

| Attribute | Type | Content |
| --- | --- | --- |
| `mmd:physics:type` | `token` | `spring6Dof`, `sixDof`, `pointToPoint`, `coneTwist`, `slider`, `hinge` |
| `mmd:physics:rigidBodyA`, `rigidBodyB` | `int` | the two bodies |
| `mmd:physics:position` | `point3d` | §6.3 point |
| `mmd:physics:orientation` | `quatf` | the Euler angles through §6.3 |
| `mmd:physics:translationLowerLimit`, `translationUpperLimit` | `vector3f` | meters, §6.3 translation limits |
| `mmd:physics:rotationLowerLimit`, `rotationUpperLimit` | `vector3f` | radians, §6.3 rotation limits |
| `mmd:physics:translationSpring`, `rotationSpring` | `vector3f` | as stored: their unit depends on the length scale ([PMX_CONTRACT.md §13](PMX_CONTRACT.md#13-rigid-bodies-and-joints)) |

### 13.3 Repairs

A body whose bone the parser rejected keeps `mmd:physics:bone = −1`. A joint
either of whose bodies is rejected, or `−1` in the source, authors no prim: it
joins nothing. A shape, physics mode or joint type the source's version does
not define is authored as `sphere`, `followBone` or `spring6Dof`, with
`MMD_PHYSICS_UNKNOWN_SHAPE`, `MMD_PHYSICS_UNKNOWN_MODE` or
`MMD_PHYSICS_UNKNOWN_JOINT_TYPE`
([PMX_CONTRACT.md §13](PMX_CONTRACT.md#13-rigid-bodies-and-joints)).

### 13.4 Nothing is simulated

No `PhysicsScene` is authored, no body carries a velocity, and nothing moves a
body off its rest frame or a bone after a body. A consumer recovers every
rigid body — its bone, shape and parameters — and every joint and the bodies
it joins from these properties alone, which is Phase 6's acceptance
([DESIGN_POLICY.md §14](DESIGN_POLICY.md#14-phases)). Runtime simulation is
the optional, downstream flow defined by
[PHYSICS_INTEGRATION.md](PHYSICS_INTEGRATION.md): `usd-physics-plugins`
simulates, `usd-stage-runner` orders the step, and the importer remains static.

## 14. Validation checklist

The stage tests assert, for every fixture that reaches the relevant Phase:

- `defaultPrim == "Asset"` and `/Asset` has `kind = component`;
- `upAxis == "Y"` and `metersPerUnit == 1`;
- `/Asset.customData["mmd:stageContractVersion"] == 1`;
- `/Asset` is a `UsdSkelRoot` exactly when the model has bones;
- `/Asset/geo/Mesh` is a `UsdGeomMesh` with `subdivisionScheme = "none"`;
- the `materialBind` subsets partition the faces, and each binds an existing
  `/Asset/mtl` material;
- `/Asset/skel/Skeleton` is a valid `UsdSkelSkeleton`: parent-before-child,
  unique joint paths, `bindTransforms` and `restTransforms` sized to `joints`;
- the mesh's skel binding resolves, joint indices are in range, and each
  vertex's weights sum to 1 within tolerance;
- every source name survives byte-exact in its provenance field, Japanese
  included;
- a texture whose filename is Japanese resolves through its `SdfAssetPath`;
- `/Asset/rig` exists exactly when the model has bones, every
  `/Asset/rig/Bones` array is sized to `joints`, and every IK chain and append
  relation the source holds is reconstructed from the stage alone;
- `/Asset/physics` exists exactly when the model has a rigid body, every rigid
  body and every joint that joins two is one prim with the values §13 names,
  every `UsdPhysics` joint's `body0` and `body1` agree with its
  `mmd:physics:rigidBodyA` and `rigidBodyB`, no `PhysicsScene` is authored,
  and every body's bone and every joint's bodies are recovered from the stage
  alone;
- no attribute has time samples.

Golden `.usda` baselines cover the compact fixtures; the checklist is asserted
semantically so it holds for every fixture.

## 15. What the stage never contains

Solved IK, evaluated append transforms, simulated physics, evaluated group,
bone, UV, material or impulse morphs, VMD animation, toon-shading results,
renderer-specific shaders, the original file bytes, or anything with a clock.
Those belong to runtimes and renderers that read this contract.

## 16. Open questions

Each is resolved in the Phase named, with a fixture, and recorded here when it
is.

| Id | Question | Proposed answer | Resolve by |
| --- | --- | --- | --- |
| — | none open | | |

Resolved:

| Id | Question | Decision | Resolved |
| --- | --- | --- | --- |
| STAGE-O1 | Unit scale `s` | `0.08` m per MMD unit (§6.2). Locally held distributed character models import 1.57–1.71 m tall. | Phase 2, 2026-09-15 |
| STAGE-O2 | Mesh prim name and the one-mesh rule | `/Asset/geo/Mesh`, one mesh (§8.1) | Phase 2, 2026-09-15 |
| STAGE-O3 | Double-sidedness | the mesh is double-sided if any material that draws a face is no-cull (§8.5) | Phase 2, 2026-09-15 |
| STAGE-O5 | Weight normalization tolerance and zero-weight rule | `1e-5`; zero weights bind to the first bone that names one (§9.5) | Phase 2, 2026-09-15 |
| STAGE-O4 | Encoding of non-vertex morph semantics | typeless prims, one uniform `mmd:morph:*` array per field and a relationship for group and flip members (§11.2) | Phase 4, 2026-09-16 |
| STAGE-O6, rig half | Rig prim shapes | typeless prims with uniform `mmd:rig:*` attributes and no API schema, since no consumer has asked for one ([DESIGN_POLICY.md §6](DESIGN_POLICY.md#6-the-schema-admission-test)): per-joint arrays parallel to the Skeleton's `joints` on `/Asset/rig/Bones`, and one prim per IK chain, whose links vary in length (§12). The physics half stays open. | Phase 5, 2026-09-16 |
| STAGE-O6, physics half | Physics prim shapes | `UsdPhysics` where its semantics match — an `Xform` with `PhysicsRigidBodyAPI` and `PhysicsMassAPI` per body, a guide-purpose collider child, a `PhysicsJoint` with per-axis `PhysicsLimitAPI` for both 6-DOF joint types — and every PMX value as uniform `mmd:physics:*` beside it, with no MMD API schema. Collision groups, damping, friction, restitution and springs have no exact `UsdPhysics` counterpart and are preserved only; PMX 2.1's other joint types are typeless prims (§13). | Phase 6, 2026-09-17 |
