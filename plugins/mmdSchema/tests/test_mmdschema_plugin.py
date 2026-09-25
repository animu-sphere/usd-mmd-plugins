#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""The mmdSchema bundle on its own, with no PMX importer in the session.

Asserts, against docs/design/MATERIAL_POLICY.md:

  1. discovery -- the `mmdSchema` plugin is registered and declares exactly
     one type, MmdMaterialAPI, single-apply, and only on a Material (§4.3);
  2. inventory -- the applied prim definition holds exactly the §4.1
     properties, each `inputs:mmd:material:<name>` with the type, variability
     and fallback §4.1 and §4.3 state, and the two token sets;
  3. connectability -- every property is a UsdShade input of the Material, and
     a realization graph's input connected to one resolves to it (§3);
  4. the fixture -- authored values read back, and a Material that authors
     nothing reads every fallback.

Python reads the schema through the registry: no compiled module exists
(§4.3). Needs the bundle's plugInfo.json on PXR_PLUGINPATH_NAME, as
tests/CMakeLists.txt or `ost plugin run plugins/mmdSchema -- python <this>`
arrange.
"""

from __future__ import annotations

import pathlib
import sys

from pxr import Gf, Plug, Sdf, Usd, UsdShade

FIXTURE = pathlib.Path(__file__).resolve().parent / "fixtures" / "basic.usda"
API = "MmdMaterialAPI"
PREFIX = "inputs:mmd:material:"

C4 = Sdf.ValueTypeNames.Color4f
C3 = Sdf.ValueTypeNames.Color3f
F = Sdf.ValueTypeNames.Float
B = Sdf.ValueTypeNames.Bool
A = Sdf.ValueTypeNames.Asset
T = Sdf.ValueTypeNames.Token
I = Sdf.ValueTypeNames.Int
VARYING = Sdf.VariabilityVarying
UNIFORM = Sdf.VariabilityUniform

# MATERIAL_POLICY.md §4.1 (type, variability) and §4.3 (fallback).
INVENTORY = {
    "diffuseColor": (C4, VARYING, Gf.Vec4f(1, 1, 1, 1)),
    "specularColor": (C3, VARYING, Gf.Vec3f(0, 0, 0)),
    "specularPower": (F, VARYING, 0.0),
    "ambientColor": (C3, VARYING, Gf.Vec3f(0, 0, 0)),
    "doubleSided": (B, UNIFORM, False),
    "groundShadow": (B, UNIFORM, False),
    "castSelfShadow": (B, UNIFORM, False),
    "receiveSelfShadow": (B, UNIFORM, False),
    "drawEdge": (B, UNIFORM, False),
    "vertexColor": (B, UNIFORM, False),
    "drawPoints": (B, UNIFORM, False),
    "drawLines": (B, UNIFORM, False),
    "edgeColor": (C4, VARYING, Gf.Vec4f(0, 0, 0, 0)),
    "edgeSize": (F, VARYING, 0.0),
    "texture": (A, VARYING, Sdf.AssetPath()),
    "sphereTexture": (A, VARYING, Sdf.AssetPath()),
    "sphereMode": (T, UNIFORM, "disabled"),
    "toonSource": (T, UNIFORM, "none"),
    "toonTexture": (A, VARYING, Sdf.AssetPath()),
    "sharedToonIndex": (I, UNIFORM, -1),
}
ALLOWED_TOKENS = {
    "sphereMode": ["disabled", "multiply", "add", "subTexture"],
    "toonSource": ["none", "individual", "shared"],
}

failures: list[str] = []


def check(condition: bool, message: str) -> None:
    if not condition:
        failures.append(message)


def test_discovery() -> None:
    plugin = Plug.Registry().GetPluginWithName("mmdSchema")
    check(plugin is not None, "plugin mmdSchema is not registered")
    if plugin is None:
        return
    types = plugin.metadata.get("Types", {})
    check(sorted(types) == ["UsdMmdMaterialAPI"],
          f"mmdSchema declares {sorted(types)}, expected only UsdMmdMaterialAPI")
    info = types.get("UsdMmdMaterialAPI", {})
    check(info.get("schemaIdentifier") == API, f"schemaIdentifier {info.get('schemaIdentifier')}")
    check(info.get("schemaKind") == "singleApplyAPI", f"schemaKind {info.get('schemaKind')}")
    check(info.get("apiSchemaCanOnlyApplyTo") == ["Material"],
          f"apiSchemaCanOnlyApplyTo {info.get('apiSchemaCanOnlyApplyTo')}")


def test_inventory() -> None:
    definition = Usd.SchemaRegistry().FindAppliedAPIPrimDefinition(API)
    check(definition is not None, f"no applied prim definition for {API}")
    if definition is None:
        return
    names = sorted(definition.GetPropertyNames())
    check(names == sorted(PREFIX + name for name in INVENTORY),
          f"properties differ from §4.1: {names}")
    for name, (type_name, variability, fallback) in INVENTORY.items():
        spec = definition.GetSchemaAttributeSpec(PREFIX + name)
        if spec is None:
            continue
        check(spec.typeName == type_name, f"{name}: type {spec.typeName}, expected {type_name}")
        check(spec.variability == variability, f"{name}: variability {spec.variability}")
        check(spec.default == fallback, f"{name}: fallback {spec.default!r}, expected {fallback!r}")
        if name in ALLOWED_TOKENS:
            check(list(spec.allowedTokens) == ALLOWED_TOKENS[name],
                  f"{name}: allowedTokens {list(spec.allowedTokens)}")


def test_apply() -> None:
    stage = Usd.Stage.CreateInMemory()
    material = UsdShade.Material.Define(stage, "/mtl/m").GetPrim()
    xform = stage.DefinePrim("/x", "Xform")
    check(material.CanApplyAPI(API), "MmdMaterialAPI does not apply to a Material")
    check(not xform.CanApplyAPI(API), "MmdMaterialAPI applies to an Xform")
    check(material.ApplyAPI(API), "ApplyAPI failed on a Material")
    check(material.HasAPI(API), "HasAPI is false after ApplyAPI")
    shade = UsdShade.Material(material)
    for name in INVENTORY:
        shade_input = shade.GetInput(f"mmd:material:{name}")
        check(bool(shade_input), f"{name} is not a UsdShade input of the Material")


def test_fixture() -> None:
    stage = Usd.Stage.Open(str(FIXTURE))
    check(bool(stage), f"cannot open {FIXTURE}")
    if not stage:
        return
    body = stage.GetPrimAtPath("/Asset/mtl/body")
    check(body.HasAPI(API), "body does not have MmdMaterialAPI")
    check(body.GetAttribute(PREFIX + "diffuseColor").Get() == Gf.Vec4f(0.8, 0.7, 0.6, 1),
          "body diffuseColor")
    check(body.GetAttribute(PREFIX + "sphereMode").Get() == "multiply", "body sphereMode")
    check(body.GetAttribute(PREFIX + "sharedToonIndex").Get() == 3, "body sharedToonIndex")
    # §3: a realization reaches the canonical value through its own graph
    # input, connected to the Material's.
    graph = UsdShade.NodeGraph(stage.GetPrimAtPath("/Asset/mtl/body/preview"))
    producers = graph.GetInput("diffuseColor").GetValueProducingAttributes()
    check([a.GetPath() for a in producers]
          == [Sdf.Path("/Asset/mtl/body.inputs:mmd:material:diffuseColor")],
          f"preview diffuseColor resolves to {[str(a.GetPath()) for a in producers]}")

    unauthored = stage.GetPrimAtPath("/Asset/mtl/unauthored")
    check(unauthored.HasAPI(API), "unauthored does not have MmdMaterialAPI")
    for name, (_, _, fallback) in INVENTORY.items():
        attr = unauthored.GetAttribute(PREFIX + name)
        check(bool(attr) and not attr.HasAuthoredValue(), f"unauthored {name} is authored")
        check(attr.Get() == fallback, f"unauthored {name} reads {attr.Get()!r}")


def main() -> int:
    # Every later check needs the schema registered; without it they only
    # repeat that it is not.
    test_discovery()
    if not failures:
        for test in (test_inventory, test_apply, test_fixture):
            test()
    for failure in failures:
        print(f"FAIL: {failure}", file=sys.stderr)
    if failures:
        return 1
    print(f"mmdSchema: {API} registered with {len(INVENTORY)} inputs; fixture reads")
    return 0


if __name__ == "__main__":
    sys.exit(main())
