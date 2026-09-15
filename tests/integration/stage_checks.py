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

from pxr import Kind, Sdf, Usd, UsdGeom, UsdShade, UsdSkel, UsdValidation

STAGE_CONTRACT_VERSION = 1
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
            ("skel", self.shape["skinned"])) if present]
        children = [child.GetName() for child in asset.GetChildren()]
        expect(children == scopes, f"/Asset's children are {children}, expected {scopes}")
        for name in scopes:
            expect(asset.GetChild(name).GetTypeName() == "Scope", f"/Asset/{name} is no Scope")

        custom = asset.GetCustomDataByKey
        expect(custom("mmd:stageContractVersion") == STAGE_CONTRACT_VERSION,
               "mmd:stageContractVersion is not 1")
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
            expect(prim.GetAttribute("mmd:material:doubleSided").Get() == m["doubleSided"],
                   f"{path} mmd:material:doubleSided")
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
        for input_name in ("mmd:material:diffuseColor", "mmd:material:specularColor",
                           "mmd:material:specularPower", "mmd:material:ambientColor",
                           "mmd:material:sphereMode", "mmd:material:toonSource"):
            expect(material.GetAttribute(input_name).IsValid(),
                   f"{path} has no preserved {input_name}")
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

        diffuse_color = material.GetAttribute("mmd:material:diffuseColor").Get()
        expected_alpha_mode = 2 if want["textures"].get("texture") is not None \
            or diffuse_color[3] < 1.0 else 0
        expect(mtlx_surface.GetAttribute("inputs:alpha_mode").Get() == expected_alpha_mode,
               f"{path}/mtlx/surface alpha_mode is not {expected_alpha_mode}")

        texture_want = want["textures"].get("texture")
        textured = texture_want is not None and texture_want["asset"] is not None
        if textured:
            preview_texture = child("preview/baseTexture", "Shader")
            shader_id(preview_texture, "UsdUVTexture")
            expect(preview_texture.GetAttribute("inputs:sourceColorSpace").Get() == "sRGB",
                   f"{path}/preview/baseTexture is not marked sRGB")
            for node, node_id in (("baseTexture", "ND_image_color4"),
                                  ("baseColorFactor", "ND_multiply_color4"),
                                  ("baseColorSplit", "ND_separate4_color4"),
                                  ("baseColorRgb", "ND_combine3_color3")):
                shader_id(child(f"mtlx/{node}", "Shader"), node_id)
        else:
            expect(not self.stage.GetPrimAtPath(f"{path}/preview/baseTexture").IsValid(),
                   f"{path} has a preview texture node without a base texture")
            expect(not self.stage.GetPrimAtPath(f"{path}/mtlx/baseTexture").IsValid(),
                   f"{path} has an mtlx texture node without a base texture")

    def texture_slot(self, prim: Usd.Prim, slot: str, key: str, want: dict | None) -> None:
        """The verbatim source path as provenance whenever the slot names a
        texture; the anchored asset path only when it is safe; and it
        resolves exactly when the file is beside the fixture (TEXT §7)."""
        attr = prim.GetAttribute(f"mmd:material:{slot}")
        where = f"{prim.GetPath()}.mmd:material:{slot}"
        if want is None:
            self.expect(not attr.IsValid(), f"{where} is authored for an empty slot")
            self.expect(not prim.HasCustomDataKey(key), f"{prim.GetPath()} has {key}")
            return
        self.expect(prim.GetCustomDataByKey(key) == want["source"],
                    f"{prim.GetPath()} {key} is {prim.GetCustomDataByKey(key)!r}")
        if want["asset"] is None:
            self.expect(not attr.IsValid(), f"{where} is authored for an unsafe path")
            return
        value = attr.Get()
        self.expect(attr.GetTypeName() == Sdf.ValueTypeNames.Asset
                    and value.path == want["asset"],
                    f"{where} is {value!r}, expected @{want['asset']}@")
        self.expect(bool(value.resolvedPath) == want["resolves"],
                    f"{where} resolves to {value.resolvedPath!r}")

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
