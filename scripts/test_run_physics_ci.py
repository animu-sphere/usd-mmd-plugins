#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Exercise host Python overrides against a relocated SDK's stale defaults."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from run_physics_ci import python_cmake_args


@unittest.skipUnless(os.name == "nt", "Windows installed-physics CI regression")
class HostPythonTest(unittest.TestCase):
    def test_foreign_sdk_defaults_are_overridden(self):
        with tempfile.TemporaryDirectory() as scratch:
            root = Path(scratch)
            # Match pxrConfig's fallback behavior without changing an installed SDK.
            (root / "CMakeLists.txt").write_text('''
cmake_minimum_required(VERSION 3.25)
project(ForeignPythonSdk LANGUAGES NONE)
foreach(artifact EXECUTABLE INCLUDE_DIR LIBRARY)
  if(NOT DEFINED Python3_${artifact})
    set(Python3_${artifact} "${CMAKE_CURRENT_SOURCE_DIR}/foreign-host/missing-${artifact}")
  endif()
endforeach()
find_package(Python3 3.13 EXACT REQUIRED COMPONENTS Interpreter Development)
if(NOT TARGET Python3::Python OR NOT TARGET Python3::Module)
  message(FATAL_ERROR "Python development targets missing")
endif()
''', encoding="utf-8")

            def configure(name, options):
                return subprocess.run(["cmake", "-S", str(root), "-B", str(root / name),
                                       *options], capture_output=True, text=True,
                                      encoding="utf-8", errors="replace", timeout=60)

            old = configure("executable-only", [f"-DPython3_EXECUTABLE={Path(sys.executable).as_posix()}"])
            self.assertNotEqual(old.returncode, 0, old.stdout + old.stderr)
            self.assertIn("foreign-host", old.stdout + old.stderr)
            fixed = configure("host-sdk", python_cmake_args())
            self.assertEqual(fixed.returncode, 0, fixed.stdout + fixed.stderr)


if __name__ == "__main__":
    unittest.main()
