#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Classify a complete change list; uncertainty always selects the full suite."""

from __future__ import annotations

import argparse
from fnmatch import fnmatchcase
import json
import os
from pathlib import Path
import re
import subprocess
import urllib.request


DOCS_ROOT = {
    "README.md", "CONTRIBUTING.md", "CHANGELOG.md", "CODE_OF_CONDUCT.md",
    "SECURITY.md", "THIRD_PARTY_NOTICES.md", "LICENSE",
}
PARSER_PATHS = (
    "libs/mmdPmx/*", "libs/mmdModel/*", "libs/motionVmd/*",
    "libs/mmdMotionBinding/*", "libs/mmdControl/*", "tests/fixtures/*",
    "cmake/*", ".github/*", "scripts/check_library_boundaries.py",
    "scripts/*ci*.py", "openstrata*", "strata*.lock", "VERSION",
    "CMakeLists.txt", "CMakePresets.json",
)


def is_document(path: str) -> bool:
    # Only actual documentation, never a code/fixture/manifest file under docs/.
    return path in DOCS_ROOT or (path.startswith("docs/") and path.endswith(".md"))


def full_plan(event_name: str) -> dict[str, bool]:
    return dict(docs_only=False, run_source=event_name != "push",
                run_parser=True, run_physics=True)


def classify(files: list[dict], event_name: str) -> dict[str, bool]:
    if not files:
        return full_plan(event_name)
    paths = []
    for item in files:
        path = item.get("filename", "")
        if (item.get("status") not in {"added", "modified"}
                or not path or "\\" in path or path.startswith("/")
                or any(part in {"", ".", ".."} for part in path.split("/"))):
            return full_plan(event_name)
        paths.append(path)
    docs_only = all(is_document(path) for path in paths)
    cheap_only = all(is_document(path) or path == "scripts/check_docs.py" for path in paths)
    # Known source areas retain the existing sanitizer coverage. Unknown paths
    # and changes to the classification/orchestration itself also run sanitizers.
    known = ("libs/", "plugins/", "tools/", "tests/", "cmake/", ".github/")
    parser = any(
        not is_document(path) and path != "scripts/check_docs.py"
        and (any(fnmatchcase(path, pattern) for pattern in PARSER_PATHS)
             or not path.startswith(known))
        for path in paths
    )
    return dict(docs_only=docs_only, run_source=not cheap_only and event_name != "push",
                run_parser=parser, run_physics=not cheap_only)


def api_json(path: str):
    url = os.environ.get("GITHUB_API_URL", "https://api.github.com") + path
    request = urllib.request.Request(url, headers={
        "Accept": "application/vnd.github+json",
        "Authorization": "Bearer " + os.environ["GH_TOKEN"],
        "X-GitHub-Api-Version": "2022-11-28",
    })
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.load(response)


def pr_files(event: dict, request=api_json) -> list[dict]:
    pr = event["pull_request"]
    endpoint = f"/repos/{event['repository']['full_name']}/pulls/{int(pr['number'])}"

    def snapshot():
        current = request(endpoint)
        if any(current[side]["sha"] != pr[side]["sha"] for side in ("base", "head")):
            raise ValueError("PR revisions changed while classifying")
        count = current["changed_files"]
        if not isinstance(count, int) or not 0 < count <= 3000:
            raise ValueError("PR file list is empty or exceeds the API limit")
        return count

    count = snapshot()
    files = []
    for page in range(1, (count + 99) // 100 + 1):
        batch = request(f"{endpoint}/files?per_page=100&page={page}")
        if not isinstance(batch, list):
            raise ValueError("Invalid PR files response")
        files.extend(batch)
    if (len(files) != count or len({f["filename"] for f in files}) != count
            or snapshot() != count):
        raise ValueError("Incomplete or changing PR file list")
    return files


def push_files(event: dict) -> list[dict]:
    revisions = [event[key] for key in ("before", "after")]
    if any(not re.fullmatch(r"[0-9a-f]{40}", rev) or set(rev) == {"0"}
           for rev in revisions):
        raise ValueError("Push has no usable comparison revisions")
    result = subprocess.run(
        ["git", "diff", "--no-renames", "--name-status", "-z", *revisions, "--"],
        check=True, capture_output=True, timeout=60,
    )
    fields = result.stdout.decode("utf-8").rstrip("\0").split("\0")
    if len(fields) % 2:
        raise ValueError("Invalid git change list")
    statuses = {"A": "added", "M": "modified", "D": "removed"}
    return [dict(filename=path, status=statuses.get(status, "unknown"))
            for status, path in zip(fields[::2], fields[1::2])]


def plan_for_event(event_name: str, event: dict) -> dict[str, bool]:
    try:
        if event_name == "pull_request":
            return classify(pr_files(event), event_name)
        if event_name == "push":
            return classify(push_files(event), event_name)
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError):
        # Do not print paths, API payloads or tokens from untrusted PR input.
        print("Change discovery unavailable; selecting all applicable CI lanes.")
    return full_plan(event_name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--github-output", type=Path, required=True)
    args = parser.parse_args()
    event_name = os.environ.get("GITHUB_EVENT_NAME", "workflow_dispatch")
    try:
        event = json.loads(Path(os.environ["GITHUB_EVENT_PATH"]).read_text(encoding="utf-8"))
        plan = plan_for_event(event_name, event)
    except (OSError, ValueError, KeyError):
        plan = full_plan(event_name)
    with args.github_output.open("a", encoding="utf-8") as out:
        for name, value in plan.items():
            out.write(f"{name}={str(value).lower()}\n")
    print(json.dumps(plan, sort_keys=True))


if __name__ == "__main__":
    main()
