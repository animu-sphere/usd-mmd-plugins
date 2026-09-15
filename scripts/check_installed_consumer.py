#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""The installed-consumer lane (docs/architecture/WORKSPACE.md §6).

Installs a built workspace into a clean prefix outside the repository, then
proves the prefix works on its own:

  1. the prefix holds what each package promises, and no file in it names the
     source tree or the build tree;
  2. tests/installed_consumer/, copied out of the repository, configures
     against the prefix alone, builds, and reads a PMX through mmdPmx;
  3. a Python host whose only plugin path is the prefix's opens a PMX through
     the installed usdMmdFileFormat.

  check_installed_consumer.py --build-dir build/windows-msvc --config Release
      --usd-root C:/usd/openusd-26.08 [--generator Ninja --make-program ...]
      [--keep DIR]
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
FIXTURES = REPO / "plugins" / "usdMmdFileFormat" / "tests" / "fixtures"
PLUGIN_RESOURCES = pathlib.Path("plugin", "resources", "usdMmdFileFormat")


def run(command: list[str], **kwargs) -> subprocess.CompletedProcess:
    print("$ " + " ".join(str(c) for c in command), flush=True)
    return subprocess.run([str(c) for c in command], check=True, text=True,
                          encoding="utf-8", errors="replace", **kwargs)


def shared_library(name: str) -> str:
    if sys.platform == "win32":
        return f"lib{name}.dll"
    return f"lib{name}.dylib" if sys.platform == "darwin" else f"lib{name}.so"


def check_prefix(prefix: pathlib.Path, build_dir: pathlib.Path) -> list[str]:
    errors: list[str] = []
    expected = [
        pathlib.Path("include", "mmdPmx", "Reader.h"),
        pathlib.Path("lib", "cmake", "mmdPmx", "mmdPmxConfig.cmake"),
        pathlib.Path("lib", "cmake", "mmdPmx", "mmdPmxConfigVersion.cmake"),
        pathlib.Path("lib", shared_library("UsdMmdFileFormat")),
        PLUGIN_RESOURCES / "plugInfo.json",
        PLUGIN_RESOURCES / "buildInfo.json",
    ]
    for relative in expected:
        if not (prefix / relative).is_file():
            errors.append(f"the prefix has no {relative.as_posix()}")

    plug_info = prefix / PLUGIN_RESOURCES / "plugInfo.json"
    if plug_info.is_file():
        for plugin in json.loads(plug_info.read_text(encoding="utf-8"))["Plugins"]:
            library_path = plugin["LibraryPath"]
            if os.path.isabs(library_path):
                errors.append(f"plugInfo.json LibraryPath is absolute: {library_path}")
            elif not (plug_info.parent / library_path).is_file():
                errors.append(f"plugInfo.json LibraryPath does not resolve: "
                              f"{library_path}")

    build_info = prefix / PLUGIN_RESOURCES / "buildInfo.json"
    if build_info.is_file():
        info = json.loads(build_info.read_text(encoding="utf-8"))
        if info.get("stageContractVersion") != 1:
            errors.append(f"buildInfo.json stageContractVersion is "
                          f"{info.get('stageContractVersion')!r}")
        if info.get("openusdVersion") != "26.08":
            errors.append(f"buildInfo.json openusdVersion is "
                          f"{info.get('openusdVersion')!r}")

    # Nothing installed may point back at where it was built.
    forbidden = set()
    for root in (REPO, build_dir):
        text = str(root.resolve())
        forbidden.update({text, text.replace("\\", "/"),
                          text.replace("\\", "\\\\")})
    for path in prefix.rglob("*"):
        if path.suffix.lower() not in {".json", ".cmake", ".yaml", ".txt", ".h"}:
            continue
        content = path.read_text(encoding="utf-8", errors="replace")
        lowered = content.lower()
        for needle in forbidden:
            if needle.lower() in lowered:
                errors.append(f"{path.relative_to(prefix).as_posix()} names "
                              f"{needle}")
                break
    return errors


def main() -> int:
    # Paths are printed, and a console's code page may not spell them.
    sys.stdout.reconfigure(errors="backslashreplace")
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--build-dir", required=True, type=pathlib.Path)
    parser.add_argument("--config", default="Release")
    parser.add_argument("--usd-root", required=True, type=pathlib.Path)
    parser.add_argument("--generator")
    parser.add_argument("--make-program")
    parser.add_argument("--cxx-compiler")
    parser.add_argument("--keep", type=pathlib.Path,
                        help="work here instead of a deleted temporary directory")
    args = parser.parse_args()

    scratch_owner = None
    if args.keep:
        work = args.keep.resolve()
        if work.exists():
            shutil.rmtree(work)
        work.mkdir(parents=True)
    else:
        scratch_owner = tempfile.TemporaryDirectory(prefix="usdmmd-consumer-")
        work = pathlib.Path(scratch_owner.name).resolve()

    try:
        prefix = work / "prefix"
        run(["cmake", "--install", args.build_dir, "--prefix", prefix,
             "--config", args.config])

        errors = check_prefix(prefix, args.build_dir)
        if errors:
            print("\n".join(errors), file=sys.stderr)
            return 1
        print("the prefix holds every promised file and names no build location")

        # The C++ consumer, configured from a copy outside the repository.
        source = work / "consumer-src"
        shutil.copytree(REPO / "tests" / "installed_consumer", source)
        fixtures = work / "fixtures"
        shutil.copytree(FIXTURES, fixtures)
        build = work / "consumer-build"
        configure = ["cmake", "-S", source, "-B", build,
                     f"-DCMAKE_PREFIX_PATH={prefix.as_posix()}"]
        if args.generator:
            configure += ["-G", args.generator]
        if args.make_program:
            configure.append(f"-DCMAKE_MAKE_PROGRAM={args.make_program}")
        if args.cxx_compiler:
            configure.append(f"-DCMAKE_CXX_COMPILER={args.cxx_compiler}")
        configure.append(f"-DCMAKE_BUILD_TYPE={args.config}")
        run(configure)
        run(["cmake", "--build", build, "--config", args.config])

        probes = sorted(build.rglob("pmx_header_probe.exe" if sys.platform == "win32"
                                    else "pmx_header_probe"))
        if not probes:
            print("the consumer built no pmx_header_probe", file=sys.stderr)
            return 1
        manifest = json.loads((fixtures / "fixtures.json").read_text(encoding="utf-8"))
        for relative, expectation in sorted(manifest.items()):
            result = subprocess.run([str(probes[0]), str(fixtures / relative)],
                                    text=True, encoding="utf-8",
                                    stdout=subprocess.PIPE)
            want = (f"version={expectation['sourceVersion']}" if expectation["opens"]
                    else f"fatal={expectation['fatal']}")
            if want not in result.stdout.splitlines():
                print(f"{relative}: the installed mmdPmx printed "
                      f"{result.stdout!r}, expected {want}", file=sys.stderr)
                return 1
            print(f"ok  mmdPmx reads {relative}: {want}")

        # The Python host, with only the prefix on the plugin path.
        env = dict(os.environ)
        env["PXR_PLUGINPATH_NAME"] = str(prefix / PLUGIN_RESOURCES)
        paths = [str(args.usd_root / "bin"), str(args.usd_root / "lib")]
        env["PATH"] = os.pathsep.join(paths + [env.get("PATH", "")])
        env["PYTHONPATH"] = os.pathsep.join(
            [str(args.usd_root / "lib" / "python"), env.get("PYTHONPATH", "")])
        run([sys.executable, source / "open_stage.py", prefix,
             fixtures / "minimal.pmx"], env=env)
        print("installed-consumer lane passed")
        return 0
    except subprocess.CalledProcessError as error:
        print(f"command failed with exit status {error.returncode}", file=sys.stderr)
        return 1
    finally:
        if scratch_owner:
            scratch_owner.cleanup()


if __name__ == "__main__":
    raise SystemExit(main())
