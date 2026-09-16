# Source mapping

Where each PMX field lands on the stage — a lookup table, not a contract. The
decisions behind each row are in [PMX_CONTRACT.md](../design/PMX_CONTRACT.md)
(source → canonical) and [STAGE_CONTRACT.md](../design/STAGE_CONTRACT.md) /
[MATERIAL_POLICY.md](../design/MATERIAL_POLICY.md) (canonical → USD); when
this table disagrees with them, they win and this table is the bug.

Status (2026-09-15): **rows of Phases 0–2 are implemented; the rest are
intended.** The Phase column says when a row became true or is expected to;
[CAPABILITY_MATRIX.md](CAPABILITY_MATRIX.md) says which claims a fixture
backs.

Paths are abbreviated: `Mesh` is `/Asset/geo/Mesh`, `Skel` is
`/Asset/skel/Skeleton`, `Mtl` is `/Asset/mtl/<materialId>`, `Morph` is
`/Asset/morph/<morphId>`.

## Header

| PMX field | USD | Phase |
| --- | --- | :---: |
| version | `/Asset` customData `mmd:sourceVersion` | 0 |
| (format) | `/Asset` customData `mmd:sourceFormat = "PMX"` | 0 |
| text encoding, index sizes, additional UV count | not authored (syntax only) | 1 |
| model name / English name | `/Asset` customData `mmd:sourceName` / `mmd:sourceEnglishName` | 2 |
| comment / English comment | `/Asset` customData `mmd:sourceComment` / `mmd:sourceEnglishComment` | 2 |

## Vertices and faces

| PMX field | USD | Phase |
| --- | --- | :---: |
| position | `Mesh.points` (converted, `× s`) | 2 |
| normal | `Mesh.normals` (vertex; converted) | 2 |
| UV | `Mesh` `primvars:st` (`v → 1 − v`) | 2 |
| additional vec4 1–4 | `Mesh` `primvars:mmd:uv1`–`uv4` (raw) | 2 |
| deform type | `Mesh` `primvars:mmd:deformType` | 2 |
| deform bones / weights | `Mesh` `primvars:skel:jointIndices` / `jointWeights` | 2 |
| SDEF C / R0 / R1 | `Mesh` `primvars:mmd:sdefC` / `sdefR0` / `sdefR1` | 2 |
| edge scale | `Mesh` `primvars:mmd:edgeScale` | 2 |
| face indices | `Mesh.faceVertexIndices` (winding reversed), `faceVertexCounts` (all 3) | 2 |

## Textures

| PMX field | USD | Phase |
| --- | --- | :---: |
| texture path | `SdfAssetPath` `@./<normalized>@` on the using material's slot, or none if unsafe; verbatim string in the material's customData | 2 |

## Materials

| PMX field | USD | Phase |
| --- | --- | :---: |
| name / English name | `Mtl` identifier; customData `mmd:sourceName` / `mmd:sourceEnglishName` | 2 |
| (table index) | `Mtl` customData `mmd:sourceIndex` (= draw order) | 2 |
| face count | `Mesh/<materialId>` `GeomSubset.indices` + `material:binding` | 2 |
| diffuse | `Mtl.mmd:material:diffuseColor`; `preview` `diffuseColor`/`opacity`; `mtlx` `base_color`/`alpha` | 3 |
| specular | `Mtl.mmd:material:specularColor` | 3 |
| specular power | `Mtl.mmd:material:specularPower`; `roughness` in both realizations | 3 |
| ambient | `Mtl.mmd:material:ambientColor` | 3 |
| flag `0x01` | `Mtl.mmd:material:doubleSided`; `Mesh.doubleSided` if any material that draws a face has it | 2 |
| flags `0x02`–`0x80` | `Mtl.mmd:material:groundShadow` … `drawLines` | 3 |
| edge color / size | `Mtl.mmd:material:edgeColor` / `edgeSize` | 3 |
| texture | `Mtl.mmd:material:texture` and customData `mmd:sourceTexturePath` | 2 |
| texture | base texture node in both realizations | 3 |
| sphere texture | `Mtl.mmd:material:sphereTexture` and customData `mmd:sourceSphereTexturePath` | 2 |
| sphere mode | `Mtl.mmd:material:sphereMode` | 3 |
| toon reference: a texture | `Mtl.mmd:material:toonTexture` and customData `mmd:sourceToonTexturePath` | 2 |
| toon reference + value | `Mtl.mmd:material:toonSource`; `sharedToonIndex` for a shared slot | 3 |
| memo | `Mtl` customData `mmd:sourceMemo` | 3 |

## Bones

| PMX field | USD | Phase |
| --- | --- | :---: |
| name / English name | joint path component (identifier); `Skel.mmd:bone:sourceName` / `sourceEnglishName` | 2 |
| (table index) | `Skel.mmd:bone:sourceIndex` | 2 |
| position | `Skel.bindTransforms` (translation), `Skel.restTransforms` (parent-relative) | 2 |
| parent | `Skel.joints` hierarchy | 2 |
| transform layer, deform-after-physics | `/Asset/rig` | 5 |
| tail | `/Asset/rig` | 5 |
| append rotation / translation + ratio | `/Asset/rig` | 5 |
| fixed axis, local axes | `/Asset/rig` | 5 |
| external parent | `/Asset/rig` | 5 |
| IK target, loops, limit, links | `/Asset/rig` | 5 |
| rotatable / translatable / visible / operable flags | `/Asset/rig` | 5 |

## Morphs

| PMX field | USD | Phase |
| --- | --- | :---: |
| name / English name / index | `Morph` identifier; customData provenance | 4 |
| panel | `Morph.mmd:morph:panel` | 4 |
| type | `Morph.mmd:morph:type` | 4 |
| vertex offsets | `Morph` as `UsdSkelBlendShape`: `offsets`, `pointIndices`; `Mesh` `skel:blendShapes` / `skel:blendShapeTargets`. Without bones: `Morph.mmd:morph:offsets`, `.mmd:morph:pointIndices` | 4 |
| group / flip members | `Morph.mmd:morph:members` (rel) + `.mmd:morph:weights` | 4 |
| bone offsets | `Morph.mmd:morph:joints`, `.translations`, `.rotations` | 4 |
| UV offsets | `Morph.mmd:morph:pointIndices`, `.uvOffsets` (raw) | 4 |
| material offsets | `Morph.mmd:morph:materialIndices` (`−1` = all), `.materialOperations`, and one array per modulated value | 4 |
| impulse offsets | `Morph.mmd:morph:rigidBodyIndices` (source table), `.impulseLocal`, `.velocities`, `.torques` | 4 |

## Display frames

| PMX field | USD | Phase |
| --- | --- | :---: |
| all | not authored in contract v1 (kept in the canonical model) | — |

## Physics

| PMX field | USD | Phase |
| --- | --- | :---: |
| rigid body | `/Asset/physics/rigidBodies/<id>`: `UsdPhysics` where it matches, `mmd:physics:*` otherwise | 6 |
| joint | `/Asset/physics/joints/<id>`: `UsdPhysics` where it matches, `mmd:physics:*` otherwise | 6 |
| soft body (2.1) | not authored (`MMD_PHYSICS_SOFT_BODY_UNSUPPORTED`) | — |
