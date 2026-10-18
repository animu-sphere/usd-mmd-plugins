#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Negative controls for the acceptance checker: bad evidence must fail."""

import json
import pathlib
import unittest

from check_fixtures import check_output


class FixtureCheckerTest(unittest.TestCase):
    def setUp(self):
        root = pathlib.Path(__file__).resolve().parents[2]
        name = "sample-2.0-utf16.pmx"
        self.path = root / "plugins/usdMmdFileFormat/tests/fixtures" / name
        manifest = json.loads((self.path.parent / "fixtures.json").read_text(encoding="utf-8"))
        self.manifest = {self.path: manifest[name]}
        self.reader = (
            f"{self.path}\trigidBodies=3\tjoints=2\tpreservedOnlyJoints=0\taccepted"
            "\tbodies=3\tjoints=2\tfilters=3\tmaterials=3\tdamping=3"
            "\tunsupportedSpringJoints=1\tunsupportedSpringAxes=5\tjointFidelity=approximate"
        )
        self.execution = self.reader + "\tconstraints=2\tsteps=120\treplay=identical\treplaySamples=360"
        self.springs = self.execution + (
            "\ttranslationSpringModel=experimentalHookean\tapproximateTranslationSpringAxes=2"
            "\trotationSpringModel=experimentalSmallAngle\tapproximateRotationSpringAxes=3"
        )

    def check(self, output, execution=False, springs=False):
        check_output(output, [self.path], self.manifest, execution, springs, springs)

    def test_valid_evidence(self):
        self.check(self.reader)
        self.check(self.execution, execution=True)
        self.check(self.springs, execution=True, springs=True)

    def test_missing_duplicate_rejected_rows(self):
        for output in ("", self.reader + "\n" + self.reader,
                       self.reader.replace("accepted", "rejected")):
            with self.subTest(output=output), self.assertRaises(ValueError):
                self.check(output)

    def test_dropped_body_or_source_gap(self):
        for old, new in (("bodies=3", "bodies=2"),
                         ("unsupportedSpringAxes=5", "unsupportedSpringAxes=0"),
                         ("jointFidelity=approximate", "jointFidelity=unverified")):
            with self.subTest(field=old), self.assertRaises(ValueError):
                self.check(self.reader.replace(old, new))

    def test_missing_or_incomplete_replay(self):
        for output in (self.reader, self.execution.replace("\treplay=identical", ""),
                       self.execution.replace("replaySamples=360", "replaySamples=0"),
                       self.execution.replace("steps=120", "steps=119")):
            with self.subTest(output=output), self.assertRaises(ValueError):
                self.check(output, execution=True)

    def test_spring_modes_and_counts(self):
        for old, new in (("experimentalHookean", "unsupported"),
                         ("approximateTranslationSpringAxes=2", "approximateTranslationSpringAxes=0"),
                         ("approximateRotationSpringAxes=3", "approximateRotationSpringAxes=0")):
            with self.subTest(field=old), self.assertRaises(ValueError):
                self.check(self.springs.replace(old, new), execution=True, springs=True)
        with self.assertRaises(ValueError):
            self.check(self.springs, execution=True)
        with self.assertRaises(ValueError):
            self.check(self.execution)


if __name__ == "__main__":
    unittest.main()
