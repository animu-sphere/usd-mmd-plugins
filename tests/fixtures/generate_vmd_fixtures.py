#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Write the VMD fixtures this repository tests against.

Every fixture is synthetic: its bytes are written here, so each one has a
known author and license (docs/design/DESIGN_POLICY.md §13). No distributed
motion is ever a fixture.

  generate_vmd_fixtures.py --out DIR    write the fixtures and fixtures.json

Nothing is committed: the tools' tests and the fuzzing lane write the
fixtures into a scratch directory each time, so there is no copy to keep in
step and no bundle manifest that must name them (compare generate_fixtures.py).

`fixtures.json` records what reading each fixture must do. One that opens
lists `signature`, `modelName`, `sectionsPresent`, `counts` (the records in
every section), `tracks` (how many bone, morph and IK tracks the records
group into), and `diagnostics` -- the codes motionVmd reports, the reader's
first, then BuildMotion's. One that fails names its `fatal` code.

The layout follows docs/design/MOTION_CONTRACT.md §3. This encoder is written
independently of the C++ one in libs/motionVmd/tests/VmdEncoder.h, so the two
check each other. Names are encoded with Python's cp932 codec; the C++ tests
state their CP932 bytes literally, so the project's table is never checked
against itself.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import struct
import sys

SIGNATURE_V1 = b"Vocaloid Motion Data file"
SIGNATURE_V2 = b"Vocaloid Motion Data 0002"


def field(data: str | bytes, width: int) -> bytes:
    raw = data.encode("cp932") if isinstance(data, str) else data
    return raw[:width].ljust(width, b"\0")


def linear_bone_interpolation() -> bytes:
    # MMD's default: every curve (20, 20) - (107, 107).
    row = bytes([20] * 4 + [20] * 4 + [107] * 4 + [107] * 4)
    return row * 4


class Motion:
    """A VMD as data: records per section, in file order."""

    def __init__(self, model: str | bytes, version: int = 2) -> None:
        self.version = version
        self.model = model
        self.bones: list[tuple] = []
        self.morphs: list[tuple] = []
        self.cameras: list[tuple] = []
        self.lights: list[tuple] = []
        self.shadows: list[tuple] = []
        self.ik: list[tuple] = []
        self.sections = 6
        self.trailing = b""

    def encode(self) -> bytes:
        out = bytearray()
        if self.version == 1:
            out += field(SIGNATURE_V1, 30) + field(self.model, 10)
        else:
            out += field(SIGNATURE_V2, 30) + field(self.model, 20)
        sections = [
            (self.bones, lambda r: field(r[0], 15) + struct.pack(
                "<I3f4f", r[1], *r[2], *r[3]) + linear_bone_interpolation()),
            (self.morphs, lambda r: field(r[0], 15) + struct.pack("<If", r[1], r[2])),
            (self.cameras, lambda r: struct.pack("<If3f3f", r[0], r[1], *r[2], *r[3])
             + bytes(range(24)) + struct.pack("<IB", r[4], r[5])),
            (self.lights, lambda r: struct.pack("<I3f3f", r[0], *r[1], *r[2])),
            (self.shadows, lambda r: struct.pack("<IBf", *r)),
            (self.ik, lambda r: struct.pack("<IBI", r[0], r[1], len(r[2])) + b"".join(
                field(name, 20) + bytes([enabled]) for name, enabled in r[2])),
        ]
        for records, encode in sections[:self.sections]:
            out += struct.pack("<I", len(records))
            for record in records:
                out += encode(record)
        return bytes(out) + self.trailing


def sample() -> Motion:
    m = Motion("サンプル")
    for frame, x in ((0, 0.0), (30, 1.0), (60, 2.0)):
        m.bones.append(("センター", frame, (x, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0)))
    m.bones.append(("右足ＩＫ", 15, (0.0, 1.0, -1.0), (0.0, 0.0, 0.0, 1.0)))
    m.bones.append(("左ひじ", 0, (0.0, 0.0, 0.0), (0.0, 0.3826834, 0.0, 0.9238795)))
    m.morphs.append(("まばたき", 10, 1.0))
    m.morphs.append(("まばたき", 0, 0.0))
    m.morphs.append(("あ", 5, 0.5))
    m.cameras.append((0, -45.0, (0.0, 10.0, 0.0), (0.0, 0.0, 0.0), 30, 0))
    m.lights.append((0, (0.6, 0.6, 0.6), (-0.5, -1.0, 0.5)))
    m.shadows.append((0, 1, 0.0085))
    m.ik.append((0, 1, [("右足ＩＫ", 1), ("左足ＩＫ", 1)]))
    m.ik.append((120, 1, [("右足ＩＫ", 0), ("左足ＩＫ", 1)]))
    return m


def fixtures() -> dict[str, tuple[Motion | bytes, dict]]:
    out: dict[str, tuple[Motion | bytes, dict]] = {}

    def opens(signature: str, model: str, sections: int, counts: list[int],
              tracks: list[int], diagnostics: list[str]) -> dict:
        names = ["boneKeyframes", "morphKeyframes", "cameraKeyframes",
                 "lightKeyframes", "selfShadowKeyframes", "ikKeyframes"]
        return {
            "opens": True,
            "signature": signature,
            "modelName": model,
            "sectionsPresent": sections,
            "counts": dict(zip(names, counts)),
            "tracks": dict(zip(["bones", "morphs", "ik"], tracks)),
            "diagnostics": diagnostics,
        }

    v2 = SIGNATURE_V2.decode()
    v1 = SIGNATURE_V1.decode()

    out["minimal.vmd"] = (Motion("minimal"),
                          opens(v2, "minimal", 6, [0] * 6, [0, 0, 0], []))
    out["sample.vmd"] = (sample(),
                         opens(v2, "サンプル", 6, [5, 3, 1, 1, 1, 2], [3, 2, 2], []))

    old = sample()
    old.version = 1
    old.model = "古い"
    old.sections = 2
    out["version1-ends-after-morphs.vmd"] = (
        old, opens(v1, "古い", 2, [5, 3, 0, 0, 0, 0], [3, 2, 0], []))

    duplicate = sample()
    duplicate.bones.append(("センター", 30, (9.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0)))
    duplicate.ik.append((120, 0, []))
    out["recoverable/duplicate-keyframes.vmd"] = (
        duplicate, opens(v2, "サンプル", 6, [6, 3, 1, 1, 1, 3], [3, 2, 2],
                         ["MMD_MOTION_DUPLICATE_KEYFRAME", "MMD_MOTION_DUPLICATE_KEYFRAME"]))

    names = Motion("names")
    # 16 bytes in a 15-byte field: cut inside the last character.
    names.bones.append(("右腕捩りボーン先", 0, (0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0)))
    # 0x85 0x40: a pair CP932 does not map.
    names.morphs.append((b"\x85\x40", 0, 1.0))
    names.trailing = b"\0\0\0\0"
    out["recoverable/names-and-trailing-bytes.vmd"] = (
        names, opens(v2, "names", 6, [1, 1, 0, 0, 0, 0], [1, 1, 0],
                     ["MMD_TEXT_TRUNCATED_CP932", "MMD_TEXT_INVALID_CP932",
                      "MMD_MOTION_TRAILING_BYTES"]))

    def fails(code: str) -> dict:
        return {"opens": False, "fatal": code}

    whole = sample().encode()
    out["malformed/bad-signature.vmd"] = (
        b"Vocaloid Motion Data 0003" + whole[25:], fails("MMD_MOTION_BAD_SIGNATURE"))
    # Cut inside the first bone record.
    out["malformed/truncated.vmd"] = (whole[:120], fails("MMD_MOTION_COUNT_EXCEEDS_BUFFER"))
    out["malformed/truncated-count.vmd"] = (whole[:52], fails("MMD_MOTION_TRUNCATED_BUFFER"))
    huge = bytearray(whole)
    huge[50:54] = struct.pack("<I", 0x7FFFFFFF)
    out["malformed/count-exceeds-buffer.vmd"] = (
        bytes(huge), fails("MMD_MOTION_COUNT_EXCEEDS_BUFFER"))
    return out


def write(out_dir: pathlib.Path) -> int:
    manifest = {}
    for relative, (motion, expectation) in sorted(fixtures().items()):
        path = out_dir / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(motion if isinstance(motion, bytes) else motion.encode())
        manifest[relative] = expectation
    (out_dir / "fixtures.json").write_bytes(
        (json.dumps(manifest, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))
    print(f"wrote {len(manifest)} VMD fixtures to {out_dir}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", type=pathlib.Path, required=True,
                        help="output directory")
    args = parser.parse_args()
    return write(args.out)


if __name__ == "__main__":
    sys.exit(main())
