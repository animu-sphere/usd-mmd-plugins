#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Open a stage, write down everything it holds, and optionally check a package.

Run by test_mmd_export.py in two processes: one with this repository's plugins
on PXR_PLUGINPATH_NAME, which opens the .pmx, and one with none, which opens
the .usdz (docs/design/PACKAGING_POLICY.md §14). The two dumps are compared
there.

  stage_probe.py <stage> --out <dump.json>              dump only
  stage_probe.py <stage> --out <dump.json> --package    and check §8 and §9's
                                                        post-write list, with
                                                        no MMD plugin loaded
"""

from __future__ import annotations

import argparse
import json
import os
import sys

from pxr import Ar, Sdf, Usd


def normalize(value):
    """A JSON value both processes spell alike. An asset path is kept as
    what the layer authors; its resolution differs by design."""
    if isinstance(value, Sdf.AssetPath):
        return {"asset": value.path}
    if isinstance(value, Sdf.AssetPathArray) or (
            isinstance(value, (list, tuple)) and value and
            all(isinstance(v, Sdf.AssetPath) for v in value)):
        return [{"asset": v.path} for v in value]
    if isinstance(value, dict):
        return {str(k): normalize(v) for k, v in sorted(value.items())}
    if isinstance(value, Sdf.Path):
        return str(value)
    if isinstance(value, (bool, int, str)) or value is None:
        return value
    if isinstance(value, float):
        return repr(value)
    return repr(value)


def api_schemas(prim: Usd.Prim) -> list[str]:
    op = prim.GetMetadata("apiSchemas")
    return [] if op is None else [str(t) for t in op.GetAddedOrExplicitItems()]


def dump(stage: Usd.Stage) -> dict:
    root = stage.GetPseudoRoot()
    out = {
        "stage": {key: normalize(stage.GetMetadata(key))
                  for key in ("defaultPrim", "upAxis", "metersPerUnit")},
        "prims": {},
    }
    for prim in Usd.PrimRange.Stage(stage, Usd.PrimAllPrimsPredicate):
        if prim == root:
            continue
        metadata = {k: normalize(v) for k, v in prim.GetAllAuthoredMetadata().items()
                    if k not in ("apiSchemas", "specifier", "typeName")}
        properties = {}
        for prop in prim.GetAuthoredProperties():
            record = {"metadata": {k: normalize(v)
                                   for k, v in prop.GetAllAuthoredMetadata().items()
                                   if k not in ("typeName", "variability")}}
            if isinstance(prop, Usd.Attribute):
                record["type"] = str(prop.GetTypeName())
                record["variability"] = str(prop.GetVariability())
                record["default"] = normalize(prop.Get())
                record["samples"] = [[t, normalize(prop.Get(t))]
                                     for t in prop.GetTimeSamples()]
                record["connections"] = [str(p) for p in prop.GetConnections()]
            else:
                record["targets"] = [str(p) for p in prop.GetTargets()]
            properties[prop.GetName()] = record
        out["prims"][str(prim.GetPath())] = {
            "type": prim.GetTypeName(), "specifier": str(prim.GetSpecifier()),
            "apiSchemas": api_schemas(prim), "metadata": metadata,
            "properties": properties,
        }
    return out


def check_package(path: str, stage: Usd.Stage) -> list[str]:
    """§8 and §9's post-write list, from a process with no MMD plugin."""
    problems = []
    if Sdf.FileFormat.FindByExtension("pmx") is not None:
        problems.append("the importer is loaded: this process must have no MMD plugin")
    if Usd.SchemaRegistry.GetTypeFromSchemaTypeName("MmdMaterialAPI"):
        problems.append("mmdSchema is loaded: this process must have no MMD plugin")
    if stage.GetMetadata("defaultPrim") != "Asset" or not stage.GetPrimAtPath("/Asset"):
        problems.append("defaultPrim is not /Asset")
    for error in stage.GetCompositionErrors():
        problems.append(f"composition error: {error}")

    package = os.path.normcase(os.path.realpath(path))

    def inside(resolved: str) -> bool:
        if not Ar.IsPackageRelativePath(resolved):
            return False
        outer, _ = Ar.SplitPackageRelativePathOuter(resolved)
        return os.path.normcase(os.path.realpath(outer)) == package

    for layer in stage.GetUsedLayers():
        real = layer.realPath
        if layer.anonymous:
            continue
        if os.path.normcase(os.path.realpath(real)) != package and not inside(real):
            problems.append(f"uses the layer {layer.identifier} from outside the package")

    assets = 0
    for prim in Usd.PrimRange.Stage(stage, Usd.PrimAllPrimsPredicate):
        for attr in prim.GetAuthoredAttributes():
            type_name = attr.GetTypeName()
            if type_name not in (Sdf.ValueTypeNames.Asset, Sdf.ValueTypeNames.AssetArray):
                continue
            values = [attr.Get()] + [attr.Get(t) for t in attr.GetTimeSamples()]
            for value in values:
                for each in (value if type_name == Sdf.ValueTypeNames.AssetArray
                             else [value]):
                    if each is None or not each.path:
                        continue
                    assets += 1
                    if os.path.isabs(each.path) or ":" in each.path:
                        problems.append(f"{attr.GetPath()}: absolute {each.path}")
                    elif not inside(each.resolvedPath):
                        problems.append(f"{attr.GetPath()}: {each.path} does not resolve "
                                        f"inside the package")

    # The schema is unknown here, so the API is not applied -- but it is
    # still authored, and its inputs are still there (§8).
    for prim in stage.Traverse():
        if prim.GetTypeName() != "Material":
            continue
        if "MmdMaterialAPI" not in api_schemas(prim):
            problems.append(f"{prim.GetPath()} does not author MmdMaterialAPI")
        if "MmdMaterialAPI" in prim.GetAppliedSchemas():
            problems.append(f"{prim.GetPath()} applies an unregistered schema")
        if not prim.GetAttribute("inputs:mmd:material:diffuseColor"):
            problems.append(f"{prim.GetPath()} lost its canonical inputs")
    print(f"{assets} asset values resolve inside the package")
    return problems


def main() -> int:
    sys.stdout.reconfigure(errors="backslashreplace")
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("stage")
    parser.add_argument("--out", required=True)
    parser.add_argument("--package", action="store_true")
    args = parser.parse_args()

    stage = Usd.Stage.Open(args.stage)
    if not stage:
        print(f"could not open {args.stage}", file=sys.stderr)
        return 1
    with open(args.out, "w", encoding="utf-8") as out:
        json.dump(dump(stage), out, ensure_ascii=False, indent=1, sort_keys=True)
    if args.package:
        problems = check_package(args.stage, stage)
        if problems:
            print("\n".join(problems), file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
