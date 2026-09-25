#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Write mmdSchema's committed usdGenSchema output from schema/schema.usda.

schema/schema.usda is the one source of MmdMaterialAPI
(docs/design/MATERIAL_POLICY.md §4.3). This script is the one author of what
usdGenSchema derives from it and the bundle commits, so a plain-CMake build
needs neither `ost` nor usdGenSchema:

  * src/mmdSchema/  api.h, tokens.h, tokens.cpp and one .h/.cpp per class --
    the C++ accessors the library compiles;
  * plugin/resources/mmdSchema/generatedSchema.usda -- the prim definitions
    the schema registry reads;
  * plugin/resources/mmdSchema/plugInfo.json.in -- the registration, with its
    `Types`, and the library path left for CMake to configure per platform.

usdGenSchema's Python wrappers (wrap*.cpp, generatedSchema.module.h) are not
kept: no compiled Python module is built, and Python reads the schema through
the registry (WORKSPACE.md §1). Its output is deterministic -- no path, date or
host -- so `--check` compares bytes.

usdGenSchema runs from the OpenUSD 26.08 install named by --usd-root, or by
USD_INSTALL_ROOT, under the interpreter running this script, which must be the
one OpenUSD was built with and have jinja2.

  generate_schema.py [--usd-root <dir>]            rewrite the committed files
  generate_schema.py [--usd-root <dir>] --check    fail if any differs
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import subprocess
import sys
import tempfile

BUNDLE = pathlib.Path(__file__).resolve().parents[1]
SCHEMA = BUNDLE / "schema" / "schema.usda"
SOURCES = BUNDLE / "src" / "mmdSchema"
RESOURCES = BUNDLE / "plugin" / "resources" / "mmdSchema"

# The C++ usdGenSchema writes that the library compiles.
CPP_OUTPUTS = ["api.h", "tokens.h", "tokens.cpp", "mmdMaterialAPI.h", "mmdMaterialAPI.cpp"]

# plugInfo.json's placeholders, as the file-format bundle's template spells
# them: the library sits in the bundle's lib/, three levels above resources.
PLUG_INFO_PLACEHOLDERS = {
    "@PLUG_INFO_LIBRARY_PATH@":
        "../../../lib/@USDMMD_PLUGIN_LIBRARY_PREFIX@mmdSchema@CMAKE_SHARED_LIBRARY_SUFFIX@",
    "@PLUG_INFO_RESOURCE_PATH@": ".",
    "@PLUG_INFO_ROOT@": ".",
}


def usd_gen_schema(usd_root: pathlib.Path, out: pathlib.Path) -> None:
    tool = usd_root / "bin" / "usdGenSchema"
    if not tool.is_file():
        raise SystemExit(f"generate_schema: no usdGenSchema under {usd_root / 'bin'}")
    env = dict(os.environ)
    env["PYTHONPATH"] = os.pathsep.join(
        [str(usd_root / "lib" / "python"), env.get("PYTHONPATH", "")])
    env["PATH"] = os.pathsep.join(
        [str(usd_root / "bin"), str(usd_root / "lib"), env.get("PATH", "")])
    # Doc strings stay ASCII, but usdGenSchema reads and writes in the locale's
    # encoding unless told otherwise.
    env["PYTHONUTF8"] = "1"
    result = subprocess.run(
        [sys.executable, str(tool), str(SCHEMA), str(out)],
        env=env, capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stdout + result.stderr)
        raise SystemExit(f"generate_schema: usdGenSchema failed ({result.returncode})")


def plug_info_template(generated: str) -> str:
    # usdGenSchema writes JSON behind '#' comment lines; the template is plain
    # JSON, laid out as the other bundle's.
    body = "\n".join(line for line in generated.splitlines() if not line.lstrip().startswith("#"))
    text = json.dumps(json.loads(body), indent=4, sort_keys=True) + "\n"
    for placeholder, value in PLUG_INFO_PLACEHOLDERS.items():
        if placeholder not in text:
            raise SystemExit(f"generate_schema: plugInfo.json has no {placeholder}")
        text = text.replace(placeholder, value)
    return text


def lf(data: bytes) -> bytes:
    # usdGenSchema writes the platform's line ending; what is committed is
    # '\n' everywhere (.gitattributes), so one check holds on every host.
    return data.replace(b"\r\n", b"\n")


def expected(out: pathlib.Path) -> dict[pathlib.Path, bytes]:
    files = {SOURCES / name: lf((out / name).read_bytes()) for name in CPP_OUTPUTS}
    files[RESOURCES / "generatedSchema.usda"] = lf((out / "generatedSchema.usda").read_bytes())
    plug_info = lf((out / "plugInfo.json").read_bytes()).decode("utf-8")
    files[RESOURCES / "plugInfo.json.in"] = plug_info_template(plug_info).encode("utf-8")
    return files


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--usd-root", type=pathlib.Path,
                        default=os.environ.get("USD_INSTALL_ROOT"))
    parser.add_argument("--check", action="store_true",
                        help="fail if a committed file differs from what would be written")
    args = parser.parse_args()
    if args.usd_root is None:
        parser.error("name the OpenUSD install with --usd-root or USD_INSTALL_ROOT")

    with tempfile.TemporaryDirectory() as scratch:
        out = pathlib.Path(scratch)
        usd_gen_schema(args.usd_root.resolve(), out)
        files = expected(out)

    stale = [path for path, data in files.items()
             if not path.is_file() or path.read_bytes() != data]
    if args.check:
        for path in stale:
            print(f"stale: {path.relative_to(BUNDLE).as_posix()}", file=sys.stderr)
        if stale:
            print("run plugins/mmdSchema/tools/generate_schema.py to regenerate", file=sys.stderr)
            return 1
        return 0
    for path in stale:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(files[path])
        print(f"wrote {path.relative_to(BUNDLE).as_posix()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
