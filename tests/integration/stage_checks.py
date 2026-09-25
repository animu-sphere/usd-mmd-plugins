# SPDX-License-Identifier: Apache-2.0
"""Shared assertions for the integration tests.

Each check is one line of the stage contract's validation checklist
(docs/design/STAGE_CONTRACT.md §14) that the current Phase can reach, held
against what fixtures.json says the stage of each fixture must be. They
assert meaning, not text: codes, types, values -- never messages.
"""

from __future__ import annotations

import json
import pathlib

from pxr import Gf, Kind, Sdf, Usd, UsdGeom, UsdPhysics, UsdShade, UsdSkel, UsdValidation, Vt

STAGE_CONTRACT_VERSION = 2
# The canonical material values MmdMaterialAPI declares that a PMX material
# may leave unauthored; every other one is authored (MATERIAL_POLICY.md §4.1).
CONDITIONAL_MATERIAL_INPUTS = {"texture", "sphereTexture", "toonTexture", "sharedToonIndex"}
WEIGHT_TOLERANCE = 1e-5

_VALIDATORS = None


def validators() -> list:
    """Every validator OpenUSD registers -- UsdGeom's, UsdShade's, UsdSkel's
    and the rest -- loaded once: what `usdchecker` runs."""
    global _VALIDATORS
    if _VALIDATORS is None:
        _VALIDATORS = UsdValidation.ValidationRegistry().GetOrLoadAllValidators()
        assert _VALIDATORS, "OpenUSD registers no validator"
    return _VALIDATORS


def load_manifest(fixtures: pathlib.Path) -> dict[str, dict]:
    return json.loads((fixtures / "fixtures.json").read_text(encoding="utf-8"))


def pmx_format() -> Sdf.FileFormat:
    fmt = Sdf.FileFormat.FindByExtension("pmx")
    assert fmt is not None, "no file format is registered for .pmx"
    assert fmt.formatId == "pmx", fmt.formatId
    return fmt


def recorded_codes(prim: Usd.Prim) -> list[str]:
    """The codes of /Asset's mmd:diagnostics, in emission order."""
    recorded = prim.GetCustomDataByKey("mmd:diagnostics")
    return [entry.split(":", 1)[0] for entry in (recorded or [])]


class Checker:
    def __init__(self, stage: Usd.Stage, expectation: dict, where: str) -> None:
        self.stage = stage
        self.expectation = expectation
        self.shape = expectation["stage"]
        self.where = where

    def expect(self, condition: bool, what: str) -> None:
        assert condition, f"{self.where}: {what}"

    def run(self) -> None:
        self.expect(self.stage is not None, "the stage did not open")
        self.metadata()
        mesh = self.mesh()
        self.materials(mesh)
        self.skeleton(mesh)
        self.morphs(mesh)
        self.rig()
        self.physics()
        for prim in self.stage.Traverse():
            for attr in prim.GetAttributes():
                self.expect(attr.GetNumTimeSamples() == 0,
                            f"{attr.GetPath()} has time samples")
        self.validation()

    def validation(self) -> None:
        """OpenUSD's own rules for what the stage uses, beside the contract's.
        The one error a fixture may cause is an unresolvable texture, and then
        exactly for the textures fixtures.json says do not resolve -- the
        validator and the stage agree on which files are missing."""
        unresolved = {slot["asset"][2:] for m in self.shape["materials"]
                      for slot in m["textures"].values()
                      if slot["asset"] is not None and not slot["resolves"]}
        reported = set()
        others = []
        for error in UsdValidation.ValidationContext(validators()).Validate(self.stage):
            text = error.GetErrorAsString()
            if "UnresolvableDependency" in text:
                named = [path for path in unresolved if path in text]
                if len(named) == 1:
                    reported.add(named[0])
                    continue
            others.append(text)
        self.expect(not others, "OpenUSD validation fails: " + "; ".join(others))
        self.expect(reported == unresolved,
                    f"OpenUSD finds {sorted(reported)} unresolvable, fixtures.json "
                    f"expects {sorted(unresolved)}")

    # --- /Asset and the stage (§2, §4, §5, §6) ---------------------------------

    def metadata(self) -> None:
        stage, expect = self.stage, self.expect
        layer = stage.GetRootLayer()
        expect(layer.defaultPrim == "Asset", f"defaultPrim is {layer.defaultPrim!r}")
        expect(UsdGeom.GetStageUpAxis(stage) == UsdGeom.Tokens.y,
               f"upAxis is {UsdGeom.GetStageUpAxis(stage)!r}")
        expect(UsdGeom.GetStageMetersPerUnit(stage) == 1.0,
               f"metersPerUnit is {UsdGeom.GetStageMetersPerUnit(stage)!r}")
        expect(not layer.HasTimeCodesPerSecond(), "timeCodesPerSecond is authored")

        asset = stage.GetPrimAtPath("/Asset")
        expect(asset.IsValid(), "/Asset does not exist")
        expect(stage.GetDefaultPrim() == asset, "the default prim is not /Asset")
        # The SkelRoot exactly when the model has bones (§4.1).
        wanted = "SkelRoot" if self.shape["skinned"] else "Xform"
        expect(asset.GetTypeName() == wanted,
               f"/Asset is a {asset.GetTypeName()}, expected {wanted}")
        expect(Usd.ModelAPI(asset).GetKind() == Kind.Tokens.component,
               f"/Asset kind is {Usd.ModelAPI(asset).GetKind()!r}")
        # A scope only when it has children, in the contract's order.
        scopes = [name for name, present in (
            ("geo", self.shape["mesh"] is not None),
            ("mtl", bool(self.shape["materials"])),
            ("skel", self.shape["skinned"]),
            ("morph", bool(self.shape["morphs"])),
            ("rig", self.shape["rig"] is not None),
            ("physics", self.shape["physics"] is not None)) if present]
        children = [child.GetName() for child in asset.GetChildren()]
        expect(children == scopes, f"/Asset's children are {children}, expected {scopes}")
        for name in scopes:
            expect(asset.GetChild(name).GetTypeName() == "Scope", f"/Asset/{name} is no Scope")

        custom = asset.GetCustomDataByKey
        expect(custom("mmd:stageContractVersion") == STAGE_CONTRACT_VERSION,
               f"mmd:stageContractVersion is not {STAGE_CONTRACT_VERSION}")
        expect(custom("mmd:sourceFormat") == "PMX", "mmd:sourceFormat is not PMX")
        expect(custom("mmd:sourceVersion") == self.expectation["sourceVersion"],
               f"mmd:sourceVersion is {custom('mmd:sourceVersion')!r}")
        # The model name survives byte-exact, less its U+0000 padding.
        name = self.expectation["modelName"].rstrip("\0")
        expect(custom("mmd:sourceName") == name,
               f"mmd:sourceName is {custom('mmd:sourceName')!r}, expected {name!r}")
        for key in ("mmd:sourceEnglishName", "mmd:sourceComment", "mmd:sourceEnglishComment"):
            expect(asset.HasCustomDataKey(key), f"/Asset has no {key}")

        codes = recorded_codes(asset)
        expect(codes == self.expectation["diagnostics"],
               f"mmd:diagnostics records {codes}, expected {self.expectation['diagnostics']}")
        if not self.expectation["diagnostics"]:
            expect(not asset.HasCustomDataKey("mmd:diagnostics"),
                   "an empty mmd:diagnostics is authored")

    # --- /Asset/geo (§8, §9.4) ------------------------------------------------------

    def mesh(self) -> UsdGeom.Mesh | None:
        want = self.shape["mesh"]
        prim = self.stage.GetPrimAtPath("/Asset/geo/Mesh")
        if want is None:
            self.expect(not prim.IsValid(), "a mesh is authored for a model with no vertices")
            return None
        expect = self.expect
        mesh = UsdGeom.Mesh(prim)
        expect(bool(mesh), "/Asset/geo/Mesh is not a Mesh")
        expect(mesh.GetSubdivisionSchemeAttr().Get() == UsdGeom.Tokens.none,
               "subdivisionScheme is not none")
        counts = mesh.GetFaceVertexCountsAttr().Get()
        expect(len(counts) == want["faces"] and all(c == 3 for c in counts),
               f"faceVertexCounts are {list(counts)}")
        indices = mesh.GetFaceVertexIndicesAttr().Get()
        points = mesh.GetPointsAttr().Get()
        expect(len(points) == want["points"], f"{len(points)} points")
        expect(len(indices) == 3 * want["faces"]
               and all(0 <= i < len(points) for i in indices), "a face names no point")
        expect(bool(mesh.GetDoubleSidedAttr().Get()) == want["doubleSided"],
               f"doubleSided is {mesh.GetDoubleSidedAttr().Get()}")
        expect(mesh.GetExtentAttr().HasAuthoredValue(), "no extent")

        normals = mesh.GetNormalsAttr().Get()
        expect(len(normals) == len(points)
               and mesh.GetNormalsInterpolation() == UsdGeom.Tokens.vertex,
               "normals are not one per vertex")
        api = UsdGeom.PrimvarsAPI(mesh)
        st = api.GetPrimvar("st")
        expect(st.GetInterpolation() == UsdGeom.Tokens.vertex
               and st.GetTypeName() == Sdf.ValueTypeNames.TexCoord2fArray,
               "primvars:st is not a vertex texCoord2f[]")
        for k in range(1, 5):
            present = api.HasPrimvar(f"mmd:uv{k}")
            expect(present == (k <= want["additionalUv"]),
                   f"primvars:mmd:uv{k} is {'' if present else 'not '}authored")
        expect(api.HasPrimvar("mmd:edgeScale"), "no primvars:mmd:edgeScale")

        # One vertex through the conversion: meters, Z mirrored, v flipped.
        v1 = want["vertex1"]
        i = min(1, len(points) - 1)
        expect(list(points[i]) == v1["point"], f"point {i} is {points[i]}, expected {v1['point']}")
        expect(list(normals[i]) == v1["normal"], f"normal {i} is {normals[i]}")
        expect(list(st.Get()[i]) == v1["st"], f"st {i} is {st.Get()[i]}")
        return mesh

    # --- /Asset/mtl and the material subsets (§8.1, §10; MATERIAL §4) --------------

    def materials(self, mesh: UsdGeom.Mesh | None) -> None:
        expect = self.expect
        materials = self.shape["materials"]
        for m in materials:
            path = f"/Asset/mtl/{m['id']}"
            prim = self.stage.GetPrimAtPath(path)
            expect(prim.IsValid() and prim.GetTypeName() == "Material", f"no Material {path}")
            custom = prim.GetCustomDataByKey
            expect(custom("mmd:sourceName") == m["name"]
                   and custom("mmd:sourceEnglishName") == m["englishName"]
                   and custom("mmd:sourceIndex") == m["sourceIndex"],
                   f"{path} provenance is {prim.GetCustomData()}")
            self.canonical_inputs(prim, path)
            expect(prim.GetAttribute("inputs:mmd:material:doubleSided").Get() == m["doubleSided"],
                   f"{path} inputs:mmd:material:doubleSided")
            self.material_graphs(prim, path, m)
            for slot, key in (("texture", "mmd:sourceTexturePath"),
                              ("sphereTexture", "mmd:sourceSphereTexturePath"),
                              ("toonTexture", "mmd:sourceToonTexturePath")):
                self.texture_slot(prim, slot, key, m["textures"].get(slot))

        if mesh is None:
            return
        subsets = UsdGeom.Subset.GetGeomSubsets(mesh, UsdGeom.Tokens.face,
                                                UsdShade.Tokens.materialBind)
        drawn = [m for m in materials if m["faces"][1] > 0]
        names = [s.GetPrim().GetName() for s in subsets]
        expect(names == [m["id"] for m in drawn],
               f"materialBind subsets are {names}, expected {[m['id'] for m in drawn]}")
        for subset, m in zip(subsets, drawn):
            first, count = m["faces"]
            expect(list(subset.GetIndicesAttr().Get()) == list(range(first, first + count)),
                   f"subset {m['id']} does not hold faces {first}..{first + count - 1}")
            bound = UsdShade.MaterialBindingAPI(subset.GetPrim()).GetDirectBinding()
            expect(bound.GetMaterialPath() == Sdf.Path(f"/Asset/mtl/{m['id']}")
                   and bool(bound.GetMaterial()),
                   f"subset {m['id']} is bound to {bound.GetMaterialPath()}")
        family = UsdGeom.Subset.GetFamilyType(mesh, UsdShade.Tokens.materialBind)
        want = self.shape["mesh"]["familyType"]
        if want is not None:
            expect(family == want, f"the materialBind family is {family!r}, expected {want!r}")
            valid, reason = UsdGeom.Subset.ValidateFamily(
                mesh, UsdGeom.Tokens.face, UsdShade.Tokens.materialBind)
            expect(valid, f"the materialBind family is not valid: {reason}")

    def canonical_inputs(self, prim: Usd.Prim, path: str) -> None:
        """MmdMaterialAPI applied, and its canonical values authored as
        Material interface inputs with the schema's types and variability;
        no stage-contract v1 name beside them (MATERIAL_POLICY.md §4.1, §14)."""
        expect = self.expect
        expect(prim.HasAPI("MmdMaterialAPI"),
               f"{path} does not apply MmdMaterialAPI: {prim.GetAppliedSchemas()}")
        definition = Usd.SchemaRegistry().FindAppliedAPIPrimDefinition("MmdMaterialAPI")
        expect(definition is not None, "MmdMaterialAPI is not registered")
        material = UsdShade.Material(prim)
        toon_source = prim.GetAttribute("inputs:mmd:material:toonSource").Get()
        for name in definition.GetPropertyNames():
            short = name.removeprefix("inputs:mmd:material:")
            attr = prim.GetAttribute(name)
            if short == "sharedToonIndex":
                required = toon_source == "shared"
                expect(attr.HasAuthoredValue() == required,
                       f"{path}.{name} is {'not ' if required else ''}authored for "
                       f"toonSource {toon_source!r}")
            elif short not in CONDITIONAL_MATERIAL_INPUTS:
                expect(attr.HasAuthoredValue(), f"{path}.{name} is not authored")
            if not attr.HasAuthoredValue():
                continue
            spec = definition.GetAttributeDefinition(name)
            expect(attr.GetTypeName() == spec.GetTypeName()
                   and attr.GetVariability() == spec.GetVariability(),
                   f"{path}.{name} is {attr.GetTypeName()} {attr.GetVariability()}, "
                   f"the schema declares {spec.GetTypeName()} {spec.GetVariability()}")
            expect(bool(material.GetInput(f"mmd:material:{short}")),
                   f"{path}.{name} is not a Material input")
        legacy = [a.GetName() for a in prim.GetAttributes()
                  if a.GetName().startswith("mmd:material:")]
        expect(not legacy, f"{path} still authors stage-contract v1 names {legacy}")

    def material_graphs(self, material: Usd.Prim, path: str, want: dict) -> None:
        """Validate the VRM-like unlit portable material realizations."""
        expect = self.expect

        def child(name: str, type_name: str) -> Usd.Prim:
            prim = self.stage.GetPrimAtPath(f"{path}/{name}")
            expect(prim.IsValid() and prim.GetTypeName() == type_name,
                   f"{path}/{name} is not a {type_name}")
            return prim

        def shader_id(prim: Usd.Prim, expected: str) -> None:
            got = prim.GetAttribute("info:id").Get()
            expect(str(got) == expected, f"{prim.GetPath()} info:id is {got!r}")

        def connections(prim: Usd.Prim, output: str) -> list[str]:
            attr = prim.GetAttribute(f"outputs:{output}")
            return [str(connection) for connection in attr.GetConnections()] if attr else []

        preview = child("preview", "NodeGraph")
        preview_surface = child("preview/surface", "Shader")
        shader_id(preview_surface, "UsdPreviewSurface")
        expect(preview_surface.GetAttribute("inputs:useSpecularWorkflow").Get() == 0,
               f"{path}/preview/surface is not using the unlit path")
        expect(preview_surface.GetAttribute("inputs:diffuseColor").Get() == (0.0, 0.0, 0.0),
               f"{path}/preview/surface diffuseColor is not black")
        expect(preview_surface.GetAttribute("inputs:emissiveColor").IsValid(),
               f"{path}/preview/surface has no emissiveColor")
        expect(connections(preview, "surface") ==
               [f"{path}/preview/surface.outputs:surface"],
               f"{path}/preview output is not connected to its surface shader")
        expect(connections(material, "surface") == [f"{path}/preview.outputs:surface"],
               f"{path} surface output is not connected to preview")

        mtlx = child("mtlx", "NodeGraph")
        mtlx_surface = child("mtlx/surface", "Shader")
        shader_id(mtlx_surface, "ND_gltf_pbr_surfaceshader")
        expect(mtlx_surface.GetAttribute("inputs:base_color").Get() == (0.0, 0.0, 0.0),
               f"{path}/mtlx/surface base_color is not black")
        expect(mtlx_surface.GetAttribute("inputs:emissive").IsValid(),
               f"{path}/mtlx/surface has no emissive")
        expect(mtlx_surface.GetAttribute("inputs:specular").Get() == 0.0,
               f"{path}/mtlx/surface retains a lit specular response")
        expect(mtlx_surface.GetAttribute("inputs:roughness").Get() == 1.0,
               f"{path}/mtlx/surface is not maximally rough")
        expect(mtlx_surface.GetAttribute("inputs:alpha_mode").IsValid(),
               f"{path}/mtlx/surface has no alpha_mode")
        expect(connections(mtlx, "surface") ==
               [f"{path}/mtlx/surface.outputs:surface"],
               f"{path}/mtlx output is not connected to its surface shader")
        expect(connections(material, "mtlx:surface") == [f"{path}/mtlx.outputs:surface"],
               f"{path} mtlx surface output is not connected to mtlx")
        config = material.GetAttribute("config:mtlx:version")
        expect(config.IsValid() and config.Get() == "1.39",
               f"{path} has no MaterialX 1.39 config")

        diffuse = material.GetAttribute("inputs:mmd:material:diffuseColor")
        diffuse_color = diffuse.Get()
        expected_alpha_mode = 2 if want["textures"].get("texture") is not None \
            or diffuse_color[3] < 1.0 else 0
        expect(mtlx_surface.GetAttribute("inputs:alpha_mode").Get() == expected_alpha_mode,
               f"{path}/mtlx/surface alpha_mode is not {expected_alpha_mode}")

        def reads(graph: str, node: str, input_name: str, canonical: Usd.Attribute) -> None:
            """The node's input is connected to its graph's interface input,
            which is connected to the Material's canonical input: the value
            the realization reads is the canonical one (MATERIAL §2, §3)."""
            where = f"{path}/{graph}/{node}.inputs:{input_name}"
            node_input = UsdShade.Input(
                self.stage.GetPrimAtPath(f"{path}/{graph}/{node}").GetAttribute(
                    f"inputs:{input_name}"))
            expect(bool(node_input), f"{where} does not exist")
            sources, _ = node_input.GetConnectedSources()
            expect(len(sources) == 1
                   and sources[0].source.GetPath() == Sdf.Path(f"{path}/{graph}"),
                   f"{where} is not connected to its graph's interface")
            producers = node_input.GetValueProducingAttributes(False)
            expect([a.GetPath() for a in producers] == [canonical.GetPath()],
                   f"{where} reads {[str(a.GetPath()) for a in producers]}, "
                   f"expected {canonical.GetPath()}")

        # Both graphs read diffuse from the canonical input, except the
        # untextured preview (MAT-O6), which copies it exactly.
        for node, node_id in (("baseColorSplit", "ND_separate4_color4"),
                              ("baseColorRgb", "ND_combine3_color3")):
            shader_id(child(f"mtlx/{node}", "Shader"), node_id)
        texture_want = want["textures"].get("texture")
        textured = texture_want is not None and texture_want["asset"] is not None
        if textured:
            texture = material.GetAttribute("inputs:mmd:material:texture")
            preview_texture = child("preview/baseTexture", "Shader")
            shader_id(preview_texture, "UsdUVTexture")
            expect(preview_texture.GetAttribute("inputs:sourceColorSpace").Get() == "sRGB",
                   f"{path}/preview/baseTexture is not marked sRGB")
            reads("preview", "baseTexture", "scale", diffuse)
            reads("preview", "baseTexture", "file", texture)
            for node, node_id in (("baseTexture", "ND_image_color4"),
                                  ("baseColorFactor", "ND_multiply_color4")):
                shader_id(child(f"mtlx/{node}", "Shader"), node_id)
            reads("mtlx", "baseColorFactor", "in2", diffuse)
            reads("mtlx", "baseTexture", "file", texture)
        else:
            expect(not self.stage.GetPrimAtPath(f"{path}/preview/baseTexture").IsValid(),
                   f"{path} has a preview texture node without a base texture")
            expect(not self.stage.GetPrimAtPath(f"{path}/mtlx/baseTexture").IsValid(),
                   f"{path} has an mtlx texture node without a base texture")
            emissive = preview_surface.GetAttribute("inputs:emissiveColor")
            opacity = preview_surface.GetAttribute("inputs:opacity")
            expect(not emissive.HasAuthoredConnections()
                   and not opacity.HasAuthoredConnections()
                   and tuple(emissive.Get()) == tuple(diffuse_color)[:3]
                   and opacity.Get() == diffuse_color[3],
                   f"{path}/preview/surface does not copy diffuse {diffuse_color}")
            reads("mtlx", "baseColorSplit", "in", diffuse)

    def texture_slot(self, prim: Usd.Prim, slot: str, key: str, want: dict | None) -> None:
        """The verbatim source path as provenance whenever the slot names a
        texture; the anchored asset path only when it is safe; and it
        resolves exactly when the file is beside the fixture (TEXT §7)."""
        attr = prim.GetAttribute(f"inputs:mmd:material:{slot}")
        where = f"{prim.GetPath()}.inputs:mmd:material:{slot}"
        if want is None:
            self.expect(not attr.HasAuthoredValue(), f"{where} is authored for an empty slot")
            self.expect(not prim.HasCustomDataKey(key), f"{prim.GetPath()} has {key}")
            return
        self.expect(prim.GetCustomDataByKey(key) == want["source"],
                    f"{prim.GetPath()} {key} is {prim.GetCustomDataByKey(key)!r}")
        if want["asset"] is None:
            self.expect(not attr.HasAuthoredValue(), f"{where} is authored for an unsafe path")
            return
        value = attr.Get()
        self.expect(attr.GetTypeName() == Sdf.ValueTypeNames.Asset
                    and value.path == want["asset"],
                    f"{where} is {value!r}, expected @{want['asset']}@")
        # The encoding a connected texture node reads, as OpenUSD names it.
        self.expect(attr.GetColorSpace() == "srgb_rec709_scene",
                    f"{where} colorSpace is {attr.GetColorSpace()!r}")
        self.expect(bool(value.resolvedPath) == want["resolves"],
                    f"{where} resolves to {value.resolvedPath!r}")

    # --- /Asset/morph (§11) -----------------------------------------------------------

    def morphs(self, mesh: UsdGeom.Mesh | None) -> None:
        """Every PMX morph is one prim, in morph-table order: a blend shape
        for a vertex morph the skeleton can drive, a typeless prim carrying
        its declarative semantics otherwise. Nothing here is evaluated."""
        expect = self.expect
        want = self.shape["morphs"]
        scope = self.stage.GetPrimAtPath("/Asset/morph")
        if not want:
            expect(not scope.IsValid(), "a morph scope is authored for a model with no morphs")
            return
        names = [child.GetName() for child in scope.GetChildren()]
        expect(names == [m["id"] for m in want],
               f"/Asset/morph holds {names}, expected {[m['id'] for m in want]}")

        blend_shapes = []
        for m in want:
            path = f"/Asset/morph/{m['id']}"
            prim = self.stage.GetPrimAtPath(path)
            # A blend shape needs a SkelRoot, which a boneless model is not
            # (§4.1), and a mesh to name it (§11.1): without either, the
            # vertex morph is preserved on a typeless prim.
            drivable = (m["type"] == "vertex" and self.shape["skinned"]
                        and self.shape["mesh"] is not None)
            type_name = "BlendShape" if drivable else ""
            expect(prim.GetTypeName() == type_name,
                   f"{path} is a {prim.GetTypeName()!r}, expected {type_name!r}")
            custom = prim.GetCustomDataByKey
            expect(custom("mmd:sourceName") == m["name"]
                   and custom("mmd:sourceEnglishName") == m["englishName"]
                   and custom("mmd:sourceIndex") == m["sourceIndex"],
                   f"{path} provenance is {prim.GetCustomData()}")
            for key in ("type", "panel"):
                got = prim.GetAttribute(f"mmd:morph:{key}").Get()
                expect(got == m[key], f"{path} mmd:morph:{key} is {got!r}, expected {m[key]!r}")

            if drivable:
                shape = UsdSkel.BlendShape(prim)
                expect(bool(shape), f"{path} is not a BlendShape")
                self.array(shape.GetPointIndicesAttr(), m["pointIndices"], path, "pointIndices")
                self.vectors(shape.GetOffsetsAttr(), m["offsets"], path, "offsets")
                blend_shapes.append(m["id"])
            else:
                self.morph_payload(prim, path, m)

        if mesh is None:
            return
        binding = UsdSkel.BindingAPI(mesh)
        authored = list(binding.GetBlendShapesAttr().Get() or [])
        expect(authored == blend_shapes,
               f"the mesh names blend shapes {authored}, expected {blend_shapes}")
        targets = [str(target) for target in binding.GetBlendShapeTargetsRel().GetTargets()]
        expect(targets == [f"/Asset/morph/{name}" for name in blend_shapes],
               f"skel:blendShapeTargets is {targets}")
        # The binding resolves: UsdSkel finds every blend shape from the mesh
        # and can compute a subshape from it.
        query = UsdSkel.BlendShapeQuery(binding)
        resolved = [str(query.GetBlendShape(i).GetPrim().GetPath())
                    for i in range(query.GetNumBlendShapes())]
        expect(resolved == targets,
               f"UsdSkel resolves the blend shapes {resolved}, expected {targets}")
        self.deformation(mesh, query, blend_shapes)

    def deformation(self, mesh: UsdGeom.Mesh, query: UsdSkel.BlendShapeQuery,
                    blend_shapes: list[str]) -> None:
        """Each blend shape, driven to weight 1 by UsdSkel itself, moves the
        points its morph names and no others, by the offsets it authored. The
        importer authors no weight; a consumer supplies them."""
        by_id = {m["id"]: m for m in self.shape["morphs"]}
        points = mesh.GetPointsAttr().Get()
        offsets = query.ComputeSubShapePointOffsets()
        indices = query.ComputeBlendShapePointIndices()
        for i, name in enumerate(blend_shapes):
            weights = Vt.FloatArray([1.0 if k == i else 0.0
                                     for k in range(len(blend_shapes))])
            sub = query.ComputeSubShapeWeights(weights)
            deformed = Vt.Vec3fArray(list(points))
            self.expect(query.ComputeDeformedPoints(sub[0], sub[1], sub[2], indices,
                                                    offsets, deformed),
                        f"{name} does not deform the mesh")
            # The offsets are added in float, so the deformed point is
            # compared with the same sum rather than with the difference,
            # which would not recover the offset's bits.
            want = {v: points[v] + Gf.Vec3f(*o)
                    for v, o in zip(by_id[name]["pointIndices"], by_id[name]["offsets"])
                    if any(o)}
            moved = {v: deformed[v] for v in range(len(points))
                     if deformed[v] != points[v]}
            self.expect(sorted(moved) == sorted(want),
                        f"{name} moves points {sorted(moved)}, expected {sorted(want)}")
            for v, position in moved.items():
                self.expect(position == want[v],
                            f"{name} moves point {v} to {position}, expected {want[v]}")

    def morph_payload(self, prim: Usd.Prim, path: str, m: dict) -> None:
        """The declarative semantics of a morph that is not a blend shape
        (STAGE-O4): parallel `mmd:morph:*` arrays, and the members of a group
        or flip morph as a relationship."""
        kind = m["type"]
        if kind in ("group", "flip"):
            targets = [str(target) for target in
                       prim.GetRelationship("mmd:morph:members").GetTargets()]
            self.expect(targets == [f"/Asset/morph/{name}" for name in m["members"]],
                        f"{path} mmd:morph:members is {targets}")
            self.array(prim.GetAttribute("mmd:morph:weights"), m["weights"], path, "weights")
        elif kind == "vertex":
            self.array(prim.GetAttribute("mmd:morph:pointIndices"), m["pointIndices"],
                       path, "pointIndices")
            self.vectors(prim.GetAttribute("mmd:morph:offsets"), m["offsets"], path, "offsets")
        elif kind == "bone":
            self.array(prim.GetAttribute("mmd:morph:joints"), m["joints"], path, "joints")
            self.vectors(prim.GetAttribute("mmd:morph:translations"), m["translations"],
                         path, "translations")
            got = [[*q.GetImaginary(), q.GetReal()]
                   for q in prim.GetAttribute("mmd:morph:rotations").Get()]
            self.expect(got == m["rotations"], f"{path} mmd:morph:rotations is {got}")
        elif kind.startswith("uv"):
            self.array(prim.GetAttribute("mmd:morph:pointIndices"), m["pointIndices"],
                       path, "pointIndices")
            self.vectors(prim.GetAttribute("mmd:morph:uvOffsets"), m["uvOffsets"],
                         path, "uvOffsets")
        elif kind == "material":
            self.array(prim.GetAttribute("mmd:morph:materialIndices"), m["materialIndices"],
                       path, "materialIndices")
            self.array(prim.GetAttribute("mmd:morph:materialOperations"),
                       m["materialOperations"], path, "materialOperations")
            self.vectors(prim.GetAttribute("mmd:morph:diffuseColors"), m["diffuseColors"],
                         path, "diffuseColors")
        elif kind == "impulse":
            self.array(prim.GetAttribute("mmd:morph:rigidBodyIndices"), m["rigidBodyIndices"],
                       path, "rigidBodyIndices")
            self.array(prim.GetAttribute("mmd:morph:impulseLocal"), m["impulseLocal"],
                       path, "impulseLocal")
            self.vectors(prim.GetAttribute("mmd:morph:velocities"), m["velocities"],
                         path, "velocities")
            self.vectors(prim.GetAttribute("mmd:morph:torques"), m["torques"], path, "torques")

    def array(self, attr: Usd.Attribute, want: list, path: str, name: str) -> None:
        self.expect(attr.IsValid(), f"{path} has no {name}")
        got = [str(v) if isinstance(v, str) else v for v in (attr.Get() or [])]
        self.expect(got == want, f"{path} {name} is {got}, expected {want}")

    def vectors(self, attr: Usd.Attribute, want: list[list[float]], path: str,
                name: str) -> None:
        """A vector array, exactly: every value came through the one
        conversion and is the bits the generator computed (§6.2)."""
        self.expect(attr.IsValid(), f"{path} has no {name}")
        got = [list(v) for v in (attr.Get() or [])]
        self.expect(got == want, f"{path} {name} is {got}, expected {want}")

    # --- /Asset/rig (§12) --------------------------------------------------------------

    RIG_ARRAYS = (  # attribute on /Asset/rig/Bones, field of fixtures.json
        ("transformLayers", "transformLayer"), ("deformAfterPhysics", "deformAfterPhysics"),
        ("rotatable", "rotatable"), ("translatable", "translatable"),
        ("visible", "visible"), ("operable", "operable"),
        ("tailJoints", "tailJoint"), ("tailOffsets", "tailOffset"),
        ("appendSources", "appendSource"), ("appendRatios", "appendRatio"),
        ("appendRotation", "appendRotation"), ("appendTranslation", "appendTranslation"),
        ("appendLocal", "appendLocal"),
        ("hasFixedAxis", "hasFixedAxis"), ("fixedAxes", "fixedAxis"),
        ("hasLocalAxes", "hasLocalAxes"), ("localAxesX", "localAxisX"),
        ("localAxesZ", "localAxisZ"),
        ("hasExternalParent", "hasExternalParent"),
        ("externalParentKeys", "externalParentKey"),
    )

    def rig(self) -> None:
        """Every joint's control semantics as uniform arrays parallel to the
        Skeleton's joints, and one typeless prim per IK chain, every value
        exactly the bits the generator computed."""
        expect = self.expect
        want = self.shape["rig"]
        scope = self.stage.GetPrimAtPath("/Asset/rig")
        if want is None:
            expect(not scope.IsValid(), "a rig is authored for a model without bones")
            return
        children = [child.GetName() for child in scope.GetChildren()]
        wanted = ["Bones"] + (["ik"] if want["ikChains"] else [])
        expect(children == wanted, f"/Asset/rig holds {children}, expected {wanted}")

        bones = self.stage.GetPrimAtPath("/Asset/rig/Bones")
        expect(bones.GetTypeName() == "", f"/Asset/rig/Bones is a {bones.GetTypeName()!r}")
        joints = len(self.shape["joints"])
        for attribute, field in self.RIG_ARRAYS:
            attr = bones.GetAttribute(f"mmd:rig:{attribute}")
            expect(attr.IsValid() and attr.GetVariability() == Sdf.VariabilityUniform,
                   f"/Asset/rig/Bones has no uniform mmd:rig:{attribute}")
            got = attr.Get() or []
            expect(len(got) == joints, f"mmd:rig:{attribute} holds {len(got)} of {joints} joints")
            column = [control[field] for control in want["bones"]]
            if column and isinstance(column[0], list):
                self.vectors(attr, column, "/Asset/rig/Bones", attribute)
            else:
                self.array(attr, column, "/Asset/rig/Bones", attribute)

        if want["ikChains"]:
            ik = self.stage.GetPrimAtPath("/Asset/rig/ik")
            expect(ik.GetTypeName() == "Scope", "/Asset/rig/ik is no Scope")
            names = [child.GetName() for child in ik.GetChildren()]
            expect(names == [c["id"] for c in want["ikChains"]],
                   f"/Asset/rig/ik holds {names}, expected {[c['id'] for c in want['ikChains']]}")
        for chain in want["ikChains"]:
            path = f"/Asset/rig/ik/{chain['id']}"
            prim = self.stage.GetPrimAtPath(path)
            expect(prim.IsValid() and prim.GetTypeName() == "", f"{path} is no typeless prim")
            custom = prim.GetCustomDataByKey
            expect(custom("mmd:sourceName") == chain["name"]
                   and custom("mmd:sourceEnglishName") == chain["englishName"]
                   and custom("mmd:sourceIndex") == chain["sourceIndex"],
                   f"{path} provenance is {prim.GetCustomData()}")
            for name in ("joint", "effector", "loopCount", "limitAngle"):
                got = prim.GetAttribute(f"mmd:rig:{name}").Get()
                expect(got == chain[name], f"{path} mmd:rig:{name} is {got!r}")
            links = chain["links"]
            self.array(prim.GetAttribute("mmd:rig:linkJoints"),
                       [link["joint"] for link in links], path, "linkJoints")
            self.array(prim.GetAttribute("mmd:rig:linkHasLimits"),
                       [link["hasLimits"] for link in links], path, "linkHasLimits")
            self.vectors(prim.GetAttribute("mmd:rig:linkLowerLimits"),
                         [link["lower"] for link in links], path, "linkLowerLimits")
            self.vectors(prim.GetAttribute("mmd:rig:linkUpperLimits"),
                         [link["upper"] for link in links], path, "linkUpperLimits")
        self.reconstruction(want["sourceRelations"])

    def reconstruction(self, want: dict) -> None:
        """Phase 5's acceptance (DESIGN_POLICY.md §14): a consumer reconstructs
        every IK chain and append relation from the stage alone. This reads
        nothing but the stage -- the rig's joint indices, resolved through the
        Skeleton's own provenance -- and compares with what the source says."""
        skeleton = self.stage.GetPrimAtPath("/Asset/skel/Skeleton")
        source_of = list(skeleton.GetAttribute("mmd:bone:sourceIndex").Get())
        bones = self.stage.GetPrimAtPath("/Asset/rig/Bones")
        chains = []
        ik = self.stage.GetPrimAtPath("/Asset/rig/ik")
        for prim in (ik.GetChildren() if ik.IsValid() else []):
            attr = prim.GetAttribute
            chains.append([source_of[attr("mmd:rig:joint").Get()],
                           source_of[attr("mmd:rig:effector").Get()],
                           [source_of[j] for j in attr("mmd:rig:linkJoints").Get()]])
        chains.sort()
        self.expect(chains == sorted(want["ikChains"]),
                    f"the stage reconstructs IK chains {chains}, the source has "
                    f"{sorted(want['ikChains'])}")
        appends = sorted([source_of[j], source_of[s]] for j, s in
                         enumerate(bones.GetAttribute("mmd:rig:appendSources").Get()) if s != -1)
        self.expect(appends == sorted(want["appends"]),
                    f"the stage reconstructs appends {appends}, the source has "
                    f"{sorted(want['appends'])}")

    # --- /Asset/physics (§13) ----------------------------------------------------------

    BODY_ATTRIBUTES = ("bone", "shape", "collisionGroup", "collisionMask", "mass",
                       "linearDamping", "angularDamping", "restitution", "friction", "mode")
    JOINT_ATTRIBUTES = ("type", "rigidBodyA", "rigidBodyB")
    JOINT_VECTORS = ("translationLowerLimit", "translationUpperLimit", "rotationLowerLimit",
                     "rotationUpperLimit", "translationSpring", "rotationSpring")
    LIMIT_AXES = ("transX", "transY", "transZ", "rotX", "rotY", "rotZ")

    def uniform(self, prim: Usd.Prim, name: str):
        attr = prim.GetAttribute(f"mmd:physics:{name}")
        self.expect(attr.IsValid() and attr.GetVariability() == Sdf.VariabilityUniform,
                    f"{prim.GetPath()} has no uniform mmd:physics:{name}")
        value = attr.Get()
        return str(value) if isinstance(value, str) else value

    @staticmethod
    def quat(q) -> list[float]:
        return [*q.GetImaginary(), q.GetReal()]

    def provenance(self, prim: Usd.Prim, want: dict) -> None:
        custom = prim.GetCustomDataByKey
        self.expect(custom("mmd:sourceName") == want["name"]
                    and custom("mmd:sourceEnglishName") == want["englishName"]
                    and custom("mmd:sourceIndex") == want["sourceIndex"],
                    f"{prim.GetPath()} provenance is {prim.GetCustomData()}")

    def physics(self) -> None:
        """Every rigid body and joint: UsdPhysics where it matches, every PMX
        value as `mmd:physics:*` beside it, each the bits the generator
        computed -- and nothing that steps a simulation."""
        expect = self.expect
        want = self.shape["physics"]
        scope = self.stage.GetPrimAtPath("/Asset/physics")
        if want is None:
            expect(not scope.IsValid(), "a physics scope is authored for a model with no rigid body")
            return
        children = [child.GetName() for child in scope.GetChildren()]
        wanted = ["rigidBodies"] + (["joints"] if want["joints"] else [])
        expect(children == wanted, f"/Asset/physics holds {children}, expected {wanted}")
        for name in wanted:
            expect(scope.GetChild(name).GetTypeName() == "Scope",
                   f"/Asset/physics/{name} is no Scope")
        for prim in self.stage.Traverse():
            expect(prim.GetTypeName() != "PhysicsScene", f"{prim.GetPath()} is a PhysicsScene")

        bodies = self.stage.GetPrimAtPath("/Asset/physics/rigidBodies")
        names = [child.GetName() for child in bodies.GetChildren()]
        expect(names == [b["id"] for b in want["rigidBodies"]],
               f"/Asset/physics/rigidBodies holds {names}")
        for b in want["rigidBodies"]:
            self.rigid_body(b)

        if want["joints"]:
            joints = self.stage.GetPrimAtPath("/Asset/physics/joints")
            names = [child.GetName() for child in joints.GetChildren()]
            expect(names == [j["id"] for j in want["joints"]],
                   f"/Asset/physics/joints holds {names}")
        for j in want["joints"]:
            self.physics_joint(j)
        self.physics_recovery(want["sourceRelations"])

    def rigid_body(self, b: dict) -> None:
        expect = self.expect
        path = f"/Asset/physics/rigidBodies/{b['id']}"
        prim = self.stage.GetPrimAtPath(path)
        expect(prim.GetTypeName() == "Xform", f"{path} is a {prim.GetTypeName()!r}")
        expect(prim.HasAPI(UsdPhysics.RigidBodyAPI) and prim.HasAPI(UsdPhysics.MassAPI),
               f"{path} lacks PhysicsRigidBodyAPI or PhysicsMassAPI")
        self.provenance(prim, b)
        ops = UsdGeom.Xformable(prim).GetOrderedXformOps()
        expect([op.GetOpType() for op in ops] ==
               [UsdGeom.XformOp.TypeTranslate, UsdGeom.XformOp.TypeOrient],
               f"{path} xformOps are {[op.GetOpName() for op in ops]}")
        expect(list(ops[0].Get()) == b["position"], f"{path} is at {ops[0].Get()}")
        expect(self.quat(ops[1].Get()) == b["orientation"],
               f"{path} is turned {ops[1].Get()}, expected {b['orientation']}")
        body = UsdPhysics.RigidBodyAPI(prim)
        expect(body.GetKinematicEnabledAttr().Get() == b["kinematic"],
               f"{path} physics:kinematicEnabled")
        mass = UsdPhysics.MassAPI(prim).GetMassAttr()
        expect(mass.HasAuthoredValue() == (b["mass"] > 0.0)
               and (not mass.HasAuthoredValue() or mass.Get() == b["mass"]),
               f"{path} physics:mass is {mass.Get()}, the source's {b['mass']}")
        for name in self.BODY_ATTRIBUTES:
            got = self.uniform(prim, name)
            expect(got == b[name], f"{path} mmd:physics:{name} is {got!r}, expected {b[name]!r}")
        got = list(self.uniform(prim, "size"))
        expect(got == b["sizeFloat"], f"{path} mmd:physics:size is {got}")

        collider = prim.GetChild("collider")
        kind = {"sphere": "Sphere", "box": "Cube", "capsule": "Capsule"}[b["shape"]]
        expect(collider.GetTypeName() == kind, f"{path}/collider is a {collider.GetTypeName()!r}")
        expect(collider.HasAPI(UsdPhysics.CollisionAPI), f"{path}/collider has no CollisionAPI")
        expect(UsdGeom.Imageable(collider).GetPurposeAttr().Get() == UsdGeom.Tokens.guide,
               f"{path}/collider is not guide purpose")
        size = b["size"]
        if kind == "Sphere":
            expect(collider.GetAttribute("radius").Get() == size[0], f"{path}/collider radius")
        elif kind == "Capsule":
            capsule = UsdGeom.Capsule(collider)
            expect(capsule.GetRadiusAttr().Get() == size[0]
                   and capsule.GetHeightAttr().Get() == size[1]
                   and capsule.GetAxisAttr().Get() == UsdGeom.Tokens.y,
                   f"{path}/collider is not a Y capsule of radius {size[0]}, height {size[1]}")
        else:
            cube = UsdGeom.Cube(collider)
            scale = cube.GetOrderedXformOps()
            expect(cube.GetSizeAttr().Get() == 2.0 and len(scale) == 1
                   and list(scale[0].Get()) == size,
                   f"{path}/collider is not a cube of half extents {size}")

    def physics_joint(self, j: dict) -> None:
        expect = self.expect
        path = f"/Asset/physics/joints/{j['id']}"
        prim = self.stage.GetPrimAtPath(path)
        type_name = "PhysicsJoint" if j["usdPhysics"] else ""
        expect(prim.GetTypeName() == type_name,
               f"{path} is a {prim.GetTypeName()!r}, expected {type_name!r}")
        self.provenance(prim, j)
        for name in self.JOINT_ATTRIBUTES:
            got = self.uniform(prim, name)
            expect(got == j[name], f"{path} mmd:physics:{name} is {got!r}, expected {j[name]!r}")
        expect(list(self.uniform(prim, "position")) == j["position"],
               f"{path} mmd:physics:position")
        expect(self.quat(self.uniform(prim, "orientation")) == j["orientation"],
               f"{path} mmd:physics:orientation")
        for name in self.JOINT_VECTORS:
            got = list(self.uniform(prim, name))
            expect(got == j[name], f"{path} mmd:physics:{name} is {got}, expected {j[name]}")
        if not j["usdPhysics"]:
            expect(not prim.GetAppliedSchemas(), f"{path} applies {prim.GetAppliedSchemas()}")
            return
        joint = UsdPhysics.Joint(prim)
        for rel, body in ((joint.GetBody0Rel(), j["body0"]), (joint.GetBody1Rel(), j["body1"])):
            targets = [str(t) for t in rel.GetTargets()]
            expect(targets == [f"/Asset/physics/rigidBodies/{body}"],
                   f"{path} {rel.GetName()} is {targets}")
        expect(list(joint.GetLocalPos0Attr().Get()) == j["localPos0"]
               and list(joint.GetLocalPos1Attr().Get()) == j["localPos1"],
               f"{path} local positions are {joint.GetLocalPos0Attr().Get()}, "
               f"{joint.GetLocalPos1Attr().Get()}")
        expect(self.quat(joint.GetLocalRot0Attr().Get()) == j["localRot0"]
               and self.quat(joint.GetLocalRot1Attr().Get()) == j["localRot1"],
               f"{path} local rotations are {joint.GetLocalRot0Attr().Get()}, "
               f"{joint.GetLocalRot1Attr().Get()}")
        limited = {}
        for axis in self.LIMIT_AXES:
            if prim.HasAPI(UsdPhysics.LimitAPI, axis):
                limit = UsdPhysics.LimitAPI(prim, axis)
                limited[axis] = [limit.GetLowAttr().Get(), limit.GetHighAttr().Get()]
        expect(limited == j["limits"], f"{path} limits are {limited}, expected {j['limits']}")

    def physics_recovery(self, want: dict) -> None:
        """Phase 6's acceptance (DESIGN_POLICY.md §14): every rigid body and
        joint is recovered from the stage alone -- each body's bone through
        the Skeleton's own provenance, each joint's bodies through the
        UsdPhysics relationships where they are authored -- and compared with
        what the source says."""
        skeleton = self.stage.GetPrimAtPath("/Asset/skel/Skeleton")
        source_of = list(skeleton.GetAttribute("mmd:bone:sourceIndex").Get()) \
            if skeleton.IsValid() else []
        bodies = self.stage.GetPrimAtPath("/Asset/physics/rigidBodies").GetChildren()
        index_of = {str(b.GetPath()): b.GetCustomDataByKey("mmd:sourceIndex") for b in bodies}
        recovered = []
        for body in bodies:
            bone = body.GetAttribute("mmd:physics:bone").Get()
            recovered.append([body.GetCustomDataByKey("mmd:sourceIndex"),
                              source_of[bone] if bone != -1 else -1])
        self.expect(recovered == want["rigidBodies"],
                    f"the stage recovers rigid bodies {recovered}, the source has "
                    f"{want['rigidBodies']}")
        joints = self.stage.GetPrimAtPath("/Asset/physics/joints")
        recovered = []
        for prim in (joints.GetChildren() if joints.IsValid() else []):
            stored = [prim.GetAttribute("mmd:physics:rigidBodyA").Get(),
                      prim.GetAttribute("mmd:physics:rigidBodyB").Get()]
            if prim.GetTypeName() == "PhysicsJoint":
                joint = UsdPhysics.Joint(prim)
                pair = [index_of[str(joint.GetBody0Rel().GetTargets()[0])],
                        index_of[str(joint.GetBody1Rel().GetTargets()[0])]]
                self.expect(pair == stored,
                            f"{prim.GetPath()}: body0 and body1 disagree with mmd:physics")
            recovered.append([prim.GetCustomDataByKey("mmd:sourceIndex"), *stored])
        self.expect(recovered == want["joints"],
                    f"the stage recovers joints {recovered}, the source has {want['joints']}")

    # --- /Asset/skel and the skin binding (§9) ----------------------------------------

    def skeleton(self, mesh: UsdGeom.Mesh | None) -> None:
        expect = self.expect
        prim = self.stage.GetPrimAtPath("/Asset/skel/Skeleton")
        if not self.shape["skinned"]:
            expect(not prim.IsValid(), "a skeleton is authored for a model without bones")
            if mesh is not None:
                expect(not mesh.GetPrim().HasAPI(UsdSkel.BindingAPI),
                       "an unskinned mesh has SkelBindingAPI")
            return
        skeleton = UsdSkel.Skeleton(prim)
        expect(bool(skeleton), "/Asset/skel/Skeleton is not a Skeleton")
        joints = self.shape["joints"]
        tokens = list(skeleton.GetJointsAttr().Get())
        expect(tokens == [j["path"] for j in joints],
               f"joints are {tokens}, expected {[j['path'] for j in joints]}")
        valid, reason = UsdSkel.Topology(tokens).Validate()
        expect(valid, f"the joint topology is not valid: {reason}")
        expect(len(set(tokens)) == len(tokens), "two joints share a path")
        bind = skeleton.GetBindTransformsAttr().Get()
        rest = skeleton.GetRestTransformsAttr().Get()
        expect(len(bind) == len(tokens) and len(rest) == len(tokens),
               "bind or rest transforms are not one per joint")
        for matrix, joint in zip(bind, joints):
            expect(list(matrix.ExtractTranslation()) == joint["bind"],
                   f"{joint['path']} binds at {matrix.ExtractTranslation()}, "
                   f"expected {joint['bind']}")
            expect(matrix.ExtractRotationQuat().GetReal() == 1.0,
                   f"{joint['path']} has a bind rotation")
        # Every source name survives byte-exact, Japanese included (§9.3).
        for attribute, field in (("mmd:bone:sourceName", "name"),
                                 ("mmd:bone:sourceEnglishName", "englishName"),
                                 ("mmd:bone:sourceIndex", "sourceIndex")):
            got = list(prim.GetAttribute(attribute).Get())
            expect(got == [j[field] for j in joints], f"{attribute} is {got}")

        # The binding resolves: UsdSkel finds the mesh as a skinning target of
        # the skeleton, and the rest pose skins the mesh onto itself.
        cache = UsdSkel.Cache()
        root = UsdSkel.Root(self.stage.GetPrimAtPath("/Asset"))
        cache.Populate(root, Usd.PrimDefaultPredicate)
        expect(bool(cache.GetSkelQuery(skeleton)), "the skeleton query is not valid")
        bindings = cache.ComputeSkelBindings(root, Usd.PrimDefaultPredicate)
        if mesh is None:
            # A model with bones and no vertices authors no mesh (§8), so the
            # skeleton stands alone and skins nothing.
            skinned = [str(target.GetPrim().GetPath())
                       for binding in bindings for target in binding.GetSkinningTargets()]
            expect(not skinned, f"the skeleton skins {skinned} with no mesh")
            return
        expect(len(bindings) == 1, f"{len(bindings)} skeleton bindings")
        targets = [q.GetPrim().GetPath() for q in bindings[0].GetSkinningTargets()]
        expect(targets == [mesh.GetPath()], f"the skinning targets are {targets}")
        query = bindings[0].GetSkinningTargets()[0]
        expect(bool(query), "the mesh's skinning query is not valid")

        n = self.shape["mesh"]["influences"]
        binding = UsdSkel.BindingAPI(mesh)
        indices = binding.GetJointIndicesPrimvar()
        weights = binding.GetJointWeightsPrimvar()
        expect(indices.GetElementSize() == n and weights.GetElementSize() == n,
               f"influences are {indices.GetElementSize()} per vertex, expected {n}")
        index_values = list(indices.Get())
        weight_values = list(weights.Get())
        expect(all(0 <= i < len(tokens) for i in index_values), "a joint index is out of range")
        for v in range(len(weight_values) // n):
            total = sum(weight_values[v * n:(v + 1) * n])
            expect(abs(total - 1.0) <= WEIGHT_TOLERANCE,
                   f"vertex {v}'s weights sum to {total}")
        api = UsdGeom.PrimvarsAPI(mesh)
        expect(api.HasPrimvar("mmd:deformType"), "no primvars:mmd:deformType")
        for name in ("mmd:sdefC", "mmd:sdefR0", "mmd:sdefR1"):
            expect(api.HasPrimvar(name) == self.shape["mesh"]["sdef"],
                   f"primvars:{name} is {'' if api.HasPrimvar(name) else 'not '}authored")


def check_opened_stage(stage: Usd.Stage, expectation: dict, where: str) -> None:
    Checker(stage, expectation, where).run()
