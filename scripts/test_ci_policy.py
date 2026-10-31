#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""CI policy regression tests: selection, complete diffs and required results."""

import copy
import hashlib
import io
import itertools
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import zipfile

import yaml

from check_ci_gate import check_gate
from classify_ci_changes import classify, full_plan, plan_for_event, pr_files, push_files
from generate_source_ci import adapt, download_ost


ROOT = Path(__file__).resolve().parents[1]


def changes(*paths, status="modified"):
    return [dict(filename=path, status=status) for path in paths]


class ClassificationTest(unittest.TestCase):
    def test_all_documents_are_cheap(self):
        plan = classify(changes("README.md", "docs/architecture/CI_POLICY.md",
                                "docs/reference/DIAGNOSTICS.md", "LICENSE"), "pull_request")
        self.assertEqual(plan, dict(docs_only=True, run_source=False,
                                    run_parser=False, run_physics=False))

    def test_mixed_change_runs_source(self):
        plan = classify(changes("README.md", "plugins/mmdSchema/src/schema.cpp"), "pull_request")
        self.assertFalse(plan["docs_only"])
        self.assertTrue(plan["run_source"])
        self.assertTrue(plan["run_physics"])

    def test_check_docs_script_is_cheap_but_not_docs_only(self):
        self.assertEqual(classify(changes("scripts/check_docs.py"), "pull_request"),
                         dict(docs_only=False, run_source=False, run_parser=False, run_physics=False))

    def test_infrastructure_and_unknown_changes_are_full(self):
        for path in (".github/workflows/ci-gate.yml", "VERSION", "openstrata.ci.yaml",
                     "openstrata.toml", "strata.lock", "CMakeLists.txt", "CMakePresets.json",
                     "scripts/classify_ci_changes.py", "new-directory/unknown.txt",
                     "docs/generate_reference.py", "docs/fixture.usda"):
            with self.subTest(path=path):
                self.assertEqual(classify(changes(path), "pull_request"), full_plan("pull_request"))

    def test_original_sanitizer_coverage_is_preserved(self):
        for directory in ("mmdPmx", "mmdModel", "motionVmd", "mmdMotionBinding", "mmdControl"):
            self.assertTrue(classify(changes(f"libs/{directory}/src/main.cpp"), "pull_request")["run_parser"])
        for path in ("tests/fixtures/generate_fixtures.py", "tests/fixtures/generate_vmd_fixtures.py",
                     "scripts/check_library_boundaries.py", "cmake/UsdMmdSanitizers.cmake"):
            self.assertTrue(classify(changes(path), "pull_request")["run_parser"])

    def test_plugin_only_does_not_add_sanitizer_work(self):
        self.assertFalse(classify(changes("plugins/mmdImaging/src/adapter.cpp"), "pull_request")["run_parser"])

    def test_deletion_rename_and_malformed_paths_are_full(self):
        for status in ("removed", "renamed", "copied", "unknown", ""):
            with self.subTest(status=status):
                entry = dict(filename="docs/new.md", previous_filename="libs/mmdPmx/a.cpp", status=status)
                self.assertEqual(classify([entry], "pull_request"), full_plan("pull_request"))
        for path in ("", "docs/../VERSION", "docs//README.md", "docs/./README.md", "/README.md", "docs\\README.md"):
            with self.subTest(path=path):
                self.assertEqual(classify(changes(path), "pull_request"), full_plan("pull_request"))

    def test_no_diff_is_full(self):
        self.assertEqual(classify([], "pull_request"), full_plan("pull_request"))

    def test_push_preserves_no_source_build_policy(self):
        self.assertFalse(classify(changes("libs/mmdControl/src/a.cpp"), "push")["run_source"])
        self.assertFalse(classify(changes("README.md"), "push")["run_physics"])

    def test_failure_and_manual_dispatch_select_full(self):
        with patch("classify_ci_changes.pr_files", side_effect=OSError("network unavailable")):
            self.assertEqual(plan_for_event("pull_request", {}), full_plan("pull_request"))
        with patch("classify_ci_changes.push_files", side_effect=subprocess.CalledProcessError(1, "git")):
            self.assertEqual(plan_for_event("push", {}), full_plan("push"))
        self.assertEqual(plan_for_event("workflow_dispatch", {}), full_plan("workflow_dispatch"))


class DiscoveryTest(unittest.TestCase):
    def setUp(self):
        self.pr = dict(number=123, head=dict(sha="a" * 40), base=dict(sha="b" * 40), changed_files=201)
        self.event = dict(repository=dict(full_name="owner/repo"), pull_request=self.pr)
        self.files = changes(*(f"docs/{i}.md" for i in range(201)))

    def request(self, url):
        if "/files?" not in url:
            return copy.deepcopy(self.pr)
        page = int(url.split("page=")[-1])
        return self.files[(page - 1) * 100:page * 100]

    def test_pagination_and_mixed_file_on_last_page(self):
        self.files[-1] = changes("libs/mmdPmx/src/parser.cpp")[0]
        files = pr_files(self.event, self.request)
        self.assertEqual(len(files), 201)
        self.assertTrue(classify(files, "pull_request")["run_source"])

    def test_truncated_and_duplicate_lists_rejected(self):
        for files in (self.files[:-1], self.files[:-1] + self.files[:1]):
            with self.subTest(length=len(files)):
                original, self.files = self.files, files
                with self.assertRaises(ValueError):
                    pr_files(self.event, self.request)
                self.files = original

    def test_over_api_limit_rejected_before_paging(self):
        self.pr["changed_files"] = 3001
        with self.assertRaises(ValueError):
            pr_files(self.event, self.request)

    def test_head_or_base_races_rejected(self):
        for side in ("head", "base"):
            def request(url):
                result = self.request(url)
                if "/files?" not in url:
                    result[side]["sha"] = "c" * 40
                return result
            with self.subTest(side=side), self.assertRaises(ValueError):
                pr_files(self.event, request)

    def test_change_during_pagination_rejected(self):
        calls = 0

        def request(url):
            nonlocal calls
            result = self.request(url)
            if "/files?" not in url:
                calls += 1
                if calls == 2:
                    result["head"]["sha"] = "c" * 40
            return result

        with self.assertRaises(ValueError):
            pr_files(self.event, request)

    def test_push_uses_complete_nul_delimited_diff(self):
        completed = subprocess.CompletedProcess([], 0, b"M\0docs/with\nnewline.md\0D\0README.md\0")
        with patch("classify_ci_changes.subprocess.run", return_value=completed) as run:
            files = push_files(dict(before="a" * 40, after="b" * 40))
        self.assertIn("--no-renames", run.call_args.args[0])
        self.assertEqual(files, [dict(filename="docs/with\nnewline.md", status="modified"),
                                 dict(filename="README.md", status="removed")])
        self.assertEqual(classify(files, "push"), full_plan("push"))

    def test_push_unusable_revisions_rejected(self):
        with self.assertRaises(ValueError):
            push_files(dict(before="0" * 40, after="b" * 40))


class GateTest(unittest.TestCase):
    def needs(self, flags):
        jobs = ("source", "parser", "physics")
        result = dict(changes=dict(result="success", outputs={
            f"run_{job}": str(flag).lower() for job, flag in zip(jobs, flags)
        }), docs=dict(result="success"))
        result.update({job: dict(result="success" if flag else "skipped")
                       for job, flag in zip(jobs, flags)})
        return result

    def test_all_valid_selection_combinations(self):
        for flags in itertools.product((False, True), repeat=3):
            check_gate(self.needs(flags))

    def test_selected_failure_cancellation_skip_or_missing_result_fails(self):
        for job in ("source", "parser", "physics", "changes", "docs"):
            for status in ("failure", "cancelled", "skipped", ""):
                needs = self.needs((True, True, True))
                needs[job]["result"] = status
                with self.subTest(job=job, status=status), self.assertRaises(ValueError):
                    check_gate(needs)

    def test_unselected_job_and_invalid_selection_fail(self):
        needs = self.needs((False, False, False))
        needs["physics"]["result"] = "failure"
        with self.assertRaises(ValueError):
            check_gate(needs)
        for invalid in (None, "", "yes", True):
            needs = self.needs((True, True, True))
            needs["changes"]["outputs"]["run_source"] = invalid
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                check_gate(needs)


class BootstrapTest(unittest.TestCase):
    def archive(self, system):
        buffer = io.BytesIO()
        if system == "Windows":
            with zipfile.ZipFile(buffer, "w") as archive:
                archive.writestr("bin/ost.exe", b"test-cli")
        else:
            with tarfile.open(fileobj=buffer, mode="w:xz") as archive:
                entry = tarfile.TarInfo("bin/ost")
                entry.size = len(b"test-cli")
                archive.addfile(entry, io.BytesIO(b"test-cli"))
        return buffer.getvalue()

    def bootstrap(self, system, machine, triple, extension, *, checksum=None, pinned=None):
        archive = self.archive(system)
        digest = hashlib.sha256(archive).hexdigest()
        expected_url = f"https://github.com/owner/repo/releases/download/v1.2.3/ost-cli-{triple}.{extension}"

        def fetch(url, **kwargs):
            if url == expected_url:
                return io.BytesIO(archive)
            self.assertEqual(url, expected_url + ".sha256")
            return io.BytesIO(((checksum or digest) + "  archive\n").encode("ascii"))

        pin = dict(repository="owner/repo", version="1.2.3", sha256={triple: pinned or digest})
        with tempfile.TemporaryDirectory() as scratch, \
                patch("generate_source_ci.platform.system", return_value=system), \
                patch("generate_source_ci.platform.machine", return_value=machine), \
                patch("generate_source_ci.urllib.request.urlopen", side_effect=fetch):
            binary = Path(download_ost(Path(scratch), pin))
            self.assertEqual(binary.name, "ost.exe" if system == "Windows" else "ost")
            self.assertEqual(binary.read_bytes(), b"test-cli")

    def test_published_platform_archives(self):
        for system, machine, triple, extension in (
            ("Windows", "AMD64", "x86_64-pc-windows-msvc", "zip"),
            ("Linux", "x86_64", "x86_64-unknown-linux-musl", "tar.xz"),
            ("Darwin", "arm64", "aarch64-apple-darwin", "tar.xz"),
        ):
            with self.subTest(system=system):
                self.bootstrap(system, machine, triple, extension)

    def test_bad_release_checksum_rejected(self):
        with self.assertRaisesRegex(ValueError, "release checksum mismatch"):
            self.bootstrap("Linux", "x86_64", "x86_64-unknown-linux-musl", "tar.xz", checksum="0" * 64)

    def test_per_triple_manifest_checksum_is_enforced(self):
        with self.assertRaisesRegex(ValueError, "differs from manifest pin"):
            self.bootstrap("Linux", "x86_64", "x86_64-unknown-linux-musl", "tar.xz", pinned="0" * 64)


class WorkflowTest(unittest.TestCase):
    def load(self, filename):
        return yaml.load((ROOT / ".github/workflows" / filename).read_text(encoding="utf-8"),
                         Loader=yaml.BaseLoader)

    def test_only_orchestrator_and_release_have_automatic_triggers(self):
        for filename in ("ost-source-ci.yml", "docs-check.yml", "parser-sanitizers.yml", "physics-installed.yml"):
            with self.subTest(filename=filename):
                triggers = self.load(filename)["on"]
                self.assertIn("workflow_call", triggers)
                self.assertNotIn("pull_request", triggers)
                self.assertNotIn("push", triggers)
        gate = self.load("ci-gate.yml")
        self.assertIn("pull_request", gate["on"])
        self.assertEqual(gate["jobs"]["required-ci"]["if"], "always()")
        self.assertEqual(set(gate["jobs"]["required-ci"]["needs"]),
                         {"changes", "docs", "source", "parser", "physics"})
        for job in ("source", "parser", "physics"):
            self.assertEqual(gate["jobs"][job]["if"], f"needs.changes.outputs.run_{job} == 'true'")

    def test_every_manifest_cell_is_still_rendered_once(self):
        manifest = yaml.load((ROOT / "openstrata.ci.yaml").read_text(encoding="utf-8"), Loader=yaml.BaseLoader)
        source = self.load("ost-source-ci.yml")
        cells = [cell for job in source["jobs"].values()
                 for cell in job.get("strategy", {}).get("matrix", {}).get("include", [])]
        self.assertEqual(sorted(cell["name"] for cell in cells),
                         sorted(cell["name"] for cell in manifest["cells"]))
        self.assertEqual(len(cells), 13)
        self.assertEqual(set(source["jobs"]["source-result"]["needs"]),
                         set(source["jobs"]) - {"source-result"})

    def test_source_aggregate_checks_every_group(self):
        import json

        source = self.load("ost-source-ci.yml")["jobs"]["source-result"]
        script = source["steps"][0]["run"].split("\n", 1)[1].rsplit("PY", 1)[0]
        results = {job: dict(result="success") for job in source["needs"]}

        def run(values):
            return subprocess.run([sys.executable, "-c", script], capture_output=True,
                                  env={**os.environ, "SOURCE_NEEDS": json.dumps(values)})

        self.assertEqual(run(results).returncode, 0)
        for job in source["needs"]:
            for status in ("failure", "cancelled", "skipped"):
                invalid = copy.deepcopy(results)
                invalid[job]["result"] = status
                with self.subTest(job=job, status=status):
                    self.assertNotEqual(run(invalid).returncode, 0)

    def test_adapter_preserves_source_jobs_and_rejects_new_triggers(self):
        raw = """# Generated by `ost ci generate github` from openstrata.ci.yaml.
on:
  pull_request:
jobs:
  pr:
    if: github.event_name == 'pull_request'
    runs-on: ubuntu-24.04
    steps:
      - run: ost build
"""
        output = yaml.load(adapt(raw), Loader=yaml.BaseLoader)
        expected = yaml.load(raw, Loader=yaml.BaseLoader)["jobs"]["pr"]
        del expected["if"]
        self.assertEqual(output["jobs"]["pr"], expected)
        with self.assertRaises(ValueError):
            adapt(raw.replace("  pull_request:", "  push:"))


if __name__ == "__main__":
    unittest.main()
