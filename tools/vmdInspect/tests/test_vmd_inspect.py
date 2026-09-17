#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""vmd_inspect against every generated VMD fixture.

The fixtures are written into a scratch directory by
tests/fixtures/generate_vmd_fixtures.py, then read through the tool:

  * one that `opens` must report ok, its signature, model name, sections,
    record counts, track counts, and exactly its `diagnostics`;
  * one that `fails` must report its `fatal` code;
  * the exit status follows the most severe diagnostic (DIAGNOSTICS.md §4).

Each fixture is read from an ASCII directory and from `ユニコード-é/`, which no
single ANSI code page can spell (TEXT_ENCODING_POLICY.md §4).
"""

from __future__ import annotations

import argparse
import json
import pathlib
import shutil
import subprocess
import sys
import tempfile

UNICODE_DIR = "ユニコード-é"


def run(tool: pathlib.Path, *args: str) -> subprocess.CompletedProcess:
    return subprocess.run([str(tool), *args], stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE)


def check_fixture(tool: pathlib.Path, path: pathlib.Path, expectation: dict,
                  where: str) -> None:
    result = run(tool, "--json", str(path))
    report = json.loads(result.stdout.decode("utf-8"))

    def expect(condition: bool, what: str) -> None:
        assert condition, f"{where}: {what}\n{result.stdout.decode('utf-8')}"

    if not expectation["opens"]:
        expect(report["ok"] is False, "read a fixture that must fail")
        expect(report["fatal"]["code"] == expectation["fatal"],
               f"fatal is {report['fatal']['code']}, expected {expectation['fatal']}")
        expect(result.returncode == 2, f"exit status {result.returncode}, expected 2")
        return

    expect(report["ok"] is True, "did not read")
    expect(report["fatal"] is None, "reports a fatal diagnostic")
    header = report["header"]
    expect(header["signature"] == expectation["signature"],
           f"signature {header['signature']!r}")
    expect(header["modelName"]["text"] == expectation["modelName"],
           f"model name {header['modelName']['text']!r}")
    expect(bytes.fromhex(header["modelName"]["bytes"]) ==
           expectation["modelName"].encode("cp932"), "model name bytes")
    expect(header["sectionsPresent"] == expectation["sectionsPresent"],
           f"sections {header['sectionsPresent']}")
    expect(report["counts"] == expectation["counts"],
           f"counts {report['counts']}, expected {expectation['counts']}")
    tracks = {kind: len(report["tracks"][kind]) for kind in ("bones", "morphs", "ik")}
    expect(tracks == expectation["tracks"],
           f"tracks {tracks}, expected {expectation['tracks']}")
    for kind in ("bones", "morphs", "ik"):
        for track in report["tracks"][kind]:
            expect(track["firstFrame"] <= track["lastFrame"], f"{kind} frame range")
            text, raw = track["name"]["text"], bytes.fromhex(track["name"]["bytes"])
            if text:
                expect(raw.startswith(text.encode("cp932")), f"{kind} name bytes {raw!r}")
    codes = [d["code"] for d in report["diagnostics"]]
    expect(codes == expectation["diagnostics"],
           f"diagnostics {codes}, expected {expectation['diagnostics']}")
    errors = any(d["severity"] == "error" for d in report["diagnostics"])
    expect(result.returncode == (1 if errors else 0), f"exit status {result.returncode}")


def main() -> int:
    sys.stdout.reconfigure(errors="backslashreplace")
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--tool", required=True, type=pathlib.Path)
    parser.add_argument("--generator", required=True, type=pathlib.Path)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="vmd-inspect-") as scratch:
        root = pathlib.Path(scratch)
        fixtures = root / "fixtures"
        subprocess.run([sys.executable, str(args.generator), "--out", str(fixtures)],
                       check=True, stdout=subprocess.DEVNULL)
        manifest = json.loads((fixtures / "fixtures.json").read_text(encoding="utf-8"))
        assert manifest, "the generator wrote no fixtures"

        for relative, expectation in sorted(manifest.items()):
            source = fixtures / relative
            check_fixture(args.tool, source, expectation, relative)
            twin = root / UNICODE_DIR / pathlib.PurePosixPath(relative).name
            twin.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, twin)
            check_fixture(args.tool, twin, expectation, f"{UNICODE_DIR}/{relative}")
            print(f"ok  {relative}")

        # The text report, with every track listed: Japanese names come out as
        # UTF-8, whatever the console's code page.
        sample = fixtures / "sample.vmd"
        text = run(args.tool, "--tracks", str(sample))
        assert text.returncode == 0, text.stderr
        out = text.stdout.decode("utf-8")
        for needle in ("Vocaloid Motion Data 0002", "model:     サンプル",
                       "bone keyframes: 5 keyframes in 3 tracks",
                       "  センター  3 keys, frames 0-60", "  左足ＩＫ  2 keys, frames 0-120",
                       "camera keyframes: 1 keyframe (frame 0)", "diagnostics: none"):
            assert needle in out, f"the text report has no {needle!r}:\n{out}"

        # Usage and I/O failures.
        assert run(args.tool).returncode == 3, "no argument is not a usage error"
        assert run(args.tool, "--bogus", str(sample)).returncode == 3
        missing = run(args.tool, "--json", str(root / "missing.vmd"))
        assert missing.returncode == 2
        assert json.loads(missing.stdout)["fatal"]["code"] == "MMD_MOTION_FILE_UNREADABLE"

    print(f"{len(manifest)} fixtures read as fixtures.json says, from both directories")
    return 0


if __name__ == "__main__":
    sys.exit(main())
