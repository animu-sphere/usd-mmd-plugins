#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Verify stage-contract physics through USD save/reopen without a solver."""

from __future__ import annotations

import argparse
import pathlib
import tempfile

from pxr import Gf, Usd, UsdGeom, UsdSkel

import stage_checks


def offsets(stage, expectation):
    """Derive bone/body rest offsets; compare against independent fixture frames."""
    skeleton = UsdSkel.Skeleton.Get(stage, "/Asset/skel/Skeleton")
    if not skeleton:
        return {}
    parents = UsdSkel.Topology(skeleton.GetJointsAttr().Get()).GetParentIndices()
    rest = skeleton.GetRestTransformsAttr().Get()
    asset_rest = []
    for index, parent in enumerate(parents):
        asset_rest.append(rest[index] * asset_rest[parent] if parent >= 0 else rest[index])
    result = {}
    for body in expectation["stage"]["physics"]["rigidBodies"]:
        bone = body["bone"]
        if bone == -1:
            continue
        prim = stage.GetPrimAtPath(f"/Asset/physics/rigidBodies/{body['id']}")
        frame = UsdGeom.Xformable(prim).GetLocalTransformation()
        # USD uses row vectors: body frame = offset * bone frame.
        offset = frame * asset_rest[bone].GetInverse()
        q = body["orientation"]
        expected = Gf.Matrix4d(1)
        expected.SetRotate(Gf.Quatd(q[3], Gf.Vec3d(*q[:3])))
        expected.SetTranslateOnly(Gf.Vec3d(*body["position"]) -
                                  Gf.Vec3d(*expectation["stage"]["joints"][bone]["bind"]))
        assert Gf.IsClose(offset, expected, 1e-7), f"{prim.GetPath()}: rest offset"
        assert Gf.IsClose(offset * asset_rest[bone], frame, 1e-12)
        result[body["id"]] = offset
    return result


def check_reopened(path, expectation, where, original):
    """Release every stage-owning wrapper before the caller removes the file.

    USDC layers may keep their backing file mapped on Windows. The checker
    also owns the stage, so dropping only the direct stage variable is not
    sufficient to release the file.
    """
    reopened = Usd.Stage.Open(str(path))
    checker = stage_checks.Checker(reopened, expectation, where)
    checker.physics()
    mesh = UsdGeom.Mesh.Get(reopened, "/Asset/geo/Mesh")
    checker.skeleton(mesh if mesh else None)
    assert UsdGeom.GetStageMetersPerUnit(reopened) == 1.0
    assert UsdGeom.GetStageUpAxis(reopened) == UsdGeom.Tokens.y
    restored = offsets(reopened, expectation)
    assert original.keys() == restored.keys()
    for key in original:
        assert Gf.IsClose(original[key], restored[key], 1e-12)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixtures", required=True, type=pathlib.Path)
    args = parser.parse_args()
    stage_checks.pmx_format()
    count = 0
    with tempfile.TemporaryDirectory(prefix="mmd-physics-roundtrip-") as scratch:
        for relative, expectation in sorted(stage_checks.load_manifest(args.fixtures).items()):
            if not expectation["opens"] or not expectation["stage"]["physics"]:
                continue
            stage = Usd.Stage.Open(str(args.fixtures / relative))
            stage_checks.Checker(stage, expectation, relative).physics()
            original = offsets(stage, expectation)
            for extension in ("usda", "usdc"):
                path = pathlib.Path(scratch) / f"{count}.{extension}"
                assert stage.Export(str(path))
                check_reopened(path, expectation, f"{relative} -> {extension}", original)
                # Exercise the Windows lifetime requirement for every file,
                # including the last USDC, before TemporaryDirectory cleanup.
                path.unlink()
            count += 1
    assert count > 0, "no physics fixtures tested"
    print(f"{count} physics fixtures preserved through USDA and USDC save/reopen")


if __name__ == "__main__":
    main()
