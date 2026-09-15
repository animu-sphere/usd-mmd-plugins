#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Open every fixture through the registered plugin.

A fixture that `opens` must author the stage fixtures.json and the stage
contract describe; one that `fails` must fail Usd.Stage.Open with an error
that names its fatal code (docs/reference/DIAGNOSTICS.md §4). Each stage is
also re-read and compared with itself: the same bytes author the same stage
(docs/design/DESIGN_POLICY.md §2.5).
"""

from __future__ import annotations

import argparse
import pathlib
import sys

from pxr import Sdf, Tf, Usd

import stage_checks


def check_opens(path: pathlib.Path, expectation: dict, fmt: Sdf.FileFormat) -> None:
    where = path.name
    assert fmt.CanRead(str(path)), f"{where}: CanRead is false"

    stage = Usd.Stage.Open(str(path))
    stage_checks.check_opened_stage(stage, expectation, where)

    layer = stage.GetRootLayer()
    assert layer.GetFileFormat().formatId == "pmx", \
        f"{where}: opened by {layer.GetFileFormat().formatId!r}"
    first = layer.ExportToString()
    assert layer.Reload(force=True), f"{where}: reload failed"
    assert layer.ExportToString() == first, f"{where}: a re-read authored a different stage"


def check_fails(path: pathlib.Path, expectation: dict, fmt: Sdf.FileFormat) -> None:
    where = path.name
    code = expectation["fatal"]
    # Selection is by extension, so a malformed file still reaches Read, and
    # it is Read that must refuse it. CanRead refuses only a foreign signature.
    if code == "MMD_PMX_BAD_SIGNATURE":
        assert not fmt.CanRead(str(path)), f"{where}: CanRead accepted a non-PMX"

    try:
        stage = Usd.Stage.Open(str(path))
    except Tf.ErrorException as error:
        assert code in str(error), f"{where}: the error does not name {code}: {error}"
        return
    raise AssertionError(f"{where}: opened (as {stage}), expected {code}")


def main() -> int:
    sys.stdout.reconfigure(errors="backslashreplace")
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--fixtures", required=True, type=pathlib.Path)
    args = parser.parse_args()

    fmt = stage_checks.pmx_format()
    manifest = stage_checks.load_manifest(args.fixtures)
    for relative, expectation in sorted(manifest.items()):
        path = args.fixtures / relative
        if expectation["opens"]:
            check_opens(path, expectation, fmt)
        else:
            check_fails(path, expectation, fmt)
        print(f"ok  {relative}")
    print(f"{len(manifest)} fixtures behaved as fixtures.json says")
    return 0


if __name__ == "__main__":
    sys.exit(main())
