#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check the installed physics probe against generated PMX expectations."""

from __future__ import annotations

import argparse
import json
import pathlib
import subprocess
import sys


# Bounded PR set: both PMX versions, both joint activation types, source-only
# joints and repairs, reordered bone associations, and an empty physics scene.
REPRESENTATIVE = (
    "sample-2.0-utf16.pmx",
    "sample-2.1-utf8.pmx",
    "recoverable/physics-repairs.pmx",
    "bones-reordered.pmx",
    "minimal.pmx",
)


def expected_fields(physics: dict | None, execution: bool,
                    translation: bool, rotation: bool) -> dict[str, str]:
    if physics is None:
        physics = {"rigidBodies": [], "joints": []}
    bodies = physics["rigidBodies"]
    joints = physics["joints"]
    standard = [joint for joint in joints if joint["usdPhysics"]]
    source_only = len(joints) - len(standard)
    spring_axes = [sum(value != 0 for kind in ("translationSpring", "rotationSpring")
                       for value in joint[kind]) for joint in joints]
    expected = {
        "rigidBodies": len(bodies), "bodies": len(bodies),
        "joints": len(standard), "preservedOnlyJoints": source_only,
        "filters": len(bodies), "materials": len(bodies), "damping": len(bodies),
        "unsupportedSpringJoints": sum(count != 0 for count in spring_axes),
        "unsupportedSpringAxes": sum(spring_axes),
        "jointFidelity": "approximate" if any(spring_axes) or source_only else "unverified",
    }
    if execution:
        expected.update(constraints=len(standard), steps=120, replay="identical",
                        replaySamples=120 * len(bodies))
    for enabled, kind, axis_prefix, model in (
        (translation, "Translation", "trans", "experimentalHookean"),
        (rotation, "Rotation", "rot", "experimentalSmallAngle"),
    ):
        if enabled:
            count = 0
            for joint in standard:
                if joint["type"] != "spring6Dof":
                    continue
                for axis, stiffness in zip("XYZ", joint[kind.lower() + "Spring"]):
                    limit = joint["limits"].get(axis_prefix + axis)
                    if stiffness > 0 and (limit is None or limit[0] != limit[1]):
                        count += 1
            expected[kind.lower() + "SpringModel"] = model
            expected["approximate" + kind + "SpringAxes"] = count
    return {key: str(value) for key, value in expected.items()}


def check_output(output: str, paths: list[pathlib.Path], manifest: dict,
                 execution: bool, translation: bool, rotation: bool) -> None:
    rows = [line.split("\t") for line in output.splitlines()
            if not line.startswith("control")]
    if len(rows) != len(paths):
        raise ValueError(f"expected {len(paths)} stage rows, got {len(rows)}")
    for row, path in zip(rows, paths):
        if row[0] != str(path) or row.count("accepted") != 1 or "rejected" in row:
            raise ValueError(f"missing accepted stage: {path}: {row}")
        fields = {}
        for item in row[1:]:
            if "=" not in item:
                continue
            key, value = item.split("=", 1)
            if key in fields and fields[key] != value:
                raise ValueError(f"inconsistent {key} in {path}")
            fields[key] = value
        expected = expected_fields(manifest[path]["stage"]["physics"], execution,
                                   translation, rotation)
        for key, value in expected.items():
            if fields.get(key) != value:
                raise ValueError(f"{path.name}: {key}={fields.get(key)!r}, expected {value!r}")
        absent = []
        if not execution:
            absent.extend(("constraints", "steps", "replay", "replaySamples"))
        if not translation:
            absent.extend(("translationSpringModel", "approximateTranslationSpringAxes"))
        if not rotation:
            absent.extend(("rotationSpringModel", "approximateRotationSpringAxes"))
        if any(key in fields for key in absent):
            raise ValueError(f"unexpected execution/spring feature in {path}: {fields}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=pathlib.Path, required=True)
    parser.add_argument("--fixtures", type=pathlib.Path, required=True)
    parser.add_argument("--execution", action="store_true")
    parser.add_argument("--translation-springs", action="store_true")
    parser.add_argument("--rotation-springs", action="store_true")
    parser.add_argument("--all", action="store_true", help="opt-in full opening corpus")
    args = parser.parse_args()
    if (args.translation_springs or args.rotation_springs) and not args.execution:
        parser.error("spring checks require --execution")
    fixtures = args.fixtures.resolve()
    manifest = json.loads((fixtures / "fixtures.json").read_text(encoding="utf-8"))
    names = sorted(k for k, v in manifest.items() if v["opens"]) if args.all else REPRESENTATIVE
    paths = [fixtures / name for name in names]
    expectations = {path: manifest[name] for path, name in zip(paths, names)}
    result = subprocess.run([str(args.probe.resolve()), *map(str, paths)],
                            capture_output=True, text=True, encoding="utf-8", errors="replace",
                            timeout=120)
    # Preserve per-prim fidelity diagnostics as evidence, including on failure.
    print(result.stdout, end="")
    print(result.stderr, end="", file=sys.stderr)
    if result.returncode:
        raise ValueError(f"probe exited with {result.returncode}")
    check_output(result.stdout, paths, expectations, args.execution,
                 args.translation_springs, args.rotation_springs)
    print(f"physics fixtures passed: {len(paths)} inputs")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, OSError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
