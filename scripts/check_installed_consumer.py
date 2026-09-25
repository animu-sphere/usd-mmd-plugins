#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""The installed-consumer lane (docs/architecture/WORKSPACE.md §6).

Installs a built workspace into a clean prefix outside the repository, then
proves the prefix works on its own:

  1. the prefix holds what each package promises, and no file in it names the
     source tree or the build tree;
  2. tests/installed_consumer/, copied out of the repository, configures
     against the prefix alone -- finding mmdModel, whose package finds mmdPmx,
     mmdMotionBinding, whose package finds mmdModel and motionVmd, and
     mmdControl, and both Phase 9 adapters with their released shared-motion
     dependencies --,
     builds, reads and canonicalizes every PMX fixture, reads every VMD
     fixture, binds one to a PMX and evaluates it;
  3. the installed mmd_inspect and vmd_inspect read every fixture from the
     prefix's bin/;
  4. a Python host whose only plugin path is the prefix's opens a PMX through
     the installed usdMmdFileFormat, with MmdMaterialAPI applied;
  5. the installed mmdSchema applies MmdMaterialAPI from a C++ consumer, and
     reads a fallback through the prefix's registered definition.

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
VMD_GENERATOR = REPO / "tests" / "fixtures" / "generate_vmd_fixtures.py"
PLUGIN_RESOURCES = pathlib.Path("plugin", "resources", "usdMmdFileFormat")
SCHEMA_RESOURCES = pathlib.Path("plugin", "resources", "mmdSchema")


def run(command: list[str], **kwargs) -> subprocess.CompletedProcess:
    print("$ " + " ".join(str(c) for c in command), flush=True)
    return subprocess.run([str(c) for c in command], check=True, text=True,
                          encoding="utf-8", errors="replace", **kwargs)


def shared_library(name: str) -> str:
    if sys.platform == "win32":
        return f"lib{name}.dll"
    return f"lib{name}.dylib" if sys.platform == "darwin" else f"lib{name}.so"


def executable(name: str) -> str:
    return f"{name}.exe" if sys.platform == "win32" else name


def check_prefix(prefix: pathlib.Path, build_dir: pathlib.Path) -> list[str]:
    errors: list[str] = []
    # The libraries install under CMAKE_INSTALL_LIBDIR, which GNUInstallDirs
    # makes lib64 on some Linux distributions; the plugin bundle's lib/ is
    # fixed by its plugInfo.json LibraryPath (PACKAGE_CONTRACT.md).
    for package in ("mmdPmx", "mmdModel", "motionVmd", "mmdMotionBinding", "mmdControl",
                    "mmdSkeletonAdapter", "mmdMotionAdapter", "mmdSchema"):
        config_dirs = sorted(p.parent for p in prefix.glob(
            f"lib*/cmake/{package}/{package}Config.cmake"))
        if len(config_dirs) != 1:
            errors.append(f"the prefix has {len(config_dirs)} {package} package configs "
                          f"under lib*/cmake/{package}, expected one")
        else:
            version_file = config_dirs[0] / f"{package}ConfigVersion.cmake"
            if not version_file.is_file():
                errors.append(f"the prefix has no "
                              f"{version_file.relative_to(prefix).as_posix()}")
    expected = [
        pathlib.Path("include", "mmdPmx", "Reader.h"),
        pathlib.Path("include", "mmdModel", "Canonicalize.h"),
        pathlib.Path("include", "motionVmd", "Reader.h"),
        pathlib.Path("include", "mmdMotionBinding", "Bind.h"),
        pathlib.Path("include", "mmdControl", "Evaluator.h"),
        pathlib.Path("include", "mmdSkeletonAdapter", "Adapter.h"),
        pathlib.Path("include", "mmdMotionAdapter", "Adapter.h"),
        pathlib.Path("include", "mmdSchema", "mmdMaterialAPI.h"),
        pathlib.Path("bin", executable("mmd_inspect")),
        pathlib.Path("bin", executable("vmd_inspect")),
        pathlib.Path("lib", shared_library("UsdMmdFileFormat")),
        PLUGIN_RESOURCES / "plugInfo.json",
        PLUGIN_RESOURCES / "buildInfo.json",
        pathlib.Path("lib", shared_library("mmdSchema")),
        SCHEMA_RESOURCES / "plugInfo.json",
        SCHEMA_RESOURCES / "generatedSchema.usda",
    ]
    for relative in expected:
        if not (prefix / relative).is_file():
            errors.append(f"the prefix has no {relative.as_posix()}")

    for plug_info in (prefix / PLUGIN_RESOURCES / "plugInfo.json",
                      prefix / SCHEMA_RESOURCES / "plugInfo.json"):
        if not plug_info.is_file():
            continue
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
        if info.get("stageContractVersion") != 2:
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
    parser.add_argument("--python3-executable", type=pathlib.Path)
    parser.add_argument("--python3-library", type=pathlib.Path)
    parser.add_argument("--python3-include-dir", type=pathlib.Path)
    parser.add_argument("--dependency-prefix", action="append", default=[],
                        type=pathlib.Path)
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
        search = [prefix, args.usd_root, *args.dependency_prefix]
        configure = ["cmake", "-S", source, "-B", build,
                     "-DCMAKE_PREFIX_PATH=" + ";".join(p.as_posix() for p in search)]
        # OpenUSD's relocatable pxrConfig.cmake carries build-host Python paths
        # as fallbacks.  Define the three inputs before find_package(pxr) so
        # those fallbacks cannot override the Python resolved by the workspace
        # configure on this host.
        for cmake_name, value in (
                ("Python3_EXECUTABLE", args.python3_executable),
                ("Python3_LIBRARY", args.python3_library),
                ("Python3_INCLUDE_DIR", args.python3_include_dir)):
            if value:
                configure.append(f"-D{cmake_name}={value.as_posix()}")
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
            if expectation["opens"]:
                stage = expectation["stage"]
                faces = stage["mesh"]["faces"] if stage["mesh"] else 0
                wants = [f"version={expectation['sourceVersion']}",
                         f"joints={len(stage['joints'])} faces={faces}"]
            else:
                wants = [f"fatal={expectation['fatal']}"]
            lines = result.stdout.splitlines()
            if not all(want in lines for want in wants):
                print(f"{relative}: the installed mmdPmx and mmdModel printed "
                      f"{result.stdout!r}, expected {wants}", file=sys.stderr)
                return 1
            print(f"ok  mmdPmx and mmdModel read {relative}: {', '.join(wants)}")

        # The installed tool, from the prefix alone: it links mmdPmx
        # statically and needs no other file.
        tool = prefix / "bin" / executable("mmd_inspect")
        for relative, expectation in sorted(manifest.items()):
            result = subprocess.run([str(tool), "--json", str(fixtures / relative)],
                                    stdout=subprocess.PIPE)
            report = json.loads(result.stdout.decode("utf-8"))
            if report["ok"] != expectation["opens"]:
                print(f"{relative}: the installed mmd_inspect reported ok="
                      f"{report['ok']}", file=sys.stderr)
                return 1
        print(f"ok  the installed mmd_inspect reads all {len(manifest)} fixtures")

        # VMD: the installed motionVmd and mmdMotionBinding through the
        # consumer, and the installed vmd_inspect, over the generated fixtures.
        vmd_fixtures = work / "vmd-fixtures"
        run([sys.executable, VMD_GENERATOR, "--out", vmd_fixtures],
            stdout=subprocess.DEVNULL)
        vmd_manifest = json.loads(
            (vmd_fixtures / "fixtures.json").read_text(encoding="utf-8"))
        vmd_probes = sorted(build.rglob(executable("vmd_probe")))
        if not vmd_probes:
            print("the consumer built no vmd_probe", file=sys.stderr)
            return 1
        vmd_tool = prefix / "bin" / executable("vmd_inspect")
        for relative, expectation in sorted(vmd_manifest.items()):
            path = vmd_fixtures / relative
            result = subprocess.run([str(vmd_probes[0]), str(path)], text=True,
                                    encoding="utf-8", stdout=subprocess.PIPE)
            if expectation["opens"]:
                tracks = expectation["tracks"]
                want = f"tracks={tracks['bones']} {tracks['morphs']} {tracks['ik']}"
            else:
                want = f"fatal={expectation['fatal']}"
            if want not in result.stdout.splitlines():
                print(f"{relative}: the installed motionVmd printed "
                      f"{result.stdout!r}, expected {want}", file=sys.stderr)
                return 1
            report = json.loads(subprocess.run(
                [str(vmd_tool), "--json", str(path)], stdout=subprocess.PIPE).stdout)
            if report["ok"] != expectation["opens"]:
                print(f"{relative}: the installed vmd_inspect reported ok="
                      f"{report['ok']}", file=sys.stderr)
                return 1
        # The sample model has センター and 左ひじ of the sample motion's three
        # bones, one of its two morphs, and neither IK bone.
        bound = subprocess.run([str(vmd_probes[0]), str(vmd_fixtures / "sample.vmd"),
                                str(fixtures / "sample-2.1-utf8.pmx")], text=True,
                               encoding="utf-8", stdout=subprocess.PIPE)
        if bound.returncode != 0 or "bound=2 1 0" not in bound.stdout.splitlines():
            print(f"the installed mmdMotionBinding printed {bound.stdout!r}",
                  file=sys.stderr)
            return 1
        print(f"ok  the installed motionVmd, mmdMotionBinding and vmd_inspect read all "
              f"{len(vmd_manifest)} VMD fixtures")

        # The installed mmdControl evaluates that bound motion at every frame.
        control_probes = sorted(build.rglob(executable("control_probe")))
        if not control_probes:
            print("the consumer built no control_probe", file=sys.stderr)
            return 1
        model = "sample-2.1-utf8.pmx"
        joints = len(manifest[model]["stage"]["joints"])
        evaluated = subprocess.run([str(control_probes[0]), str(vmd_fixtures / "sample.vmd"),
                                    str(fixtures / model)], text=True,
                                   encoding="utf-8", stdout=subprocess.PIPE)
        lines = evaluated.stdout.splitlines()
        if (evaluated.returncode != 0 or len(lines) != 1
                or not lines[0].startswith(f"joints={joints} ")
                or not lines[0].endswith(" finite=1")):
            print(f"the installed mmdControl printed {evaluated.stdout!r}, expected "
                  f"joints={joints} and finite=1", file=sys.stderr)
            return 1
        print(f"ok  the installed mmdControl evaluated sample.vmd over {model}: "
              f"{lines[0]}")

        adapter_probes = sorted(build.rglob(executable("adapter_probe")))
        if not adapter_probes:
            print("the consumer built no adapter_probe", file=sys.stderr)
            return 1
        # The adapters link OpenUSD's foundation libraries through the shared
        # motion packages, so the probe loads them: on Windows from PATH, which
        # an `ost` session sets and a plain-CMake build does not.
        runtime_env = dict(os.environ)
        runtime_env["PATH"] = os.pathsep.join(
            [str(args.usd_root / "bin"), str(args.usd_root / "lib"),
             runtime_env.get("PATH", "")])
        adapted = subprocess.run(
            [str(adapter_probes[0]), str(vmd_fixtures / "sample.vmd"),
             str(fixtures / model)], text=True, encoding="utf-8",
            stdout=subprocess.PIPE, env=runtime_env)
        adapter_lines = adapted.stdout.splitlines()
        if (adapted.returncode != 0 or len(adapter_lines) != 1
                or not adapter_lines[0].startswith("samples=")
                or f"joints={joints}" not in adapter_lines[0]
                or not adapter_lines[0].endswith("roleTable=2")):
            print(f"the installed adapters printed {adapted.stdout!r}", file=sys.stderr)
            return 1
        print(f"ok  the installed Phase 9 adapters built a shared MotionClip: "
              f"{adapter_lines[0]}")

        # The schema, registered from the prefix alone.
        schema_probes = sorted(build.rglob(executable("schema_probe")))
        if not schema_probes:
            print("the consumer built no schema_probe", file=sys.stderr)
            return 1
        schema_env = dict(runtime_env)
        schema_env["PATH"] = os.pathsep.join([str(prefix / "lib"), schema_env["PATH"]])
        schema_env["PXR_PLUGINPATH_NAME"] = str(prefix / SCHEMA_RESOURCES)
        schema = subprocess.run([str(schema_probes[0])], text=True, encoding="utf-8",
                                stdout=subprocess.PIPE, env=schema_env)
        want = "schema=MmdMaterialAPI inputs=20 sphereMode=disabled"
        if schema.returncode != 0 or schema.stdout.splitlines() != [want]:
            print(f"the installed mmdSchema printed {schema.stdout!r}, expected {want}",
                  file=sys.stderr)
            return 1
        print(f"ok  the installed mmdSchema applied its API: {want}")

        # The Python host, with only the prefix on the plugin path: the
        # importer and the schema bundle it requires.
        env = dict(runtime_env)
        env["PXR_PLUGINPATH_NAME"] = os.pathsep.join(
            [str(prefix / PLUGIN_RESOURCES), str(prefix / SCHEMA_RESOURCES)])
        env["PYTHONPATH"] = os.pathsep.join(
            [str(args.usd_root / "lib" / "python"), env.get("PYTHONPATH", "")])
        run([sys.executable, source / "open_stage.py", prefix,
             fixtures / "sample-2.0-utf16.pmx"], env=env)
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
