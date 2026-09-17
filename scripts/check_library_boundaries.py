#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Enforce a plain library's side of the workspace dependency contract.

docs/architecture/WORKSPACE.md §2.3 names three gates; this script is the
link-line and include gates for one library under libs/ (the graph gate is
`ost plugin test --workspace --graph-only`). It fails when:

  * the library's sources or CMakeLists.txt reach OpenUSD, a physics SDK, or
    plugin registration (`#include <pxr/...>`, `find_package(pxr ...)`,
    `TF_REGISTRY_FUNCTION`, ...), or include a header of a component
    WORKSPACE.md §2.2 forbids it (`--forbid-include mmdPmx/`);
  * the library carries a plugin manifest or a plugInfo.json;
  * the library target's link line -- its LINK_LIBRARIES and
    INTERFACE_LINK_LIBRARIES, as CMake resolved them -- names anything but the
    edges WORKSPACE.md §2.1 allows it (`--allow`);
  * an executable that links the library and nothing else imports an OpenUSD
    shared library. For a static library this is the check that matters: a
    static archive has no import table of its own, so a forbidden edge shows
    up in what its consumers load.

Usage:
  check_library_boundaries.py --name mmdPmx --source libs/mmdPmx
      --link-file <build>/mmdPmx_link.txt --binary <build>/mmdPmx_tests.exe
      [--allow mmdPmx::mmdPmx ...] [--forbid-include mmdModel/ ...]
"""

from __future__ import annotations

import argparse
import os
import pathlib
import re
import shutil
import subprocess
import sys

FORBIDDEN_SOURCE = re.compile(
    r"#\s*include\s*[<\"](?:pxr/|btBulletDynamicsCommon|PxPhysicsAPI|Jolt/)"
    r"|PXR_NAMESPACE|TF_REGISTRY_FUNCTION|SDF_DEFINE_FILE_FORMAT"
    r"|AR_DEFINE_(?:PACKAGE_)?RESOLVER",
    re.IGNORECASE)
FORBIDDEN_CMAKE = re.compile(
    r"find_package\s*\(\s*(?:pxr|Bullet|PhysX|Jolt)\b", re.IGNORECASE)
FORBIDDEN_FILES = {"openstrata.plugin.yaml", "pluginfo.json", "pluginfo.json.in"}

# OpenUSD's shared libraries: usd_tf.dll on Windows, libusd_tf.so on Linux,
# libusd_tf.dylib on macOS, and the monolithic usd_ms in any of those forms.
USD_LIBRARY = re.compile(r"\b(?:lib)?usd_[A-Za-z0-9]+\.(?:dll|so|dylib)\b",
                         re.IGNORECASE)
SOURCE_SUFFIXES = {".h", ".hpp", ".hh", ".inl", ".c", ".cc", ".cpp", ".cxx"}


def _forbidden_include(prefixes: list[str]) -> re.Pattern | None:
    """`#include <prefix...>` or `#include "prefix..."` for any prefix."""
    if not prefixes:
        return None
    return re.compile(r"#\s*include\s*[<\"](?:" +
                      "|".join(re.escape(p) for p in prefixes) + ")")


def _find_dumpbin() -> str | None:
    tool = shutil.which("dumpbin")
    if tool:
        return tool
    roots = [
        pathlib.Path(os.environ.get("ProgramFiles", r"C:\Program Files")),
        pathlib.Path(os.environ.get("ProgramFiles(x86)",
                                    r"C:\Program Files (x86)")),
    ]
    for root in roots:
        # Any Visual Studio release: only `/dependents` is needed, and a
        # locator that names one release breaks on an in-place upgrade.
        matches = sorted(root.glob(
            "Microsoft Visual Studio/*/*/VC/Tools/MSVC/*/bin/Hostx64/x64/"
            "dumpbin.exe"), reverse=True)
        if matches:
            return str(matches[0])
    return None


def _binary_dependencies(binary: pathlib.Path) -> str:
    if sys.platform == "win32":
        tool = _find_dumpbin()
        if not tool:
            raise RuntimeError("dumpbin was not found")
        command = [tool, "/nologo", "/dependents", str(binary)]
    elif sys.platform == "darwin":
        tool = shutil.which("otool")
        if not tool:
            raise RuntimeError("otool was not found")
        command = [tool, "-L", str(binary)]
    else:
        tool = shutil.which("readelf")
        if not tool:
            raise RuntimeError("readelf was not found")
        command = [tool, "-d", str(binary)]
    return subprocess.run(
        command, check=True, text=True, encoding="utf-8", errors="replace",
        stdout=subprocess.PIPE).stdout


def _link_items(link_file: pathlib.Path) -> list[str]:
    """Every item on the LINK_LIBRARIES / INTERFACE_LINK_LIBRARIES lines."""
    items: list[str] = []
    for line in link_file.read_text(encoding="utf-8").splitlines():
        _, _, value = line.partition("=")
        for item in value.split(";"):
            item = item.strip()
            # $<LINK_ONLY:x> is how CMake carries a static library's private
            # dependency to its consumers; the dependency is still an edge.
            match = re.fullmatch(r"\$<LINK_ONLY:(.*)>", item)
            if match:
                item = match.group(1)
            if item:
                items.append(item)
    return items


def check(args: argparse.Namespace) -> list[str]:
    errors: list[str] = []
    source = args.source.resolve()

    for path in source.rglob("*"):
        if path.is_file() and path.name.lower() in FORBIDDEN_FILES:
            errors.append(f"plugin registration file is forbidden: {path}")

    sibling = _forbidden_include(args.forbid_include)
    for area in (source / "include", source / "src"):
        for path in area.rglob("*"):
            if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES:
                text = path.read_text(encoding="utf-8")
                if FORBIDDEN_SOURCE.search(text):
                    errors.append(f"OpenUSD, physics or plugin API: {path}")
                if sibling and sibling.search(text):
                    errors.append(f"{args.name} includes a component WORKSPACE.md "
                                  f"§2.2 forbids it: {path}")

    for cmake in [source / "CMakeLists.txt", *source.rglob("*.cmake"),
                  *source.rglob("*.cmake.in")]:
        if cmake.is_file() and FORBIDDEN_CMAKE.search(
                cmake.read_text(encoding="utf-8")):
            errors.append(f"{args.name} CMake must not resolve OpenUSD or a "
                          f"physics engine: {cmake}")

    allowed = set(args.allow)
    for item in _link_items(args.link_file):
        if item not in allowed:
            errors.append(f"{args.name} links '{item}', which WORKSPACE.md "
                          f"§2.1 does not allow it")

    try:
        dependencies = _binary_dependencies(args.binary.resolve())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as exc:
        errors.append(f"could not inspect {args.binary}: {exc}")
        dependencies = ""
    for match in sorted(set(USD_LIBRARY.findall(dependencies))):
        errors.append(f"{args.binary.name}, which links only {args.name}, "
                      f"imports the OpenUSD library {match}")
    return errors


def selftest() -> int:
    """Each rule against a case it must reject, so a rule that can no longer
    fail is found on every run rather than by the next forbidden edge."""
    failures: list[str] = []

    def expect(condition: bool, what: str) -> None:
        if not condition:
            failures.append(what)

    expect(bool(FORBIDDEN_SOURCE.search('#include "pxr/usd/sdf/layer.h"')),
           "a pxr include is not caught")
    expect(bool(FORBIDDEN_SOURCE.search("#include <pxr/pxr.h>")),
           "an angle-bracket pxr include is not caught")
    expect(not FORBIDDEN_SOURCE.search('#include "mmdPmx/Reader.h"'),
           "an own include is rejected")
    sibling = _forbidden_include(["mmdPmx/", "mmdModel/"])
    expect(bool(sibling.search("#include <mmdModel/Basis.h>")),
           "a forbidden sibling include is not caught")
    expect(bool(sibling.search('#include "mmdPmx/Diagnostic.h"')),
           "a quoted forbidden sibling include is not caught")
    expect(not sibling.search('#include "motionVmd/Reader.h"'),
           "an allowed include is rejected as a sibling")
    expect(_forbidden_include([]) is None, "no prefix still forbids something")
    expect(bool(FORBIDDEN_CMAKE.search("find_package(pxr REQUIRED CONFIG)")),
           "find_package(pxr) is not caught")
    for name in ("usd_tf.dll", "libusd_sdf.so", "libusd_usd.dylib",
                 "usd_ms.dll"):
        expect(bool(USD_LIBRARY.search(f"    {name}\n")),
               f"{name} is not recognized as OpenUSD")
    for name in ("KERNEL32.dll", "MSVCP140.dll", "libstdc++.so.6",
                 "libc.so.6"):
        expect(not USD_LIBRARY.search(f"    {name}\n"),
               f"{name} is mistaken for OpenUSD")

    import tempfile
    with tempfile.TemporaryDirectory() as scratch:
        link_file = pathlib.Path(scratch) / "link.txt"
        link_file.write_text(
            "LINK_LIBRARIES=usd_tf\n"
            "INTERFACE_LINK_LIBRARIES=$<LINK_ONLY:sdf>;mmdPmx::mmdPmx\n",
            encoding="utf-8")
        expect(_link_items(link_file) == ["usd_tf", "sdf", "mmdPmx::mmdPmx"],
               "link items are not read back as CMake wrote them")

    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    print("check_library_boundaries selftest passed")
    return 0


def main() -> int:
    if sys.argv[1:] == ["--selftest"]:
        return selftest()

    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--name", required=True)
    parser.add_argument("--source", required=True, type=pathlib.Path)
    parser.add_argument("--link-file", required=True, type=pathlib.Path)
    parser.add_argument("--binary", required=True, type=pathlib.Path)
    parser.add_argument("--allow", action="append", default=[],
                        help="a link item WORKSPACE.md §2.1 permits")
    parser.add_argument("--forbid-include", action="append", default=[],
                        help="a header prefix WORKSPACE.md §2.2 forbids, e.g. mmdPmx/")
    args = parser.parse_args()

    errors = check(args)
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print(f"{args.name} boundary check passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
