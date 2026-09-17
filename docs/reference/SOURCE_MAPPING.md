# Source mapping

Where each PMX field lands on the stage — a lookup table, not a contract. The
decisions behind each row are in [PMX_CONTRACT.md](../design/PMX_CONTRACT.md)
(source → canonical) and [STAGE_CONTRACT.md](../design/STAGE_CONTRACT.md) /
[MATERIAL_POLICY.md](../design/MATERIAL_POLICY.md) (canonical → USD); when
this table disagrees with them, they win and this table is the bug.

Status (2026-09-17): **rows of Phases 0–6 are implemented; the rest are
intended.** The Phase column says when a row became true or is expected to;
[CAPABILITY_MATRIX.md](CAPABILITY_MATRIX.md) says which claims a fixture
backs.

Paths are abbreviated: `Mesh` is `/Asset/geo/Mesh`, `Skel` is
`/Asset/skel/Skeleton`, `Mtl` is `/Asset/mtl/<materialId>`, `Morph` is
`/Asset/morph/<morphId>`, `Bones` is `/Asset/rig/Bones`, `IK` is
`/Asset/rig/ik/<boneId>`, `Body` is `/Asset/physics/rigidBodies/<rigidBodyId>`,
`Joint` is `/Asset/physics/joints/<jointId>`. Every joint index is canonical.

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
| transform layer, deform-after-physics | `Bones.mmd:rig:transformLayers`, `deformAfterPhysics` | 5 |
| tail | `Bones.mmd:rig:tailJoints` (a bone) or `tailOffsets` (an offset) | 5 |
| append rotation / translation + ratio, local append | `Bones.mmd:rig:appendSources`, `appendRatios`, `appendRotation`, `appendTranslation`, `appendLocal` | 5 |
| fixed axis, local axes | `Bones.mmd:rig:hasFixedAxis` / `fixedAxes`, `hasLocalAxes` / `localAxesX` / `localAxesZ` | 5 |
| external parent | `Bones.mmd:rig:hasExternalParent` / `externalParentKeys` | 5 |
| IK bone, target, loops, limit | one `IK` prim: `mmd:rig:joint`, `effector`, `loopCount`, `limitAngle` | 5 |
| IK links | `IK.mmd:rig:linkJoints`, `linkHasLimits`, `linkLowerLimits`, `linkUpperLimits` | 5 |
| rotatable / translatable / visible / operable flags | `Bones.mmd:rig:rotatable`, `translatable`, `visible`, `operable` | 5 |

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
| impulse offsets | `Morph.mmd:morph:rigidBodyIndices` (the order of `Body`), `.impulseLocal`, `.velocities`, `.torques` | 4 |

## Display frames

| PMX field | USD | Phase |
| --- | --- | :---: |
| all | not authored in contract v1 (kept in the canonical model) | — |

## Physics

| PMX field | USD | Phase |
| --- | --- | :---: |
| rigid body name, English name, (table index) | `Body` customData provenance | 6 |
| bone | `Body.mmd:physics:bone` | 6 |
| group, non-collision mask | `Body.mmd:physics:collisionGroup`, `collisionMask` | 6 |
| shape, size | `Body.mmd:physics:shape`, `size`; `Body/collider` (`Sphere`, `Cube`, `Capsule`) | 6 |
| position, rotation | `Body.xformOp:translate`, `xformOp:orient` | 6 |
| mass | `Body.mmd:physics:mass`; `physics:mass` when positive | 6 |
| linear and angular damping, restitution, friction | `Body.mmd:physics:linearDamping`, `angularDamping`, `restitution`, `friction` | 6 |
| physics mode | `Body.mmd:physics:mode`; `physics:kinematicEnabled` | 6 |
| joint name, English name, (table index) | `Joint` customData provenance | 6 |
| joint type | `Joint.mmd:physics:type`; a `PhysicsJoint` for the 6-DOF types | 6 |
| rigid bodies A and B | `Joint.mmd:physics:rigidBodyA`, `rigidBodyB`; `physics:body0`, `body1` | 6 |
| position, rotation | `Joint.mmd:physics:position`, `orientation`; `physics:localPos0/1`, `localRot0/1` | 6 |
| translation and rotation limits | `Joint.mmd:physics:translationLowerLimit` … `rotationUpperLimit`; `limit:<axis>:physics:low/high` | 6 |
| translation and rotation springs | `Joint.mmd:physics:translationSpring`, `rotationSpring` | 6 |
| soft body (2.1) | not authored (`MMD_PHYSICS_SOFT_BODY_UNSUPPORTED`) | — |
