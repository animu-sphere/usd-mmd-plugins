#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Build, test and package the libraries a C++ consumer pins.

The aggregate product carries bundles and tools, and a library reaches it only
inside the bundle that links it. `usd-avatar-runtime` builds its MMD session
against `mmdPhysicsAdapter`, so that library and what it requires are published
on their own (PACKAGE_CONTRACT.md).

The order matters twice. Dependencies come first, and every library is tested
straight after its own build: building a dependent rebuilds its prerequisites
with testing disabled, and `ost library test` then refuses them. Packaging comes
after every build, so no later prerequisite rebuild can change what was staged.

Each library is packaged twice and must keep its digest. It is staged as
`stage_release.py` stages a bundle: the archive and its manifest in `--out`, the
dist directory in `<oci-out>/packages/<name>/`, and one row
`library name version target archive_digest` in `<oci-out>/package-digests.tsv`.

  package_libraries.py --target cy2026 --profile usd \
      --out release-stage --oci-out oci-stage
"""

from __future__ import annotations

import argparse
import json
import pathlib
import shutil
import subprocess

REPO = pathlib.Path(__file__).resolve().parents[1]
# Dependencies first. make_package_pins.py holds the table to this list.
LIBRARIES = ("mmdPmx", "motionVmd", "mmdModel", "mmdMotionBinding", "mmdControl",
             "mmdPhysicsAdapter")
# Reaches OpenUSD. The pinned `ost` rejects the Python path OpenUSD's targets expose in
# a generated consumer (fixed in 0.23.16), and cannot declare the X11 packages Linux
# needs (0.23.18), so usd-avatar-runtime's session lanes build against this one instead.
NO_GENERATED_CONSUMER = {"mmdPhysicsAdapter"}


def ost(*arguments: str) -> None:
    print("+ ost", " ".join(arguments), flush=True)
    subprocess.run(["ost", *arguments], check=True, cwd=REPO)


def package(library: str, cell: tuple[str, ...]) -> dict:
    command = ["ost", "library", "package", library, *cell, "--json"]
    done = subprocess.run(command, check=True, cwd=REPO, capture_output=True, text=True)
    return json.loads(done.stdout)["data"]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--target", required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--out", type=pathlib.Path, required=True)
    parser.add_argument("--oci-out", type=pathlib.Path, required=True)
    args = parser.parse_args()

    version = (REPO / "VERSION").read_text(encoding="utf-8").strip()
    cell = ("--target", args.target, "--profile", args.profile)
    for name in LIBRARIES:
        library = f"libs/{name}"
        ost("library", "build", library, *cell)
        ost("library", "test", library, *cell)
        if name not in NO_GENERATED_CONSUMER:
            ost("library", "verify-consumer", library, *cell)

    args.out.mkdir(parents=True, exist_ok=True)
    args.oci_out.mkdir(parents=True, exist_ok=True)
    for name in LIBRARIES:
        library = f"libs/{name}"
        first, second = package(library, cell), package(library, cell)
        if first["archive_digest"] != second["archive_digest"]:
            raise SystemExit(f"two packagings of {name} disagree: "
                             f"{first['archive_digest']}, {second['archive_digest']}")
        if first["library"]["version"] != version:
            raise SystemExit(f"{name} is version {first['library']['version']}, "
                             f"VERSION is {version}")
        archive = pathlib.Path(first["archive"])
        target = first["target"]
        shutil.copy2(archive, args.out / archive.name)
        shutil.copy2(archive.parent / "manifest.json",
                     args.out / f"{name}-{version}-{target}.manifest.json")
        shutil.copytree(archive.parent, args.oci_out / "packages" / name)
        row = ["library", name, version, target, first["archive_digest"]]
        with (args.oci_out / "package-digests.tsv").open("a", encoding="utf-8", newline="\n") as rows:
            rows.write("\t".join(row) + "\n")
        print(f"staged {archive.name} {first['archive_digest']}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
