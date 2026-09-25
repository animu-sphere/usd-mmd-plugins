#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Prove a packaged product works with nothing from the build tree.

The release workflow runs this over the product archive it is about to
publish. Every other lane tests a build tree or one bundle's package; this is
the only check of what a user actually installs -- all three members, from
the aggregate, into a fresh prefix outside the repository:

  ost plugin product install --prefix <scratch>/prefix <product>
  <prefix>/tools/mmd_inspect/bin/mmd_inspect --json <a PMX fixture>
  <prefix>/tools/vmd_inspect/bin/vmd_inspect --json <a generated VMD>
  ost plugin run plugins/usdMmdFileFormat --no-inject \\
      --plugin-path <prefix>/bundles/usdMmdFileFormat -- python <stage check>

`--no-inject` leaves only the runtime and the installed bundle on the
discovery path, and the stage check opens the fixture at `/Asset`, asserts
that the plugin which opened it is the installed one, that its materials
apply `MmdMaterialAPI` from the installed `mmdSchema`, and that the installed
`buildInfo.json` names this VERSION. The fixtures are copied under a
non-ASCII directory first (TEXT_ENCODING_POLICY.md).

  product_smoke.py --product dist/products/usd-mmd-plugins/<version>/<target>
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parents[1]
FIXTURE = REPO / "plugins" / "usdMmdFileFormat" / "tests" / "fixtures" / "sample-2.1-utf8.pmx"

STAGE_CHECK = r'''
import json, pathlib, sys
sys.stdout.reconfigure(errors="backslashreplace")
prefix = pathlib.Path(sys.argv[1]).resolve()
bundle = prefix / "bundles" / "usdMmdFileFormat"
sys.path.insert(0, str(bundle))
import openstrata_activate  # noqa: F401 -- DLL search paths before pxr, on Windows
from pxr import Plug, Sdf, Usd

stage = Usd.Stage.Open(sys.argv[2])
assert stage, "the fixture did not open"
assert stage.GetDefaultPrim().GetPath() == Sdf.Path("/Asset"), stage.GetDefaultPrim().GetPath()

plugin = Plug.Registry().GetPluginWithName("UsdMmdFileFormat")
assert plugin and plugin.isLoaded, "UsdMmdFileFormat is not loaded after the open"
where = pathlib.Path(plugin.path).resolve()
assert prefix in where.parents, f"the plugin loaded from {where}, not the installed product"

# Stage contract 2: every material applies the installed mmdSchema's API.
schema = Plug.Registry().GetPluginWithName("mmdSchema")
assert schema and prefix in pathlib.Path(schema.path).resolve().parents, \
    f"mmdSchema is not registered from the installed product: {schema and schema.path}"
hair = stage.GetPrimAtPath("/Asset/mtl/hair")
assert hair.HasAPI("MmdMaterialAPI"), hair.GetAppliedSchemas()

stamp = json.loads((bundle / "plugin" / "resources" / "usdMmdFileFormat" / "buildInfo.json")
                   .read_text(encoding="utf-8"))
assert stamp["projectVersion"] == sys.argv[3], stamp
print(f"stage: /Asset opened through {where}; buildInfo names {stamp['projectVersion']}")
'''


def run(command: list[str], **kwargs) -> subprocess.CompletedProcess:
    print("$ " + " ".join(command), flush=True)
    # Every Python this starts -- the fixture generator, the stage check --
    # prints those paths too.
    env = dict(os.environ, PYTHONIOENCODING="utf-8")
    return subprocess.run(command, check=False, env=env, **kwargs)


def tool(prefix: pathlib.Path, name: str) -> pathlib.Path:
    for candidate in (name + ".exe", name):
        path = prefix / "tools" / name / "bin" / candidate
        if path.is_file():
            return path
    raise SystemExit(f"FAIL: the product installs no {name} under {prefix / 'tools' / name / 'bin'}")


def inspect(executable: pathlib.Path, source: pathlib.Path) -> None:
    result = run([str(executable), "--json", str(source)], stdout=subprocess.PIPE)
    if result.returncode != 0:
        raise SystemExit(f"FAIL: {executable.name} exited {result.returncode} on {source.name}")
    report = json.loads(result.stdout.decode("utf-8"))
    if report.get("ok") is not True:
        raise SystemExit(f"FAIL: {executable.name} reported ok={report.get('ok')} on {source.name}")
    print(f"{executable.name}: read {source.name}")


def main() -> int:
    # The scratch paths are non-ASCII on purpose, and a hosted Windows
    # runner's console is cp1252: printing them must not be what fails.
    sys.stdout.reconfigure(errors="backslashreplace")
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--product", type=pathlib.Path, required=True,
                        help="product dist directory, manifest.json or .tar.zst")
    parser.add_argument("--ost", default="ost")
    # `python` as the session's PATH resolves it, the way usd-vrm-plugins'
    # clean-install smoke runs on every OS: the runtime's own where it bundles
    # one, the host 3.13 on Windows, where it does not.
    parser.add_argument("--python", default="python",
                        help="the interpreter the runtime session runs the stage check with")
    parser.add_argument("--keep", action="store_true")
    args = parser.parse_args()
    version = (REPO / "VERSION").read_text(encoding="utf-8").strip()

    scratch = pathlib.Path(tempfile.mkdtemp(prefix="mmd-product-smoke-"))
    if REPO in scratch.resolve().parents:
        raise SystemExit(f"FAIL: the scratch directory {scratch} is inside the repository")
    try:
        prefix = scratch / "prefix"
        if run([args.ost, "plugin", "product", "install", "--prefix", str(prefix),
                str(args.product.resolve())]).returncode != 0:
            raise SystemExit("FAIL: the product did not install")

        inputs = scratch / "入力"
        inputs.mkdir()
        pmx = inputs / FIXTURE.name
        shutil.copyfile(FIXTURE, pmx)
        if run([sys.executable, str(REPO / "tests" / "fixtures" / "generate_vmd_fixtures.py"),
                "--out", str(inputs)], stdout=subprocess.DEVNULL).returncode != 0:
            raise SystemExit("FAIL: the VMD fixture generator failed")

        inspect(tool(prefix, "mmd_inspect"), pmx)
        inspect(tool(prefix, "vmd_inspect"), inputs / "sample.vmd")

        check = scratch / "stage_check.py"
        check.write_text(STAGE_CHECK, encoding="utf-8")
        result = run([args.ost, "plugin", "run", str(REPO / "plugins" / "usdMmdFileFormat"),
                      "--no-inject", "--plugin-path", str(prefix / "bundles" / "usdMmdFileFormat"),
                      "--", args.python, str(check), str(prefix), str(pmx), version])
        if result.returncode != 0:
            raise SystemExit(f"FAIL: the stage check exited {result.returncode}")
    finally:
        if args.keep:
            print(f"kept {scratch}")
        else:
            shutil.rmtree(scratch, ignore_errors=True)

    print("product smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
