#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Write the pin table for the packages a release pushes to GHCR.

The release workflow's `publish` job pushes every bundle and tool package, on
every target, to one GHCR repository, tagged `<name>-<version>-<target>` (the
shape usd-motion-plugins publishes). A consumer needs two digests per package
and target: the archive digest, which a descriptor or Formation names as the
`artifact`, and the OCI manifest digest to pull it from, which changes on
every republish. This writes both, from the rows the job actually pushed, as
`package-pins.json` and a Markdown section for the release notes.

Rows are tab-separated: `kind name version target archive_digest [oci_digest]`,
`kind` being `bundle` or `tool`. A dry run passes rows without an OCI digest
and no `--published`; the table then names no source and says so, instead of
inventing a locator that does not exist.

The table is checked against the tree, not against a number written down: one
bundle per plugins/*/openstrata.plugin.yaml and one tool per
tools/*/openstrata.tool.yaml, each on exactly `--targets` targets. A target
whose packages never arrived fails here rather than ship a table that is right
about everything it names and missing a platform.

  make_package_pins.py --rows .ost-ci/pushed.tsv --version 0.2.0 \\
      --repository ghcr.io/animu-sphere/usd-mmd-plugins --targets 3 \\
      --out pins --published
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys

REPO = pathlib.Path(__file__).resolve().parents[1]
KINDS = {"bundle": "plugins/*/openstrata.plugin.yaml",
         "tool": "tools/*/openstrata.tool.yaml"}


def descriptors(pattern: str) -> int:
    return sum(1 for path in REPO.glob(pattern) if ".strata" not in path.parts)


def read_rows(path: pathlib.Path, version: str, repository: str,
              published: bool) -> dict[str, dict[str, dict[str, dict]]]:
    pins: dict[str, dict[str, dict[str, dict]]] = {kind: {} for kind in KINDS}
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip():
            continue
        fields = line.split("\t")
        if len(fields) not in (5, 6):
            raise SystemExit(f"{path}:{number}: expected 5 or 6 fields, got {len(fields)}")
        kind, name, row_version, target, artifact = fields[:5]
        oci = fields[5] if len(fields) == 6 else ""
        if kind not in pins:
            raise SystemExit(f"{path}:{number}: unknown kind {kind!r} for {name}")
        if row_version != version:
            raise SystemExit(f"{path}:{number}: {name} is version {row_version}, "
                             f"the release is {version}")
        if published and not oci:
            raise SystemExit(f"{path}:{number}: {name} {target} was published "
                             f"but has no OCI digest")
        entry = {"artifact": artifact}
        if oci:
            entry["source"] = f"oci://{repository}@{oci}"
            entry["tag"] = f"oci://{repository}:{name}-{version}-{target}"
        targets = pins[kind].setdefault(name, {})
        if target in targets:
            raise SystemExit(f"{path}:{number}: {name} {target} appears twice")
        targets[target] = entry
    return pins


def check(pins: dict, want_targets: int) -> None:
    for kind, pattern in KINDS.items():
        want = descriptors(pattern)
        if len(pins[kind]) != want:
            raise SystemExit(f"the pin table has {len(pins[kind])} {kind}(s) "
                             f"{sorted(pins[kind])}, the tree declares {want}")
        for name, targets in sorted(pins[kind].items()):
            if len(targets) != want_targets:
                raise SystemExit(f"{name} has {len(targets)} target(s) {sorted(targets)}, "
                                 f"expected {want_targets}")


def render_markdown(pins: dict, version: str, published: bool) -> str:
    major, minor = (int(part) for part in version.split(".")[:2])
    md = ["## Packages on GHCR", ""]
    if not published:
        md += ["> Dry run: nothing was pushed, so no source is named. "
               "The digests are this build's.", ""]
    md += ["Every bundle and tool, on every target. Pull by the OCI digest and "
           "name the archive digest as the `artifact`.", ""]
    for kind in KINDS:
        section = "bundles" if kind == "bundle" else "tools"
        for name in sorted(pins[kind]):
            md += [f"<details><summary><code>{name}</code> ({kind})</summary>", "",
                   "```yaml", "requires:", f"  {section}:",
                   f"    - id: {name}",
                   f'      version: ">={major}.{minor},<{major}.{minor + 1}"',
                   "      artifact:", "        targets:"]
            for target, entry in sorted(pins[kind][name].items()):
                md.append(f"          {target}:")
                md.append(f"            digest: {entry['artifact']}")
                if "source" in entry:
                    md.append(f"            source: {entry['source']}")
            md += ["```", "", "</details>", ""]
    return "\n".join(md)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--rows", type=pathlib.Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--repository", required=True)
    parser.add_argument("--targets", type=int, required=True)
    parser.add_argument("--out", type=pathlib.Path, required=True)
    parser.add_argument("--published", action="store_true")
    args = parser.parse_args(argv)

    pins = read_rows(args.rows, args.version, args.repository, args.published)
    check(pins, args.targets)
    args.out.mkdir(parents=True, exist_ok=True)
    document = {"version": args.version, "published": args.published,
                "repository": args.repository,
                "bundles": pins["bundle"], "tools": pins["tool"]}
    (args.out / "package-pins.json").write_text(
        json.dumps(document, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    (args.out / "package-pins.md").write_text(
        render_markdown(pins, args.version, args.published), encoding="utf-8")
    print(", ".join(f"{len(pins[kind])} {kind}(s)" for kind in KINDS)
          + f" on {args.targets} target(s) in the pin table")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
