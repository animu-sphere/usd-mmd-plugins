# Stage contract

> Status: **proposed**, stage-contract version **1**. Each section becomes
> binding when the Phase that first authors it lands with a fixture (Phase
> numbers are [DESIGN_POLICY.md §14](DESIGN_POLICY.md#14-phases)); until then
> it may be corrected here without a version bump. The Phase 0 importer
> authors §2, the stage metadata and `/Asset` of §4 (as an `Xform`: it reads
> no bone table yet), and `mmd:stageContractVersion`, `mmd:sourceFormat`,
> `mmd:sourceVersion` and `mmd:diagnostics` of §5, with fixtures; nothing
> else here is authored yet.
>
> This document fixes the exact USD that `usdMmdFileFormat` authors from a PMX:
> stage metadata, prim hierarchy, types, names, the coordinate conversion, and
> the layout of skeleton, materials and morphs. Material graphs are detailed in
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
build-system changes, performance work, or the addition of a property a
consumer can ignore.

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
- **Semantics a consumer evaluates are namespaced custom attributes**:
  `mmd:material:*`, `mmd:morph:*`, `mmd:rig:*`, `mmd:physics:*`. They are
  attributes rather than customData so they can be queried, overridden in a
  stronger layer, and later declared verbatim by an applied API schema without
  changing the stage ([DESIGN_POLICY.md §6](DESIGN_POLICY.md#6-the-schema-admission-test)).
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
│  └─ <materialId>              UsdShadeMaterial  (graphs: MATERIAL_POLICY.md)
├─ skel                         Scope
│  └─ Skeleton                  UsdSkelSkeleton
├─ morph                        Scope                                       (Phase 4)
│  └─ <morphId>                 UsdSkelBlendShape for vertex morphs; typeless otherwise
├─ rig                          Scope — MMD control semantics               (reserved, Phase 5)
└─ physics                      Scope                                       (reserved, Phase 6)
   ├─ rigidBodies/<rigidBodyId>
   └─ joints/<jointId>
```

A scope is authored only when it has children: a PMX with no morphs has no
`/Asset/morph`. The minimal stage for a model with geometry and bones is
`/Asset`, `/Asset/geo`, `/Asset/mtl`, `/Asset/skel`.

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

The deformation skeleton lives at `/Asset/skel/Skeleton`, and `/Asset/rig` is
reserved for control semantics (IK chains, append transforms, axes). That is
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
s = 0.08 meters per MMD unit        (proposed — STAGE-O1)
```

MMD has no declared unit. The de facto convention across MMD tooling is that one
unit is about 8 cm, which makes a typical ~20-unit model about 1.6 m tall. The
stage is authored in meters (`metersPerUnit = 1`, frozen by
[DESIGN_POLICY.md §15](DESIGN_POLICY.md#15-decisions-frozen-early)), so every
length is multiplied by `s` during canonicalization, and the source unit is
never expressed through `metersPerUnit`. The value of `s` is part of the
contract: changing it after release bumps §2.

### 6.3 Conversion functions

With `S = diag(1, 1, −1)`:

| Quantity | Conversion |
| --- | --- |
| point / position | `(x, y, z) → s · (x, y, −z)` |
| direction, normal | `(x, y, z) → (x, y, −z)`, renormalized |
| displacement (morph offset, translation) | `(x, y, z) → s · (x, y, −z)` |
| unit quaternion `(x, y, z, w)` | `→ (−x, −y, z, w)` |
| rotation matrix `R` | `→ S · R · S` |
| affine matrix | rotation part `S · R · S`, translation part as a point |
| Euler angles (rigid bodies, joints) | compose to a matrix in the source's rotation order, convert the matrix, keep it as a quaternion; the source order is fixed in [PMX_CONTRACT.md §13](PMX_CONTRACT.md#13-rigid-bodies-and-joints) |
| rotation limits about X, Y (IK links, joints) | `[min, max] → [−max, −min]` — a Z mirror reverses rotation about X and Y |
| rotation limits about Z | unchanged |
| translation limits along Z | `[min, max] → s · [−max, −min]`; along X and Y, `× s` |
| triangle winding | `(i0, i1, i2) → (i0, i2, i1)` |
| UV | `(u, v) → (u, 1 − v)` for `primvars:st` only (§8.3) |
| scalar lengths (rigid-body sizes, edge offsets in units) | `× s` |
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
`/Asset/geo/Mesh` (STAGE-O2), with one `UsdGeomSubset` per material that owns at
least one face:

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
| `primvars:mmd:sdefC`, `sdefR0`, `sdefR1` | `point3f[]` | any vertex is SDEF | SDEF parameters, converted per §6.3; zero for non-SDEF vertices |

PMX has no generic vertex color. If a later source provides colors with
matching semantics, they go to `primvars:displayColor` / `displayOpacity`.

### 8.5 Double-sidedness

PMX culls per material (drawing flag `0x01`, no-cull); `UsdGeomMesh.doubleSided`
is per mesh. The mesh is authored `doubleSided = true` when **any** material
disables culling, which is the common case for hair, skirts and accessories and
avoids holes in generic viewers; single-sided materials then render their back
faces too, which is usually invisible on closed surfaces. The per-material flag
is preserved exactly as `mmd:material:doubleSided`
([MATERIAL_POLICY.md §4](MATERIAL_POLICY.md#4-canonical-material-semantics)),
and the capability matrix calls this *approximated* (STAGE-O3).

## 9. Skeleton and skinning

### 9.1 Canonical joint order

The joint order is the PMX bone-table order **if** every bone's parent precedes
it — `UsdSkel` requires parents before children. If not, the canonical order is
the stable topological order that repeatedly emits the **lowest-source-index**
bone whose parent has already been emitted (or that has none). The result is
unique for a given bone table, equals the source order whenever the source is
already valid, and raises `MMD_SKEL_JOINTS_REORDERED` (info) when it is not.

- A parent index out of range, or a bone that is its own parent, makes that
  bone a root: `MMD_SKEL_INVALID_PARENT`.
- A parent cycle is broken by making the lowest-source-index bone in the cycle a
  root: `MMD_SKEL_PARENT_CYCLE`.

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
identity. Local axes, fixed axes and bone tails are control or display
semantics, preserved under `/Asset/rig` (Phase 5), not joint orientations.

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
clamped to 0 before either rule (STAGE-O5).

## 10. Materials

One `UsdShadeMaterial` per PMX material at `/Asset/mtl/<materialId>`, in
material-table order, carrying the MMD source semantics as `mmd:material:*`
attributes and two realization graphs, `preview` and `mtlx`. Bindings target
the material prim, never a node inside it. Fully specified in
[MATERIAL_POLICY.md](MATERIAL_POLICY.md).

The material-table index is also MMD's **draw order**, which alpha-blended
MMD rendering depends on; it is preserved as `mmd:sourceIndex` and consumers
that need draw order read it from there.

## 11. Morphs

Authored from Phase 4. Every PMX morph becomes one prim under `/Asset/morph`,
named by its stable identifier, in morph-table order:

- A **vertex morph** is a `UsdSkelBlendShape` — `offsets` (§6.3 displacement)
  and sparse `pointIndices` — and the mesh lists it in `skel:blendShapes` /
  `skel:blendShapeTargets` under the same identifier.
- Every **other** morph type is a typeless prim carrying its declarative
  semantics as `mmd:morph:*` attributes (STAGE-O4): group and flip morphs as
  relationships to their member morph prims plus parallel weights; bone morphs
  as per-joint translations and rotations; UV morphs as per-vertex offsets;
  material morphs as the operation and parameter deltas; impulse morphs as
  their rigid-body targets and vectors.

Every morph prim carries:

| Property | Kind | Content |
| --- | --- | --- |
| `mmd:morph:type` | attribute, `token` | `group`, `vertex`, `bone`, `uv`, `uv1`–`uv4`, `material`, `flip`, `impulse` |
| `mmd:morph:panel` | attribute, `token` | `hidden`, `eyebrow`, `eye`, `mouth`, `other` |
| `mmd:sourceName`, `mmd:sourceEnglishName`, `mmd:sourceIndex` | customData | provenance |

Nothing is evaluated: no group expands into geometry, no bone morph moves the
rest skeleton, no material morph becomes a material variant.

## 12. Control rig — reserved

`/Asset/rig` is reserved for MMD control semantics (Phase 5): IK chains
(target, end effector, links, loop count, angle limits), append rotation and
translation (source bone, ratio), fixed and local axes, external parents,
transform layer and the after-physics flag, and bone tails. Nothing is authored
there before Phase 5, and nothing there is solved. The shape — plain attributes
or an admitted API schema — is decided then, under
[DESIGN_POLICY.md §6](DESIGN_POLICY.md#6-the-schema-admission-test).

## 13. Physics — reserved

`/Asset/physics/rigidBodies` and `/Asset/physics/joints` are reserved for
Phase 6. Standard `UsdPhysics` is used where its semantics match; unmatched
PMX parameters (collision groups and masks, physics mode, spring constants) are
preserved as `mmd:physics:*`. No simulation state is ever authored.

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
| STAGE-O1 | Unit scale `s` | `0.08` m per MMD unit (§6.2) | Phase 2 |
| STAGE-O2 | Mesh prim name and the one-mesh rule | `/Asset/geo/Mesh`, one mesh (§8.1) | Phase 2 |
| STAGE-O3 | Double-sidedness | mesh double-sided if any material is no-cull (§8.5) | Phase 2 |
| STAGE-O4 | Encoding of non-vertex morph semantics | typeless prims with `mmd:morph:*` attributes and relationships (§11) | Phase 4 |
| STAGE-O5 | Weight normalization tolerance and zero-weight rule | §9.5 | Phase 2 |
| STAGE-O6 | Rig and physics prim shapes | decided under the schema admission test | Phases 5, 6 |
