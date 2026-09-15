# SPDX-License-Identifier: Apache-2.0
"""Shared assertions for the integration tests.

Each check is one line of the stage contract's validation checklist
(docs/design/STAGE_CONTRACT.md §14) that the current Phase can reach. They
assert meaning, not text: codes, types, values -- never messages.
"""

from __future__ import annotations

import json
import pathlib

from pxr import Kind, Sdf, Usd, UsdGeom

STAGE_CONTRACT_VERSION = 1


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


def check_opened_stage(stage: Usd.Stage, expectation: dict, where: str) -> None:
    def expect(condition: bool, what: str) -> None:
        assert condition, f"{where}: {what}"

    expect(stage is not None, "the stage did not open")
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
    # Nothing is authored beneath /Asset before Phase 2, so it is an Xform
    # even for a model with bones; Phase 2 makes it the SkelRoot
    # (STAGE_CONTRACT.md §4.1).
    expect(asset.GetTypeName() == "Xform", f"/Asset is a {asset.GetTypeName()}")
    expect(not asset.GetChildren(), "/Asset has children before Phase 2")
    expect(Usd.ModelAPI(asset).GetKind() == Kind.Tokens.component,
           f"/Asset kind is {Usd.ModelAPI(asset).GetKind()!r}")

    expect(asset.GetCustomDataByKey("mmd:stageContractVersion")
           == STAGE_CONTRACT_VERSION, "mmd:stageContractVersion is not 1")
    expect(asset.GetCustomDataByKey("mmd:sourceFormat") == "PMX",
           "mmd:sourceFormat is not PMX")
    expect(asset.GetCustomDataByKey("mmd:sourceVersion")
           == expectation["sourceVersion"],
           f"mmd:sourceVersion is {asset.GetCustomDataByKey('mmd:sourceVersion')!r}")

    codes = recorded_codes(asset)
    expect(codes == expectation["diagnostics"],
           f"mmd:diagnostics records {codes}, expected "
           f"{expectation['diagnostics']}")
    if not expectation["diagnostics"]:
        expect(not asset.HasCustomDataKey("mmd:diagnostics"),
               "an empty mmd:diagnostics is authored")

    for prim in stage.Traverse():
        for attr in prim.GetAttributes():
            expect(attr.GetNumTimeSamples() == 0,
                   f"{attr.GetPath()} has time samples")
