#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check the facts documentation and manifests restate, which rot silently.

Links -- docs/contributing/documentation.md's change checklist: "Relative
links and heading anchors resolve." For every `[text](target)` outside code in
every Markdown file, this fails when:

  * a relative path names a file or directory that does not exist;
  * a `#fragment` names no heading in the target Markdown file, using
    GitHub's heading-slug rules (so `§14.1 First release — done` is
    `#141-first-release--done`);
  * a link to an absolute path or a machine-local path (`C:\\...`, `/home/...`)
    appears at all -- documents never commit one.

External links (`http:`, `https:`, `mailto:`) are not fetched.

Mirrors -- docs/architecture/WORKSPACE.md §4: the repository-root VERSION is
the single product version, and openstrata.toml and every component manifest
mirror it, every CMake project() takes it from cmake/UsdMmdProject.cmake
rather than restating it, every range a manifest requires a sibling in
admits it, and so does every version the installed-consumer lane asks
`find_package` for; cmake/UsdMmdOpenUsd.cmake's pin and every
bundle manifest's `runtime.openusd` name the same OpenUSD release.

Diagnostics -- docs/reference/DIAGNOSTICS.md §5 is the catalog, and each
component's *Codes.h declares the codes it raises: every declared code must be
catalogued as *emitted* with its declared severity, and every *emitted* code
must be declared.

Ownership -- required document owners and index links must exist. Additional
capability/current-status tables and README/design progress markers are
advisory during cleanup; historical reports/releases are exempt.

  check_docs.py             check the repository
  check_docs.py --selftest  check link, mirror and ownership rules against known cases
"""

from __future__ import annotations

import pathlib
import re
import subprocess
import sys
import unicodedata

REPO = pathlib.Path(__file__).resolve().parents[1]
SKIP_DIRS = {".git", "build", "dist", ".strata", "node_modules", "__pycache__",
             ".claude", "local", ".ost-ci", ".ost-ci-home"}

FENCE = re.compile(r"^\s*(```|~~~)")
INLINE_CODE = re.compile(r"(`+)(?:(?!\1).)+?\1")
LINK = re.compile(r"(?<!\!)\[(?:[^\[\]]|\[[^\]]*\])*\]\(\s*<?([^)\s>]+)>?(?:\s+\"[^\"]*\")?\s*\)")
IMAGE = re.compile(r"!\[[^\]]*\]\(\s*<?([^)\s>]+)>?\s*\)")
HEADING = re.compile(r"^(#{1,6})\s+(.*?)\s*#*\s*$")
EXTERNAL = re.compile(r"^[a-zA-Z][a-zA-Z0-9+.-]*:")
MACHINE_LOCAL = re.compile(r"^(?:[A-Za-z]:[\\/]|/(?:home|Users|tmp)/|\\\\)")


def slug(heading: str) -> str:
    """GitHub's anchor for a heading's text."""
    text = re.sub(r"!?\[([^\]]*)\]\([^)]*\)", r"\1", heading)  # links -> text
    text = re.sub(r"<[^>]+>", "", text)                          # inline HTML
    text = text.replace("`", "").strip().lower()
    kept = []
    for ch in text:
        category = unicodedata.category(ch)
        if ch in " -_" or category[0] in {"L", "N"} or category == "Mn":
            kept.append(ch)
    return "".join(kept).replace(" ", "-")


def markdown_lines(path: pathlib.Path):
    """(line number, text) for every line outside a fenced code block, with
    inline code spans blanked so a link inside backticks is not a link."""
    in_fence = False
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if FENCE.match(line):
            in_fence = not in_fence
            continue
        if not in_fence:
            yield number, INLINE_CODE.sub(lambda m: " " * len(m.group(0)), line)


def anchors(path: pathlib.Path, cache: dict[pathlib.Path, set[str]]) -> set[str]:
    if path not in cache:
        seen: dict[str, int] = {}
        found: set[str] = set()
        in_fence = False
        for line in path.read_text(encoding="utf-8").splitlines():
            if FENCE.match(line):
                in_fence = not in_fence
                continue
            match = None if in_fence else HEADING.match(line)
            if match:
                base = slug(match.group(2))
                count = seen.get(base, 0)
                found.add(base if count == 0 else f"{base}-{count}")
                seen[base] = count + 1
        cache[path] = found
    return cache[path]


def markdown_files(root: pathlib.Path) -> list[pathlib.Path]:
    """The repository's own Markdown: what git tracks or would track.

    Asking git rather than walking the tree matters in CI, where the checkout
    also holds the bootstrapped `ost` and the materialized runtime, each with
    Markdown of its own whose links point into trees that are not here.
    Walking the directory is the fallback for a tree without git.
    """
    try:
        listed = subprocess.run(
            ["git", "-c", f"safe.directory={root}", "-C", str(root),
             "ls-files", "-z", "--cached", "--others",
             "--exclude-standard", "--", "*.md"],
            check=True, stdout=subprocess.PIPE).stdout.decode("utf-8")
        files = [root / name for name in listed.split("\0") if name]
        return sorted(path for path in files if path.is_file())
    except (OSError, subprocess.CalledProcessError):
        pass
    files = []
    for path in root.rglob("*.md"):
        if not any(part in SKIP_DIRS for part in path.relative_to(root).parts):
            files.append(path)
    return sorted(files)


def check_file(path: pathlib.Path, cache: dict) -> list[str]:
    errors: list[str] = []
    where = (path.relative_to(REPO).as_posix() if path.is_relative_to(REPO)
             else path.name)
    for number, line in markdown_lines(path):
        for match in [*LINK.finditer(line), *IMAGE.finditer(line)]:
            target = match.group(1)
            if EXTERNAL.match(target) and not MACHINE_LOCAL.match(target):
                continue
            if MACHINE_LOCAL.match(target) or target.startswith("/"):
                errors.append(f"{where}:{number}: absolute or machine-local "
                              f"link {target}")
                continue
            file_part, _, fragment = target.partition("#")
            resolved = (path.parent / file_part).resolve() if file_part else path
            if not resolved.exists():
                errors.append(f"{where}:{number}: {file_part} does not exist")
                continue
            if fragment:
                if resolved.is_dir() or resolved.suffix.lower() != ".md":
                    errors.append(f"{where}:{number}: #{fragment} on a "
                                  f"non-Markdown target {file_part}")
                elif fragment not in anchors(resolved, cache):
                    errors.append(f"{where}:{number}: {file_part or where} has "
                                  f"no heading #{fragment}")
    return errors


# One `requires.libraries` row. External libraries have their own version and
# are deliberately not compared with this repository's VERSION.
REQUIRED_RANGE = re.compile(
    r'^\s+- id:\s*([^\s#]+)\s*\n\s+version:\s*">=([0-9.]+),<([0-9.]+)"',
    re.MULTILINE)
FIND_PACKAGE_VERSION = re.compile(r"find_package\((\w+)\s+([0-9.]+)\s+CONFIG")


def in_range(version: str, lower: str, upper: str) -> bool:
    def key(text: str) -> tuple[int, ...]:
        parts = [int(p) for p in text.split(".")]
        return tuple(parts + [0] * (3 - len(parts)))
    return key(lower) <= key(version) < key(upper)


def check_ranges(root: pathlib.Path, manifest: pathlib.Path, version: str,
                 sibling_ids: set[str]) -> list[str]:
    """Every sibling this workspace requires is built at VERSION, so every
    required range has to admit it -- or the release cannot resolve itself."""
    where = manifest.relative_to(root).as_posix()
    return [f"{where}: required range >={lower},<{upper} excludes {version}"
            for dependency, lower, upper in REQUIRED_RANGE.findall(
                manifest.read_text(encoding="utf-8"))
            if dependency in sibling_ids and not in_range(version, lower, upper)]


# `project(mmdPmx VERSION ${USDMMD_VERSION} ...)`: the argument after VERSION.
PROJECT_VERSION = re.compile(r"^project\(\w+\s+VERSION\s+(\S+)", re.MULTILINE)


def component_files(root: pathlib.Path, pattern: str) -> list[pathlib.Path]:
    """Source components only, not old installs under build/ or .strata/."""
    return sorted(path for category in ("libs", "plugins", "tools")
                  for path in (root / category).glob(f"*/{pattern}"))


def check_mirrors(root: pathlib.Path) -> list[str]:
    errors: list[str] = []
    version = (root / "VERSION").read_text(encoding="utf-8").strip()

    def expect(path: pathlib.Path, pattern: str, want: str, what: str) -> None:
        where = path.relative_to(root).as_posix()
        match = re.search(pattern, path.read_text(encoding="utf-8"), re.MULTILINE)
        if not match:
            errors.append(f"{where}: no {what} found")
        elif match.group(1) != want:
            errors.append(f"{where}: {what} is {match.group(1)}, expected {want}")

    expect(root / "openstrata.toml", r'^version\s*=\s*"([^"]+)"', version,
           "project version")
    manifests = component_files(root, "openstrata.*.yaml")
    sibling_ids = {manifest.parent.name for manifest in manifests}
    for manifest in manifests:
        expect(manifest, r"^\s+version:\s*([0-9][^\s#]*)", version, "version")
        errors.extend(check_ranges(root, manifest, version, sibling_ids))
    for cmake in [root / "CMakeLists.txt", *component_files(root, "CMakeLists.txt")]:
        project = PROJECT_VERSION.search(cmake.read_text(encoding="utf-8"))
        if project and project.group(1) != "${USDMMD_VERSION}":
            errors.append(f"{cmake.relative_to(root).as_posix()}: project() "
                          f"VERSION is {project.group(1)}, expected "
                          "${USDMMD_VERSION} (cmake/UsdMmdProject.cmake)")

    consumer = root / "tests" / "installed_consumer" / "CMakeLists.txt"
    for package, requested in FIND_PACKAGE_VERSION.findall(
            consumer.read_text(encoding="utf-8")):
        # The packages are written with SameMinorVersion compatibility.
        if requested != ".".join(version.split(".")[:2]):
            errors.append(f"{consumer.relative_to(root).as_posix()}: "
                          f"find_package({package} {requested}) cannot find "
                          f"{version}")

    pin = re.search(r'USDMMD_OPENUSD_REQUIRED_RELEASE "([^"]+)"',
                    (root / "cmake" / "UsdMmdOpenUsd.cmake").read_text(encoding="utf-8"))
    if not pin:
        errors.append("cmake/UsdMmdOpenUsd.cmake: no USDMMD_OPENUSD_REQUIRED_RELEASE")
    else:
        for manifest in sorted(root.glob("plugins/*/openstrata.plugin.yaml")):
            expect(manifest, r'^\s+openusd:\s*"==([^"]+)"', pin.group(1),
                   "runtime.openusd pin")
    return errors


# `inline constexpr Code PmxBadSignature{"MMD_PMX_BAD_SIGNATURE", Severity::Fatal};`
CODE_DECLARATION = re.compile(
    r'inline\s+constexpr\s+(?:mmd::)?Code\s+\w+\s*\{\s*"(MMD_[A-Z0-9_]+)"\s*,'
    r'\s*(?:mmd::)?Severity::(\w+)\s*\}')
# `| `MMD_PMX_BAD_SIGNATURE` *emitted* | fatal | ...`
CATALOG_ROW = re.compile(
    r"^\|\s*`(MMD_[A-Z0-9_]+)`(\s*\*emitted\*)?[^|]*\|\s*(\w+)", re.MULTILINE)


def declared_codes(root: pathlib.Path) -> dict[str, tuple[str, str]]:
    """Every code a component declares: id -> (severity, declaring file)."""
    found: dict[str, tuple[str, str]] = {}
    for pattern in ("libs/*/include/**/*Codes.h", "plugins/*/src/**/*Codes.h",
                    "tools/*/src/**/*Codes.h"):
        for header in sorted(root.glob(pattern)):
            where = header.relative_to(root).as_posix()
            for code, severity in CODE_DECLARATION.findall(
                    header.read_text(encoding="utf-8")):
                found[code] = (severity.lower(), where)
    return found


def check_diagnostics(root: pathlib.Path) -> list[str]:
    """DIAGNOSTICS.md §5 against the code: every declared code is catalogued
    as *emitted* with the severity it is declared with, and every code the
    catalog calls *emitted* is declared (DIAGNOSTICS.md, status)."""
    catalog_path = root / "docs" / "reference" / "DIAGNOSTICS.md"
    text = catalog_path.read_text(encoding="utf-8")
    section = text.split("## 5. ", 1)[-1]
    catalog = {code: (bool(emitted), severity.lower())
               for code, emitted, severity in CATALOG_ROW.findall(section)}
    declared = declared_codes(root)
    errors: list[str] = []
    for code, (severity, where) in sorted(declared.items()):
        if code not in catalog:
            errors.append(f"{where}: {code} is not in DIAGNOSTICS.md §5")
            continue
        emitted, listed = catalog[code]
        if not emitted:
            errors.append(f"DIAGNOSTICS.md §5: {code} is declared in {where} "
                          f"but not marked *emitted*")
        if listed != severity:
            errors.append(f"DIAGNOSTICS.md §5: {code} is listed as {listed}, "
                          f"declared {severity} in {where}")
    for code, (emitted, _) in sorted(catalog.items()):
        if emitted and code not in declared:
            errors.append(f"DIAGNOSTICS.md §5: {code} is marked *emitted* but "
                          f"no component declares it")
    return errors


DOC_OWNERS = (
    "docs/design/DESIGN_POLICY.md", "docs/design/STAGE_CONTRACT.md",
    "docs/design/PMX_CONTRACT.md", "docs/design/MATERIAL_POLICY.md",
    "docs/design/TEXT_ENCODING_POLICY.md", "docs/design/MOTION_CONTRACT.md",
    "docs/design/PHYSICS_INTEGRATION.md", "docs/design/PACKAGING_POLICY.md",
    "docs/architecture/WORKSPACE.md", "docs/architecture/DEPENDENCIES.md",
    "docs/architecture/PACKAGE_CONTRACT.md",
    "docs/reference/CAPABILITY_MATRIX.md", "docs/reference/DIAGNOSTICS.md",
    "docs/reference/SOURCE_MAPPING.md", "docs/roadmap/current.md",
    "docs/roadmap/backlog.md",
    "docs/contributing/documentation.md", "docs/releases/README.md",
)


def check_doc_owners(root: pathlib.Path) -> list[str]:
    """Keep the existing owners discoverable; no synonymous physics contract."""
    errors = [f"{name}: required owning document is missing"
              for name in ("README.md", "docs/README.md", "docs/roadmap/README.md",
                           *DOC_OWNERS) if not (root / name).is_file()]
    index = root / "docs/README.md"
    if index.is_file():
        linked = {(index.parent / match.group(1).split("#", 1)[0]).resolve()
                  for _, line in markdown_lines(index)
                  for match in LINK.finditer(line)
                  if not EXTERNAL.match(match.group(1))}
        errors.extend(f"docs/README.md: missing link to owner {name}"
                      for name in DOC_OWNERS if (root / name).resolve() not in linked)
    if (root / "docs/design/PHYSICS_CONTRACT.md").exists():
        errors.append("docs/design/PHYSICS_CONTRACT.md: duplicate physics owner; "
                      "use PHYSICS_INTEGRATION.md or revise ownership explicitly")
    return errors


def doc_advisories(root: pathlib.Path, files: list[pathlib.Path]) -> list[str]:
    """Heuristics only: warn about drift, never block on wording/line count."""
    warnings: list[str] = []
    for path in files:
        where = path.relative_to(root).as_posix()
        if where.startswith(("docs/reports/", "docs/releases/")):
            continue
        lines = list(markdown_lines(path))
        if where != "docs/reference/CAPABILITY_MATRIX.md":
            for number, line in lines:
                if not line.lstrip().startswith("|"):
                    continue
                cells = {cell.strip().lower() for cell in line.strip().strip("|").split("|")}
                if (("capability" in cells and bool(cells & {"current", "status"}))
                        or ("feature" in cells and "execution claim and limits" in cells)):
                    warnings.append(f"{where}:{number}: capability table outside "
                                    "CAPABILITY_MATRIX.md; link to the owner")
                    break
        if where != "docs/roadmap/current.md":
            for number, line in lines:
                cells = {cell.strip().lower() for cell in line.strip().strip("|").split("|")}
                if line.lstrip().startswith("|") and {"id", "owner", "state"} <= cells:
                    warnings.append(f"{where}:{number}: current-work table outside "
                                    "roadmap/current.md; link to the owner")
                    break
        if where in {"README.md", "docs/README.md"} or where.startswith("docs/design/"):
            for number, line in lines:
                progress = re.search(r"✅|🚧|\bas of\b", line, re.IGNORECASE)
                status = re.search(r"\bStatus:", line, re.IGNORECASE)
                normative = re.search(r"\b(binding|accepted|proposed|superseded|rejected)\b", line, re.IGNORECASE)
                if progress or (status and not normative):
                    warnings.append(f"{where}:{number}: progress wording in entry/contract; "
                                    "consider current.md or a dated report")
                    break
    return warnings


def selftest() -> int:
    cases = {
        "14.1 First substantial release — definition of done":
            "141-first-substantial-release--definition-of-done",
        "5.1 `mmdPmx` — the PMX syntax parser": "51-mmdpmx--the-pmx-syntax-parser",
        "19. Where this document departs from the implementation policy":
            "19-where-this-document-departs-from-the-implementation-policy",
        "Status at a glance": "status-at-a-glance",
        "4.1 Why `/Asset` is the SkelRoot": "41-why-asset-is-the-skelroot",
        "6. MaterialX `gltf_pbr` realization": "6-materialx-gltf_pbr-realization",
        "左腕 と [link](x.md)": "左腕-と-link",
    }
    failures = [f"slug({h!r}) = {slug(h)!r}, expected {want!r}"
                for h, want in cases.items() if slug(h) != want]
    links = LINK.findall("see [a](b.md#c) and ![i](img.png)")
    if links != ["b.md#c"]:
        failures.append(f"LINK found {links}")
    stripped = INLINE_CODE.sub(lambda m: " " * len(m.group(0)), "`[x](y.md)`")
    if LINK.search(stripped):
        failures.append("a link inside a code span is checked")
    if not MACHINE_LOCAL.match("C:\\dev\\x.md") or not MACHINE_LOCAL.match("/home/u/x"):
        failures.append("machine-local paths are not recognized")

    for version, lower, upper, want in [("0.1.0", "0.1", "0.2", True),
                                        ("0.1.0", "0.0", "0.1", False),
                                        ("0.1.9", "0.1", "0.2", True),
                                        ("1.0.0", "0.1", "1.0", False)]:
        if in_range(version, lower, upper) != want:
            failures.append(f"in_range({version}, {lower}, {upper}) != {want}")
    ranges = REQUIRED_RANGE.findall('    - id: a\n      version: ">=0.1,<0.2"\n')
    if ranges != [("a", "0.1", "0.2")]:
        failures.append(f"REQUIRED_RANGE found {ranges}")

    declarations = CODE_DECLARATION.findall(
        'inline constexpr Code PmxX{"MMD_PMX_X", Severity::Fatal};\n'
        'inline constexpr mmd::Code Y{\n    "MMD_Y_Z", mmd::Severity::Warning};\n')
    if declarations != [("MMD_PMX_X", "Fatal"), ("MMD_Y_Z", "Warning")]:
        failures.append(f"CODE_DECLARATION found {declarations}")
    rows = CATALOG_ROW.findall(
        "| `MMD_A` *emitted* | fatal | x |\n| `MMD_B` | error | y |\n"
        "| `MMD_C` *emitted* (header) | warning | z |\n")
    if [(c, bool(e), s) for c, e, s in rows] != [
            ("MMD_A", True, "fatal"), ("MMD_B", False, "error"),
            ("MMD_C", True, "warning")]:
        failures.append(f"CATALOG_ROW found {rows}")

    # The whole rule, on files: one good link, and one of each kind of bad.
    import tempfile
    with tempfile.TemporaryDirectory() as scratch:
        root = pathlib.Path(scratch)
        (root / "target.md").write_text("# Title\n\n## 2.1 Some `code` — part\n",
                                        encoding="utf-8")
        page = root / "page.md"
        page.write_text(
            "[ok](target.md#21-some-code--part)\n"
            "[no file](missing.md)\n"
            "[no anchor](target.md#nowhere)\n"
            "[local](C:\\dev\\target.md)\n"
            "```\n[in a fence](missing.md)\n```\n",
            encoding="utf-8")
        found = check_file(page, {})
        if len(found) != 3 or not all(
                any(word in e for e in found)
                for word in ("missing.md does not exist", "#nowhere",
                             "machine-local")):
            failures.append(f"check_file reported {found}")

        # A previous install must not become a source component or invalidate
        # today's version mirrors. A stale source manifest must still fail.
        (root / "VERSION").write_text("0.2.0\n", encoding="utf-8")
        (root / "openstrata.toml").write_text('version = "0.2.0"\n', encoding="utf-8")
        (root / "CMakeLists.txt").write_text("project(Test LANGUAGES CXX)\n", encoding="utf-8")
        for directory in ("cmake", "tests/installed_consumer", "libs/example",
                          "build/old-install", ".strata/old-install"):
            (root / directory).mkdir(parents=True)
        (root / "cmake/UsdMmdOpenUsd.cmake").write_text(
            'set(USDMMD_OPENUSD_REQUIRED_RELEASE "26.08")\n', encoding="utf-8")
        (root / "tests/installed_consumer/CMakeLists.txt").write_text(
            "find_package(mmdModel 0.2 CONFIG REQUIRED)\n", encoding="utf-8")
        source_manifest = root / "libs/example/openstrata.library.yaml"
        source_manifest.write_text("library:\n  version: 0.2.0\n", encoding="utf-8")
        for directory in ("build/old-install", ".strata/old-install"):
            (root / directory / "openstrata.plugin.yaml").write_text(
                "plugin:\n  version: 0.1.0\n", encoding="utf-8")
            (root / directory / "CMakeLists.txt").write_text(
                "project(Old VERSION 0.1.0)\n", encoding="utf-8")
        if check_mirrors(root):
            failures.append("old installed manifests/CMake projects affect source mirrors")
        source_manifest.write_text("library:\n  version: 0.1.0\n", encoding="utf-8")
        found = check_mirrors(root)
        if len(found) != 1 or "libs/example/openstrata.library.yaml: version" not in found[0]:
            failures.append(f"stale source manifest was not diagnosed: {found}")

        # Missing owners/index links are errors; duplicate summaries only warn.
        for name in ("README.md", "docs/roadmap/README.md", *DOC_OWNERS):
            target = root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text("# Contract\n", encoding="utf-8")
        index = root / "docs/README.md"
        index.write_text("# Docs\n" + "\n".join(
            f"[owner]({name.removeprefix('docs/')})" for name in DOC_OWNERS),
            encoding="utf-8")
        if check_doc_owners(root):
            failures.append("complete owner tree was rejected")
        owner = root / "docs/design/PHYSICS_INTEGRATION.md"
        owner.unlink()
        if not any("required owning document is missing" in e for e in check_doc_owners(root)):
            failures.append("missing owner was not diagnosed")
        owner.write_text("# Contract\n", encoding="utf-8")
        index.write_text("# Docs\n", encoding="utf-8")
        if not any("missing link to owner" in e for e in check_doc_owners(root)):
            failures.append("missing owner index links were not diagnosed")
        alias = root / "docs/design/PHYSICS_CONTRACT.md"
        alias.write_text("# Duplicate\n", encoding="utf-8")
        if not any("duplicate physics owner" in e for e in check_doc_owners(root)):
            failures.append("duplicate physics contract was not diagnosed")
        owner.write_text(
            "# Contract\n> Status: **binding**\n```\n✅ example\n```\n",
            encoding="utf-8")
        if doc_advisories(root, [owner]):
            failures.append("binding label/fenced example produced a progress warning")
        owner.write_text(
            "# Contract\n| Capability | Current |\n| --- | --- |\n"
            "| ID | Owner | State |\n✅ done\n", encoding="utf-8")
        if len(doc_advisories(root, [owner])) != 3:
            failures.append("capability/current/progress advisories were not emitted")
        report = root / "docs/reports/evidence.md"
        report.parent.mkdir(parents=True)
        report.write_text(owner.read_text(encoding="utf-8"), encoding="utf-8")
        if doc_advisories(root, [report]):
            failures.append("historical report was linted as current status")

    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    print("check_docs selftest passed")
    return 0


def main() -> int:
    if sys.argv[1:] == ["--selftest"]:
        return selftest()
    cache: dict[pathlib.Path, set[str]] = {}
    files = markdown_files(REPO)
    errors = [e for path in files for e in check_file(path, cache)]
    errors += check_mirrors(REPO)
    errors += check_diagnostics(REPO)
    errors += check_doc_owners(REPO)
    for warning in doc_advisories(REPO, files):
        print(f"advisory: {warning}", file=sys.stderr)
    if errors:
        print("\n".join(errors), file=sys.stderr)
        print(f"{len(errors)} problem(s)", file=sys.stderr)
        return 1
    print(f"{len(files)} Markdown file(s): every relative link and anchor "
          f"resolves; every version and pin mirror agrees; the diagnostic "
          f"catalog matches the {len(declared_codes(REPO))} declared codes; "
          "required document owners and index links exist")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
