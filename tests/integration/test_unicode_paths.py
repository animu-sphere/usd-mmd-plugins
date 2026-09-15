#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Open a PMX under a directory no single ANSI code page can spell.

docs/design/TEXT_ENCODING_POLICY.md §4: the importer reads through Ar, never
through a narrow path, because it runs inside a host whose code page is not
the project's. This test is that host: a Python process with no UTF-8
manifest, so an importer that opened the file by its narrow path would fail
here even where every tool with the manifest passes.

Each fixture that opens is copied under `ユニコード-é/` -- Japanese and a
Latin-1 accent, which neither CP932 nor CP1252 can both spell -- and under an
ASCII twin, and the two stages must be identical. The test also calls CanRead
directly, because Usd.Stage.Open selects a format by extension and never
reaches it.
"""

from __future__ import annotations

import argparse
import pathlib
import shutil
import sys
import tempfile

from pxr import Usd

import stage_checks

UNICODE_DIR = "ユニコード-é"
ASCII_DIR = "ascii"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--fixtures", required=True, type=pathlib.Path)
    args = parser.parse_args()

    fmt = stage_checks.pmx_format()
    manifest = stage_checks.load_manifest(args.fixtures)
    opening = {k: v for k, v in sorted(manifest.items()) if v["opens"]}
    assert opening, "fixtures.json lists no fixture that opens"

    with tempfile.TemporaryDirectory() as scratch:
        root = pathlib.Path(scratch)
        for relative, expectation in opening.items():
            exported = {}
            for directory in (ASCII_DIR, UNICODE_DIR):
                target = root / directory / pathlib.PurePosixPath(relative).name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(args.fixtures / relative, target)
                where = f"{directory}/{target.name}"

                assert fmt.CanRead(str(target)), f"{where}: CanRead is false"
                stage = Usd.Stage.Open(str(target))
                stage_checks.check_opened_stage(stage, expectation, where)
                exported[directory] = stage.GetRootLayer().ExportToString()

            assert exported[ASCII_DIR] == exported[UNICODE_DIR], \
                f"{relative}: the stage depends on the directory's name"
            print(f"ok  {relative}")
    print(f"{len(opening)} fixtures open identically under {UNICODE_DIR}/")
    return 0


if __name__ == "__main__":
    sys.exit(main())
