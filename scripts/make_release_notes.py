#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Render the GitHub release notes for one version.

Fills docs/contributing/RELEASE_NOTES_TEMPLATE.md with:

  {version}, {tag}   the release version (default: the repository-root VERSION)
  {changelog}        that version's `## [X.Y.Z]` section of CHANGELOG.md, its
                     heading dropped
  {stage_contract}   the stage-contract version the changelog preamble states
  {checksums}        the contents of a SHA256SUMS file, when one is given

The section heading must carry a date. A tag whose changelog section is not
finalized is the mistake this exists to catch; `--allow-unreleased` renders
the `## [Unreleased]` section instead, for the release workflow's dry run.

  make_release_notes.py --version 0.1.0 --checksums dist-release/SHA256SUMS \\
      --out release-notes.md
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parents[1]
FINALIZED = re.compile(r"^## \[(\d+\.\d+\.\d+)\] - \d{4}-\d{2}-\d{2}\s*$")


def section(changelog: str, name: str) -> tuple[str, str]:
    """(heading, body) of the `## [name]` section."""
    lines = changelog.splitlines()
    start = next((i for i, line in enumerate(lines)
                  if line.startswith(f"## [{name}]")), None)
    if start is None:
        raise SystemExit(f"error: CHANGELOG.md has no '## [{name}]' section")
    end = next((j for j in range(start + 1, len(lines))
                if lines[j].startswith("## ") or re.match(r"^\[[^\]]+\]: ", lines[j])),
               len(lines))
    return lines[start], "\n".join(lines[start + 1:end]).strip()


def stage_contract(changelog: str) -> str:
    match = re.search(r"Stage-contract version: \*\*(\d+)\*\*", changelog)
    if not match:
        raise SystemExit("error: CHANGELOG.md states no 'Stage-contract version: **N**'")
    return match.group(1)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--version",
                        default=(REPO / "VERSION").read_text(encoding="utf-8").strip())
    parser.add_argument("--changelog", type=pathlib.Path, default=REPO / "CHANGELOG.md")
    parser.add_argument("--template", type=pathlib.Path,
                        default=REPO / "docs" / "contributing" / "RELEASE_NOTES_TEMPLATE.md")
    parser.add_argument("--checksums", type=pathlib.Path)
    parser.add_argument("--allow-unreleased", action="store_true",
                        help="render the [Unreleased] section when the version has none")
    parser.add_argument("--out", type=pathlib.Path)
    args = parser.parse_args()

    changelog = args.changelog.read_text(encoding="utf-8")
    if args.allow_unreleased and f"## [{args.version}]" not in changelog:
        _, body = section(changelog, "Unreleased")
    else:
        heading, body = section(changelog, args.version)
        if not FINALIZED.match(heading):
            raise SystemExit(f"error: the changelog heading {heading!r} is not "
                             f"'## [{args.version}] - YYYY-MM-DD'")
    if not body:
        if not args.allow_unreleased:
            raise SystemExit(f"error: the changelog section for {args.version} is empty")
        body = "(no changes recorded yet)"

    checksums = "(written by the release workflow)"
    if args.checksums:
        checksums = args.checksums.read_text(encoding="utf-8").strip()

    notes = args.template.read_text(encoding="utf-8")
    for key, value in {"{version}": args.version, "{tag}": f"v{args.version}",
                       "{stage_contract}": stage_contract(changelog),
                       "{checksums}": checksums, "{changelog}": body}.items():
        notes = notes.replace(key, value)

    if args.out:
        args.out.write_text(notes, encoding="utf-8", newline="\n")
        print(f"wrote the release notes for v{args.version} to {args.out}")
    else:
        sys.stdout.buffer.write(notes.encode("utf-8"))  # not the console's code page
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
