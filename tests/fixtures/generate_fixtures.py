#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Write the PMX fixtures this repository tests against.

Every fixture is synthetic: its bytes are written here, so each one has a
known author and license (docs/design/DESIGN_POLICY.md §13). No distributed
MMD model is ever a fixture, and no PMX byte in the repository has another
author than this script.

  generate_fixtures.py              write the fixtures and fixtures.json
  generate_fixtures.py --check      fail unless the committed files are
                                    exactly what this script writes
  generate_fixtures.py --out DIR    write them somewhere else (a test's
                                    scratch directory)

The committed output lives in the file-format bundle,
plugins/usdMmdFileFormat/tests/fixtures/, because that is the only place its
manifest may name them from: `ost` refuses a `tests.smoke` path that leaves
the bundle (docs/architecture/WORKSPACE.md §6).

`fixtures.json` records what each fixture is for and what opening it must do:
`opens` with the diagnostic codes it must record, or `fails` with the fatal
code. Tests read expectations from there rather than restating them.

The layout follows docs/design/PMX_CONTRACT.md. Phase 0 reads only the header;
the tables are written in full anyway (empty), so the same files stay valid
inputs when the Phase 1 parser traverses them to the end.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import struct
import sys

REPO = pathlib.Path(__file__).resolve().parents[2]
COMMITTED = REPO / "plugins" / "usdMmdFileFormat" / "tests" / "fixtures"
MANIFEST = "fixtures.json"

UTF16LE = 0
UTF8 = 1


def text(value: str, encoding: int) -> bytes:
    """A PMX text field: int32 byte length, then the encoded bytes."""
    data = value.encode("utf-16-le" if encoding == UTF16LE else "utf-8")
    return struct.pack("<i", len(data)) + data


def header(version: float, globals_: list[int]) -> bytes:
    return (b"PMX " + struct.pack("<f", version)
            + bytes([len(globals_)]) + bytes(globals_))


def empty_model(version: float, encoding: int,
                extra_globals: list[int] | None = None) -> bytes:
    """A complete PMX whose every table is empty.

    Globals: the encoding, no additional vec4, and index width 1 for vertex,
    texture, material, bone, morph and rigid body.
    """
    globals_ = [encoding, 0, 1, 1, 1, 1, 1, 1] + (extra_globals or [])
    out = bytearray(header(version, globals_))
    for field in ("minimal", "minimal", "", ""):  # name, English name, comments
        out += text(field, encoding)
    # vertices, face indices, textures, materials, bones, morphs, display
    # frames, rigid bodies, joints -- and soft bodies in 2.1.
    tables = 10 if version == 2.1 else 9
    out += struct.pack("<i", 0) * tables
    return bytes(out)


def fixtures() -> dict[str, tuple[bytes, dict]]:
    """Every fixture: relative path -> (bytes, expectation)."""
    minimal = empty_model(2.0, UTF16LE)
    return {
        "minimal.pmx": (minimal, {
            "purpose": "PMX 2.0, UTF-16LE, every table empty",
            "opens": True, "sourceVersion": "2.0", "diagnostics": []}),
        "minimal-2.1-utf8.pmx": (empty_model(2.1, UTF8), {
            "purpose": "PMX 2.1, UTF-8, every table empty",
            "opens": True, "sourceVersion": "2.1", "diagnostics": []}),
        "unknown-globals.pmx": (empty_model(2.0, UTF16LE, extra_globals=[0]), {
            "purpose": "nine globals: the ninth is preserved and warned about",
            "opens": True, "sourceVersion": "2.0",
            "diagnostics": ["MMD_PMX_UNKNOWN_GLOBALS"]}),
        "malformed/bad-signature.pmx": (b"PMD " + minimal[4:], {
            "purpose": "a signature that is not \"PMX \"",
            "opens": False, "fatal": "MMD_PMX_BAD_SIGNATURE"}),
        "malformed/unsupported-version.pmx": (
            minimal[:4] + struct.pack("<f", 3.0) + minimal[8:], {
                "purpose": "version 3.0",
                "opens": False, "fatal": "MMD_PMX_UNSUPPORTED_VERSION"}),
        "malformed/truncated-header.pmx": (minimal[:12], {
            "purpose": "the file ends inside the globals",
            "opens": False, "fatal": "MMD_PMX_TRUNCATED_BUFFER"}),
    }


def manifest_text(entries: dict[str, tuple[bytes, dict]]) -> str:
    body = {path: expectation for path, (_, expectation) in entries.items()}
    return json.dumps(body, indent=2, sort_keys=True) + "\n"


def write(out: pathlib.Path) -> int:
    entries = fixtures()
    for relative, (data, _) in entries.items():
        path = out / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    (out / MANIFEST).write_bytes(manifest_text(entries).encode("utf-8"))
    print(f"wrote {len(entries)} fixtures and {MANIFEST} to {out}")
    return 0


def check(out: pathlib.Path) -> int:
    entries = fixtures()
    problems: list[str] = []
    for relative, (data, _) in entries.items():
        path = out / relative
        if not path.is_file():
            problems.append(f"missing: {relative}")
        elif path.read_bytes() != data:
            problems.append(f"differs from the generator: {relative}")
    committed = {p.relative_to(out).as_posix() for p in out.rglob("*.pmx")}
    for extra in sorted(committed - set(entries)):
        problems.append(f"not written by the generator: {extra}")
    manifest = out / MANIFEST
    if not manifest.is_file() or manifest.read_bytes() != manifest_text(
            entries).encode("utf-8"):
        problems.append(f"{MANIFEST} differs from the generator")

    if problems:
        print("\n".join(problems), file=sys.stderr)
        print("run tests/fixtures/generate_fixtures.py and commit the result",
              file=sys.stderr)
        return 1
    print(f"{len(entries)} fixtures in {out} match the generator")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true",
                        help="compare instead of writing")
    parser.add_argument("--out", type=pathlib.Path, default=COMMITTED,
                        help="output directory (default: the committed one)")
    args = parser.parse_args()
    return check(args.out) if args.check else write(args.out)


if __name__ == "__main__":
    raise SystemExit(main())
