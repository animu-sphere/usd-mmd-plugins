#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Copy one target's release files out of `ost plugin package --json`.

The release set is `.data.release_members` -- openstrata.toml's
`[workspace].release_members`, as the packager resolved it -- and it is
checked twice before anything is copied:

  * against the tree: one bundle per plugins/*/openstrata.plugin.yaml and one
    tool per tools/*/openstrata.tool.yaml. The declaration is what a mistake
    would be made permanent in, so the set is also held to something nobody
    edits in the same commit (usd-vrm-plugins' release workflow, which found
    exactly that mistake);
  * against the product: the aggregate carries exactly that many members.

For each member it stages the archive and its manifest as
`<name>-<version>-<target>.manifest.json`; then the product archive and its
manifest the same way. Libraries are not members: they ship inside the bundle
and tools that link them (PACKAGE_CONTRACT.md).

  stage_release.py --package-json .ost-ci/package.json --out release-stage
"""

from __future__ import annotations

import argparse
import json
import pathlib
import shutil

REPO = pathlib.Path(__file__).resolve().parents[1]


def fail(message: str) -> SystemExit:
    return SystemExit(f"error: {message}")


def descriptors(pattern: str) -> int:
    return sum(1 for path in REPO.glob(pattern) if ".strata" not in path.parts)


def stage(archive: pathlib.Path, name: str, version: str, target: str,
          out: pathlib.Path) -> None:
    if not archive.is_file():
        raise fail(f"{name}: the packager names {archive}, which does not exist")
    manifest = archive.parent / "manifest.json"
    if not manifest.is_file():
        raise fail(f"{name}: no manifest.json beside {archive.name}")
    shutil.copy2(archive, out / archive.name)
    shutil.copy2(manifest, out / f"{name}-{version}-{target}.manifest.json")
    print(f"staged {archive.name}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--package-json", type=pathlib.Path, required=True)
    parser.add_argument("--out", type=pathlib.Path, required=True)
    args = parser.parse_args()

    data = json.loads(args.package_json.read_text(encoding="utf-8"))["data"]
    version = (REPO / "VERSION").read_text(encoding="utf-8").strip()
    members = data.get("release_members")
    if not members:
        raise fail("the packager reports no release_members; the product's membership "
                   "would be whatever it discovered")
    product = data.get("product") or {}
    if product.get("version") != version:
        raise fail(f"the product is version {product.get('version')}, VERSION is {version}")

    packages = {p["name"]: p for p in data["packages"]}
    missing = [m for m in members if m not in packages]
    if missing:
        raise fail(f"release members were not packaged: {', '.join(missing)}")
    release = [packages[m] for m in members]

    bundles = sum(1 for p in release if p["member"] == "bundle")
    tools = sum(1 for p in release if p["member"] == "tool")
    want_bundles = descriptors("plugins/*/openstrata.plugin.yaml")
    want_tools = descriptors("tools/*/openstrata.tool.yaml")
    print(f"tree: {want_bundles} bundle(s), {want_tools} tool(s); "
          f"release: {bundles} + {tools}; product: {product.get('members')} member(s)")
    if (bundles, tools) != (want_bundles, want_tools):
        raise fail(f"the tree declares {want_bundles} bundle(s) and {want_tools} tool(s), "
                   f"the release set is {bundles} + {tools}: reconcile openstrata.toml's "
                   f"release_members with the tree")
    if product.get("members") != len(release):
        raise fail(f"the product carries {product.get('members')} member(s), "
                   f"the release set {len(release)}")

    args.out.mkdir(parents=True, exist_ok=True)
    for package in release:
        stage(pathlib.Path(package["archive"]), package["name"], version,
              package["target"], args.out)
    stage(pathlib.Path(product["archive"]), product["name"], version,
          product["target"], args.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
