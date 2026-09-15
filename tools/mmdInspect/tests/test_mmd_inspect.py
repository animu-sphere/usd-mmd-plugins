#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""mmd_inspect against every generated fixture.

The fixtures are written into a scratch directory by the workspace generator
(docs/architecture/WORKSPACE.md §3: no component's tests read the bundle's
copies), then read through the tool:

  * one that `opens` must report ok, its source version, model name and table
    counts, and exactly its `parserDiagnostics` -- mmd_inspect reads with
    mmdPmx alone, so the importer's own codes are not among them;
  * one that `fails` must report its `fatal` code;
  * the exit status follows the most severe diagnostic (DIAGNOSTICS.md §4).

Each fixture is read from an ASCII directory and from `ユニコード-é/`, which no
single ANSI code page can spell: the tool's UTF-8 manifest is what makes the
second one reachable on Windows (TEXT_ENCODING_POLICY.md §4).
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
    expect(report["header"]["version"] == expectation["sourceVersion"],
           f"version {report['header']['version']}")
    expect(report["model"]["name"] == expectation["modelName"],
           f"model name {report['model']['name']!r}")
    expect(report["counts"] == expectation["counts"],
           f"counts {report['counts']}, expected {expectation['counts']}")
    codes = [d["code"] for d in report["diagnostics"]]
    expect(codes == expectation["parserDiagnostics"],
           f"diagnostics {codes}, expected {expectation['parserDiagnostics']}")
    errors = any(d["severity"] == "error" for d in report["diagnostics"])
    expect(result.returncode == (1 if errors else 0),
           f"exit status {result.returncode}")


def main() -> int:
    sys.stdout.reconfigure(errors="backslashreplace")
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--tool", required=True, type=pathlib.Path)
    parser.add_argument("--generator", required=True, type=pathlib.Path)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="mmd-inspect-") as scratch:
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

        # The text report, with every element listed: Japanese names come out
        # as UTF-8, whatever the console's code page.
        sample = fixtures / "sample-2.1-utf8.pmx"
        text = run(args.tool, "--elements", str(sample))
        assert text.returncode == 0, text.stderr
        out = text.stdout.decode("utf-8")
        for needle in ("PMX 2.1, UTF-8", "左ひじ (LeftElbow)", "tables:",
                       "soft bodies", "diagnostics: none"):
            assert needle in out, f"the text report has no {needle!r}:\n{out}"

        # Usage and I/O failures.
        assert run(args.tool).returncode == 3, "no argument is not a usage error"
        assert run(args.tool, "--bogus", str(sample)).returncode == 3
        missing = run(args.tool, "--json", str(root / "missing.pmx"))
        assert missing.returncode == 2
        assert json.loads(missing.stdout)["fatal"]["code"] == "MMD_PMX_FILE_UNREADABLE"

    print(f"{len(manifest)} fixtures read as fixtures.json says, from both directories")
    return 0


if __name__ == "__main__":
    sys.exit(main())
