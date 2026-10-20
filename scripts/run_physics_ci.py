#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Run the bounded physics lane through installed packages (no network access)."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import sysconfig

REPO = Path(__file__).resolve().parents[1]
CASES = (("reader", "OFF", "OFF"),
         ("execution", "ON", "OFF"),
         ("springs", "ON", "ON"))


def python_cmake_args() -> list[str]:
    """Override SDK build-machine Python hints with this host's development SDK."""
    include = Path(sysconfig.get_path("include"))
    if not (include / "Python.h").is_file():
        raise FileNotFoundError(f"Host Python development headers missing: {include}")
    options = [f"-DPython3_EXECUTABLE={Path(sys.executable).as_posix()}",
               f"-DPython3_INCLUDE_DIR={include.as_posix()}"]
    if os.name == "nt":
        name = Path(sysconfig.get_config_var("LIBRARY")).with_suffix(".lib")
        library = Path(sys.base_prefix) / "libs" / name
        if not library.is_file():
            raise FileNotFoundError(f"Host Python import library missing: {library}")
        options.append(f"-DPython3_LIBRARY={library.as_posix()}")
    return options


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mmd-build", type=Path, required=True)
    parser.add_argument("--physics-prefix", type=Path, required=True)
    parser.add_argument("--usd-root", type=Path, required=True)
    parser.add_argument("--jolt-root", type=Path, required=True)
    parser.add_argument("--build-root", type=Path, required=True,
                        help="new directory for installed MMD, builds and evidence")
    parser.add_argument("--generator", help="CMake generator; default is the host default")
    args = parser.parse_args()
    mmd_build, physics, usd, jolt, root = (
        path.resolve() for path in (args.mmd_build, args.physics_prefix,
                                   args.usd_root, args.jolt_root, args.build_root))
    for required in (mmd_build / "CMakeCache.txt", usd / "pxrConfig.cmake",
                     physics / "lib/cmake/physicsUsd/physicsUsdConfig.cmake",
                     jolt / "lib/cmake/Jolt/JoltConfig.cmake"):
        if not required.is_file():
            parser.error(f"missing required build/package: {required}")
    # A new prefix cannot accidentally retain files from an earlier installation.
    root.mkdir(parents=True, exist_ok=False)
    mmd = root / "mmd-prefix"
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join((str(usd / "bin"), str(usd / "lib"), env.get("PATH", "")))
    env["PXR_PLUGINPATH_NAME"] = ""
    env["PYTHONPATH"] = str(usd / "lib/python")
    env.pop("CMAKE_PREFIX_PATH", None)
    env.pop("CMAKE_TOOLCHAIN_FILE", None)

    def run(label: str, command: list[str]) -> None:
        print("$ " + subprocess.list2cmdline(command), flush=True)
        with (root / f"{label}.log").open("w", encoding="utf-8") as log:
            result = subprocess.run(command, env=env, cwd=REPO, stdout=log,
                                    stderr=subprocess.STDOUT)
        print((root / f"{label}.log").read_text(encoding="utf-8", errors="replace"),
              end="", flush=True)
        result.check_returncode()

    python_options = python_cmake_args()
    provenance = {
        "configuration": "Release", "fullCorpus": False,
        "mmdBuild": str(mmd_build), "physicsPrefix": str(physics),
        "usdRoot": str(usd), "joltRoot": str(jolt),
        "cases": [case[0] for case in CASES],
        "pythonCmakeArgs": python_options,
    }
    (root / "configuration.json").write_text(json.dumps(provenance, indent=2) + "\n",
                                             encoding="utf-8")
    run("install-mmd", ["cmake", "--install", str(mmd_build), "--config", "Release",
                        "--prefix", str(mmd)])
    provenance["mmdBuildInfo"] = json.loads(
        (mmd / "plugin/resources/usdMmdFileFormat/buildInfo.json").read_text(encoding="utf-8"))
    (root / "configuration.json").write_text(json.dumps(provenance, indent=2) + "\n",
                                             encoding="utf-8")
    generator_name = args.generator or env.get("CMAKE_GENERATOR")
    generator = ["-G", generator_name] if generator_name else []
    if os.name == "nt" and (not generator_name or generator_name.startswith("Visual Studio")):
        generator += ["-A", "x64"]
    for name, execution, springs in CASES:
        build = root / name
        options = [
            f"-DCMAKE_PREFIX_PATH={physics.as_posix()};{usd.as_posix()}",
            *python_options,
            f"-DMMD_PHYSICS_MMD_PREFIX={mmd.as_posix()}",
            "-DBUILD_TESTING=ON", "-DMMD_PHYSICS_FULL_CORPUS=OFF",
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DMMD_PHYSICS_EXECUTION={execution}",
            f"-DMMD_PHYSICS_TRANSLATION_SPRINGS={springs}",
            f"-DMMD_PHYSICS_ROTATION_SPRINGS={springs}",
        ]
        if execution == "ON":
            options += [f"-DJolt_DIR={(jolt / 'lib/cmake/Jolt').as_posix()}"]
        run(f"{name}-configure", ["cmake", "-S", str(REPO / "tests/physics_consumer"),
                                 "-B", str(build), *generator, *options])
        run(f"{name}-build", ["cmake", "--build", str(build), "--config", "Release",
                             "--parallel", "2"])
        run(f"{name}-test", ["ctest", "--test-dir", str(build), "-C", "Release",
                            "--no-tests=error", "--timeout", "180",
                            "--output-on-failure", "--verbose",
                            "--output-junit", str(root / f"{name}.xml")])
    print(f"Installed physics lane passed; evidence: {root}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, subprocess.CalledProcessError) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
