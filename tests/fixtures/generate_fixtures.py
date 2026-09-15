#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Write the PMX fixtures this repository tests against.

Every fixture is synthetic: its bytes are written here, so each one has a
known author and license (docs/design/DESIGN_POLICY.md §13). No distributed
MMD model is ever a fixture, and no PMX byte in the repository has another
author than this script.

  generate_fixtures.py              write the fixtures and fixtures.json
  generate_fixtures.py --check      fail unless the committed files are
                                    exactly what this script writes
  generate_fixtures.py --out DIR    write them somewhere else (a test's
                                    scratch directory, a fuzzing corpus)

The committed output lives in the file-format bundle,
plugins/usdMmdFileFormat/tests/fixtures/, because that is the only place its
manifest may name them from: `ost` refuses a `tests.smoke` path that leaves
the bundle (docs/architecture/WORKSPACE.md §6).

`fixtures.json` records what each fixture is for and what reading it must do.
A fixture that opens lists `diagnostics` (the codes the stage records, in
order), `parserDiagnostics` (the codes mmdPmx reports, which the stage's list
starts with), `counts` (the size of every table) and `modelName`; one that
fails names its `fatal` code. Tests read expectations from there rather than
restating them.

The layout follows docs/design/PMX_CONTRACT.md. This encoder is written
independently of the C++ one in libs/mmdPmx/tests/PmxEncoder.h, so the two
check each other: the parser reads these bytes, and the tools and the importer
must agree with what this file says they hold.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import struct
import sys

REPO = pathlib.Path(__file__).resolve().parents[2]
COMMITTED = REPO / "plugins" / "usdMmdFileFormat" / "tests" / "fixtures"
MANIFEST = "fixtures.json"

UTF16LE = 0
UTF8 = 1

INDEX_KINDS = ("vertex", "texture", "material", "bone", "morph", "rigidBody")
DEFORM_TYPES = {"BDEF1": 0, "BDEF2": 1, "BDEF4": 2, "SDEF": 3, "QDEF": 4}
MORPH_TYPES = {"group": 0, "vertex": 1, "bone": 2, "uv": 3, "additionalUv1": 4,
               "additionalUv2": 5, "additionalUv3": 6, "additionalUv4": 7,
               "material": 8, "flip": 9, "impulse": 10}
TABLES = ("vertices", "faces", "textures", "materials", "bones", "morphs",
          "displayFrames", "rigidBodies", "joints", "softBodies")

# Bone flags (PMX_CONTRACT.md §9).
TAIL_IS_BONE = 0x0001
ROTATABLE = 0x0002
TRANSLATABLE = 0x0004
VISIBLE = 0x0008
OPERABLE = 0x0010
IK = 0x0020
APPEND_ROTATION = 0x0100
APPEND_TRANSLATION = 0x0200
FIXED_AXIS = 0x0400
LOCAL_AXES = 0x0800
EXTERNAL_PARENT = 0x2000


# --- the encoder ---------------------------------------------------------------

class Writer:
    """PMX primitives, little-endian, at the model's encoding and widths."""

    def __init__(self, model: dict) -> None:
        self.model = model
        self.out = bytearray()

    def u8(self, value: int) -> None:
        self.out += struct.pack("<B", value & 0xFF)

    def u16(self, value: int) -> None:
        self.out += struct.pack("<H", value & 0xFFFF)

    def i32(self, value: int) -> None:
        self.out += struct.pack("<i", value)

    def f32(self, *values: float) -> None:
        for value in values:
            self.out += struct.pack("<f", value)

    def text(self, value: str | bytes) -> None:
        """int32 byte length, then the bytes. A `bytes` value is written as
        it is, whatever the encoding: how a fixture carries invalid text."""
        if isinstance(value, str):
            value = value.encode(
                "utf-8" if self.model["encoding"] == UTF8 else "utf-16-le")
        self.i32(len(value))
        self.out += value

    def index(self, kind: str, value: int) -> None:
        """An index at its kind's width, two's complement: -1 is 0xFF at
        width 1 (and 255 for a vertex index, which is unsigned there)."""
        width = self.model["widths"][kind]
        if width == 1:
            self.u8(value)
        elif width == 2:
            self.u16(value)
        else:
            self.i32(value)


def encode(model: dict) -> tuple[bytes, dict[str, int]]:
    """The model's bytes, and the offset at which each table's count starts."""
    w = Writer(model)
    offsets: dict[str, int] = {}
    widths = model["widths"]
    globals_ = ([model["encoding"], model["additionalVec4"]]
                + [widths[k] for k in INDEX_KINDS] + model.get("extraGlobals", []))
    w.out += b"PMX "
    w.f32(model["version"])
    w.u8(len(globals_))
    for value in globals_:
        w.u8(value)
    for field in model["info"]:
        w.text(field)

    offsets["vertices"] = len(w.out)
    w.i32(len(model["vertices"]))
    for v in model["vertices"]:
        w.f32(*v["position"], *v["normal"], *v["uv"])
        for k in range(model["additionalVec4"]):
            w.f32(*v["additionalVec4"][k])
        d = v["deform"]
        kind = d["type"]  # a name, or an invalid number written as it is
        w.u8(DEFORM_TYPES[kind] if isinstance(kind, str) else kind)
        for bone in d["bones"]:
            w.index("bone", bone)
        w.f32(*d["weights"])
        for vec in d.get("sdef", ()):
            w.f32(*vec)
        w.f32(v["edgeScale"])

    offsets["faces"] = len(w.out)
    w.i32(len(model["faces"]))
    for index in model["faces"]:
        w.index("vertex", index)

    offsets["textures"] = len(w.out)
    w.i32(len(model["textures"]))
    for path in model["textures"]:
        w.text(path)

    offsets["materials"] = len(w.out)
    w.i32(len(model["materials"]))
    for m in model["materials"]:
        w.text(m["name"])
        w.text(m["englishName"])
        w.f32(*m["diffuse"], *m["specular"], m["specularPower"], *m["ambient"])
        w.u8(m["flags"])
        w.f32(*m["edgeColor"], m["edgeSize"])
        w.index("texture", m["texture"])
        w.index("texture", m["sphereTexture"])
        w.u8(m["sphereMode"])
        kind, value = m["toon"]
        if kind == "texture":
            w.u8(0)
            w.index("texture", value)
        elif kind == "shared":
            w.u8(1)
            w.u8(value)
        else:  # an invalid reference byte, and nothing after it
            w.u8(value)
            continue
        w.text(m["memo"])
        w.i32(m["faceCount"])

    offsets["bones"] = len(w.out)
    w.i32(len(model["bones"]))
    for b in model["bones"]:
        flags = b["flags"]
        w.text(b["name"])
        w.text(b["englishName"])
        w.f32(*b["position"])
        w.index("bone", b["parent"])
        w.i32(b["transformLayer"])
        w.u16(flags)
        if flags & TAIL_IS_BONE:
            w.index("bone", b["tail"])
        else:
            w.f32(*b["tail"])
        if flags & (APPEND_ROTATION | APPEND_TRANSLATION):
            w.index("bone", b["append"][0])
            w.f32(b["append"][1])
        if flags & FIXED_AXIS:
            w.f32(*b["fixedAxis"])
        if flags & LOCAL_AXES:
            w.f32(*b["localAxes"][0], *b["localAxes"][1])
        if flags & EXTERNAL_PARENT:
            w.i32(b["externalParentKey"])
        if flags & IK:
            ik = b["ik"]
            w.index("bone", ik["target"])
            w.i32(ik["loopCount"])
            w.f32(ik["limitAngle"])
            w.i32(len(ik["links"]))
            for bone, limits in ik["links"]:
                w.index("bone", bone)
                if limits is None:
                    w.u8(0)
                else:
                    w.u8(1)
                    w.f32(*limits[0], *limits[1])

    offsets["morphs"] = len(w.out)
    w.i32(len(model["morphs"]))
    for m in model["morphs"]:
        w.text(m["name"])
        w.text(m["englishName"])
        w.u8(m["panel"])
        kind = m["type"]
        w.u8(MORPH_TYPES[kind] if isinstance(kind, str) else kind)
        if not isinstance(kind, str):
            continue  # an invalid type, and nothing after it
        w.i32(len(m["offsets"]))
        for o in m["offsets"]:
            if kind in ("group", "flip"):
                w.index("morph", o[0])
                w.f32(o[1])
            elif kind == "vertex":
                w.index("vertex", o[0])
                w.f32(*o[1])
            elif kind == "bone":
                w.index("bone", o[0])
                w.f32(*o[1], *o[2])
            elif kind in ("uv", "additionalUv1", "additionalUv2",
                          "additionalUv3", "additionalUv4"):
                w.index("vertex", o[0])
                w.f32(*o[1])
            elif kind == "material":
                w.index("material", o["material"])
                w.u8(o["operation"])
                w.f32(*o["diffuse"], *o["specular"], o["specularPower"],
                      *o["ambient"], *o["edgeColor"], o["edgeSize"],
                      *o["textureTint"], *o["sphereTint"], *o["toonTint"])
            elif kind == "impulse":
                w.index("rigidBody", o[0])
                w.u8(o[1])
                w.f32(*o[2], *o[3])

    offsets["displayFrames"] = len(w.out)
    w.i32(len(model["displayFrames"]))
    for f in model["displayFrames"]:
        w.text(f["name"])
        w.text(f["englishName"])
        w.u8(f["special"])
        w.i32(len(f["elements"]))
        for kind, index in f["elements"]:
            w.u8(kind)
            if kind in (0, 1):
                w.index("bone" if kind == 0 else "morph", index)

    offsets["rigidBodies"] = len(w.out)
    w.i32(len(model["rigidBodies"]))
    for r in model["rigidBodies"]:
        w.text(r["name"])
        w.text(r["englishName"])
        w.index("bone", r["bone"])
        w.u8(r["group"])
        w.u16(r["nonCollisionMask"])
        w.u8(r["shape"])
        w.f32(*r["size"], *r["position"], *r["rotation"], r["mass"],
              r["linearDamping"], r["angularDamping"], r["restitution"],
              r["friction"])
        w.u8(r["physicsMode"])

    offsets["joints"] = len(w.out)
    w.i32(len(model["joints"]))
    for j in model["joints"]:
        w.text(j["name"])
        w.text(j["englishName"])
        w.u8(j["type"])
        w.index("rigidBody", j["rigidBodies"][0])
        w.index("rigidBody", j["rigidBodies"][1])
        for vec in j["vectors"]:  # position, rotation, 4 limits, 2 springs
            w.f32(*vec)

    if model["version"] == 2.1:
        offsets["softBodies"] = len(w.out)
        w.i32(len(model["softBodies"]))
        for s in model["softBodies"]:
            w.text(s["name"])
            w.text(s["englishName"])
            w.u8(s["shape"])
            w.index("material", s["material"])
            w.u8(s["group"])
            w.u16(s["nonCollisionMask"])
            w.u8(s["flags"])
            w.i32(s["bLinkDistance"])
            w.i32(s["clusterCount"])
            w.f32(s["totalMass"], s["collisionMargin"])
            w.i32(s["aeroModel"])
            w.f32(*s["config"], *s["cluster"])
            for value in s["iteration"]:
                w.i32(value)
            w.f32(*s["materialCoefficients"])
            w.i32(len(s["anchors"]))
            for rigid_body, vertex, near in s["anchors"]:
                w.index("rigidBody", rigid_body)
                w.index("vertex", vertex)
                w.u8(near)
            w.i32(len(s["pins"]))
            for vertex in s["pins"]:
                w.index("vertex", vertex)

    w.out += model.get("trailing", b"")
    return bytes(w.out), offsets


# --- models ----------------------------------------------------------------------

def empty_model(version: float, encoding: int, name: str = "minimal",
                extra_globals: list[int] | None = None) -> dict:
    """Every table empty; no additional vec4; index width 1 throughout."""
    return {
        "version": version, "encoding": encoding, "additionalVec4": 0,
        "widths": {kind: 1 for kind in INDEX_KINDS},
        "extraGlobals": extra_globals or [],
        "info": [name, name, "", ""],
        **{table: [] for table in TABLES},
    }


def deform(kind: str, bones: list[int], weights: list[float] = (),
           sdef: tuple = ()) -> dict:
    """A deform as stored: BDEF1 one bone and no weight, BDEF2 and SDEF two
    bones and one weight, BDEF4 and QDEF four of each."""
    return {"type": kind, "bones": list(bones), "weights": list(weights),
            "sdef": sdef}


def vertex(x: float, d: dict, extra: int) -> dict:
    return {"position": (x, 10.0 + x, -0.5), "normal": (0.0, 0.0, -1.0),
            "uv": (x / 8.0, 1.0 - x / 8.0),
            "additionalVec4": [(x, float(k), 0.5, 1.0) for k in range(extra)],
            "deform": d, "edgeScale": 1.0}


def material(name: str, english: str, face_count: int, toon: tuple,
             texture: int = -1, sphere: int = -1, flags: int = 0) -> dict:
    return {"name": name, "englishName": english,
            "diffuse": (0.8, 0.8, 0.8, 1.0), "specular": (0.1, 0.1, 0.1),
            "specularPower": 8.0, "ambient": (0.4, 0.4, 0.4), "flags": flags,
            "edgeColor": (0.0, 0.0, 0.0, 1.0), "edgeSize": 1.0,
            "texture": texture, "sphereTexture": sphere,
            "sphereMode": 2 if sphere >= 0 else 0, "toon": toon,
            "memo": "", "faceCount": face_count}


def bone(name: str, english: str, position: tuple, parent: int, flags: int,
         tail, **extra) -> dict:
    return {"name": name, "englishName": english, "position": position,
            "parent": parent, "transformLayer": extra.pop("layer", 0),
            "flags": flags, "tail": tail, **extra}


def rigid_body(name: str, bone_index: int, shape: int, mode: int) -> dict:
    return {"name": name, "englishName": "", "bone": bone_index, "group": 0,
            "nonCollisionMask": 0xFFFF, "shape": shape,
            "size": (0.5, 1.0, 0.0), "position": (0.0, 15.0, 0.0),
            "rotation": (0.0, 0.0, 0.1), "mass": 1.0, "linearDamping": 0.5,
            "angularDamping": 0.5, "restitution": 0.0, "friction": 0.5,
            "physicsMode": mode}


def sample_model(version: float, encoding: int, width: int, extra: int) -> dict:
    """A small model that uses every table and every record variant the
    version allows, with Japanese names as the ordinary case."""
    v21 = version == 2.1
    model = empty_model(version, encoding)
    model["additionalVec4"] = extra
    model["widths"] = {kind: width for kind in INDEX_KINDS}
    model["info"] = [f"サンプル{version}", f"Sample {version}",
                     "合成フィクスチャ\r\n配布モデルではない",
                     "A synthetic fixture\r\nnot a distributed model"]

    model["vertices"] = [
        vertex(0.0, deform("BDEF1", [0]), extra),
        vertex(1.0, deform("BDEF2", [1, 2], [0.25]), extra),
        vertex(2.0, deform("BDEF4", [0, 1, 2, -1], [0.5, 0.3, 0.2, 0.0]), extra),
        vertex(3.0, deform("SDEF", [1, 2], [0.5],
                           ((0.0, 12.0, 0.0), (0.0, 12.5, 0.0), (0.0, 11.5, 0.0))),
               extra),
        vertex(4.0, deform("QDEF", [0, 1, -1, -1], [0.5, 0.5, 0.0, 0.0])
               if v21 else deform("BDEF1", [2]), extra),
        vertex(5.0, deform("BDEF2", [2, -1], [1.0]), extra),
    ]
    model["faces"] = [0, 1, 2, 2, 3, 4, 3, 4, 5]
    model["textures"] = ["tex\\髪.png", "tex/肌.png", "sph\\光沢.sph",
                         "toon\\ト ゥ ー ン.bmp"]
    model["materials"] = [
        material("髪", "hair", 3, ("texture", 3), texture=0, sphere=2,
                 flags=0x01 | 0x10),
        material("肌", "skin", 6, ("shared", 1), texture=1,
                 flags=0x02 | 0x04 | 0x08 | (0x20 if v21 else 0)),
    ]
    model["bones"] = [
        bone("センター", "center", (0.0, 8.0, 0.0), -1,
             ROTATABLE | TRANSLATABLE | VISIBLE | OPERABLE, (0.0, 1.0, 0.0)),
        bone("左腕", "LeftArm", (1.5, 13.0, 0.0), 0,
             TAIL_IS_BONE | ROTATABLE | VISIBLE | OPERABLE, 2),
        bone("左ひじ", "LeftElbow", (3.0, 12.0, 0.0), 1,
             ROTATABLE | VISIBLE | APPEND_ROTATION | FIXED_AXIS | LOCAL_AXES
             | EXTERNAL_PARENT, (1.0, 0.0, 0.0), layer=1,
             append=(1, 0.5), fixedAxis=(1.0, 0.0, 0.0),
             localAxes=((1.0, 0.0, 0.0), (0.0, 0.0, 1.0)), externalParentKey=3),
        bone("左手首ＩＫ", "LeftWrist IK", (4.5, 11.0, 0.0), 0,
             TAIL_IS_BONE | ROTATABLE | TRANSLATABLE | VISIBLE | OPERABLE | IK,
             -1, ik={"target": 2, "loopCount": 40, "limitAngle": 2.0,
                     "links": [(2, ((-3.1, 0.0, 0.0), (-0.01, 0.0, 0.0))),
                               (1, None)]}),
    ]
    model["morphs"] = [
        {"name": "まばたき", "englishName": "blink", "panel": 2, "type": "vertex",
         "offsets": [(0, (0.0, -0.1, 0.0)), (5, (0.0, -0.05, 0.02))]},
        {"name": "笑い", "englishName": "smile", "panel": 3, "type": "group",
         "offsets": [(0, 1.0), (2, 0.5)]},
        {"name": "肩", "englishName": "", "panel": 4, "type": "bone",
         "offsets": [(1, (0.0, 0.2, 0.0), (0.0, 0.0, 0.1305262, 0.9914449))]},
        {"name": "UV", "englishName": "uv", "panel": 4, "type": "uv",
         "offsets": [(3, (0.1, 0.0, 0.0, 0.0))]},
        {"name": "追加UV", "englishName": "", "panel": 0, "type": "additionalUv1",
         "offsets": [(4, (0.0, 0.5, 0.0, 0.0))]},
        {"name": "材質", "englishName": "material", "panel": 4, "type": "material",
         "offsets": [{"material": -1, "operation": 0,
                      "diffuse": (1.0, 1.0, 1.0, 0.5), "specular": (1.0, 1.0, 1.0),
                      "specularPower": 1.0, "ambient": (1.0, 1.0, 1.0),
                      "edgeColor": (1.0, 1.0, 1.0, 1.0), "edgeSize": 1.0,
                      "textureTint": (1.0, 1.0, 1.0, 1.0),
                      "sphereTint": (1.0, 1.0, 1.0, 1.0),
                      "toonTint": (1.0, 1.0, 1.0, 1.0)}]},
    ]
    if v21:
        model["morphs"] += [
            {"name": "反転", "englishName": "flip", "panel": 4, "type": "flip",
             "offsets": [(1, 1.0)]},
            {"name": "衝撃", "englishName": "impulse", "panel": 4, "type": "impulse",
             "offsets": [(1, 0, (0.0, 1.0, 0.0), (0.0, 0.0, 0.0))]},
        ]
    model["displayFrames"] = [
        {"name": "Root", "englishName": "Root", "special": 1, "elements": [(0, 0)]},
        {"name": "表情", "englishName": "Exp", "special": 1,
         "elements": [(1, 0), (1, 1)]},
        {"name": "左腕", "englishName": "LeftArm", "special": 0,
         "elements": [(0, 1), (0, 2), (0, 3)]},
    ]
    model["rigidBodies"] = [
        rigid_body("頭", 0, 0, 0),
        rigid_body("髪", -1, 2, 2),
    ]
    model["joints"] = [
        {"name": "頭-髪", "englishName": "", "type": 0, "rigidBodies": (0, 1),
         "vectors": [(0.0, 15.0, 0.0), (0.0, 0.0, 0.0), (0.0, 0.0, 0.0),
                     (0.0, 0.0, 0.0), (-0.2, -0.2, -0.2), (0.2, 0.2, 0.2),
                     (0.0, 0.0, 0.0), (5.0, 5.0, 5.0)]},
    ]
    if v21:
        model["softBodies"] = [{
            "name": "布", "englishName": "cloth", "shape": 0, "material": 0,
            "group": 1, "nonCollisionMask": 0xFFFE, "flags": 0x03,
            "bLinkDistance": 2, "clusterCount": 0, "totalMass": 1.0,
            "collisionMargin": 0.05, "aeroModel": 1,
            "config": tuple(0.1 * k for k in range(12)),
            "cluster": (0.1, 1.0, 0.5, 0.5, 0.5, 0.5),
            "iteration": (0, 1, 0, 4), "materialCoefficients": (1.0, 1.0, 1.0),
            "anchors": [(0, 0, 1)], "pins": [1, 2]}]
    return model


def counts(model: dict) -> dict[str, int]:
    """Every table's size; a PMX 2.0 file has no soft-body table, so zero."""
    return {table: len(model[table]) for table in TABLES}


# --- the fixtures ------------------------------------------------------------------

def opens(model: dict, purpose: str, parser: list[str] | None = None,
          importer: list[str] | None = None) -> tuple[bytes, dict]:
    data, _ = encode(model)
    parser = parser or []
    name = model["info"][0]
    return data, {
        "purpose": purpose, "opens": True,
        "sourceVersion": f"{model['version']:.1f}",
        "modelName": name if isinstance(name, str) else "",
        "counts": counts(model),
        "parserDiagnostics": parser,
        "diagnostics": parser + (importer or []),
    }


def fails(data: bytes, purpose: str, fatal: str) -> tuple[bytes, dict]:
    return data, {"purpose": purpose, "opens": False, "fatal": fatal}


def patched(model: dict, table: str, delta: int, value: bytes) -> bytes:
    """The model's bytes with `value` written at a table's count offset plus
    `delta`."""
    data, offsets = encode(model)
    at = offsets[table] + delta
    return data[:at] + value + data[at + len(value):]


def fixtures() -> dict[str, tuple[bytes, dict]]:
    """Every fixture: relative path -> (bytes, expectation)."""
    soft_body = ["MMD_PHYSICS_SOFT_BODY_UNSUPPORTED"]
    minimal, _ = encode(empty_model(2.0, UTF16LE))
    entries = {
        # Phase 0: the header, every table empty.
        "minimal.pmx": opens(empty_model(2.0, UTF16LE),
                             "PMX 2.0, UTF-16LE, every table empty"),
        "minimal-2.1-utf8.pmx": opens(empty_model(2.1, UTF8),
                                      "PMX 2.1, UTF-8, every table empty"),
        "unknown-globals.pmx": opens(
            empty_model(2.0, UTF16LE, extra_globals=[0]),
            "nine globals: the ninth is preserved and warned about",
            parser=["MMD_PMX_UNKNOWN_GLOBALS"]),
        "malformed/bad-signature.pmx": fails(
            b"PMD " + minimal[4:], "a signature that is not \"PMX \"",
            "MMD_PMX_BAD_SIGNATURE"),
        "malformed/unsupported-version.pmx": fails(
            minimal[:4] + struct.pack("<f", 3.0) + minimal[8:], "version 3.0",
            "MMD_PMX_UNSUPPORTED_VERSION"),
        "malformed/truncated-header.pmx": fails(
            minimal[:12], "the file ends inside the globals",
            "MMD_PMX_TRUNCATED_BUFFER"),

        # Phase 1: every table, every record variant, every index width.
        "sample-2.0-utf16.pmx": opens(
            sample_model(2.0, UTF16LE, 1, 0),
            "PMX 2.0, UTF-16LE, index width 1: every table and every 2.0 "
            "record variant, Japanese names"),
        "sample-2.1-utf8.pmx": opens(
            sample_model(2.1, UTF8, 2, 2),
            "PMX 2.1, UTF-8, index width 2, two additional vec4: QDEF, flip "
            "and impulse morphs, a soft body",
            importer=soft_body),
        "sample-2.1-utf16-wide.pmx": opens(
            sample_model(2.1, UTF16LE, 4, 4),
            "PMX 2.1, UTF-16LE, index width 4, four additional vec4",
            importer=soft_body),
    }

    # Recoverable: the model opens, and the stage records why it is not exact.
    bad_utf8 = sample_model(2.0, UTF8, 1, 0)
    bad_utf8["materials"][1]["name"] = b"\xE8\x82"  # a truncated sequence
    bad_utf16 = sample_model(2.0, UTF16LE, 1, 0)
    bad_utf16["bones"][1]["englishName"] = b"L\x00e\x00f\x00t"  # odd length
    bad_index = sample_model(2.0, UTF16LE, 1, 0)
    bad_index["bones"][0]["parent"] = 9
    bad_index["materials"][0]["texture"] = 7
    bad_index["vertices"][5]["deform"]["weights"] = [0.5]  # none, weighted
    short = sample_model(2.0, UTF16LE, 1, 0)
    short["materials"][1]["faceCount"] = 3
    trailing = sample_model(2.1, UTF8, 1, 0)
    trailing["trailing"] = b"\x00" * 16
    entries.update({
        "recoverable/invalid-utf8-name.pmx": opens(
            bad_utf8, "a material name that is not valid UTF-8: read as empty",
            parser=["MMD_TEXT_INVALID_UTF8"]),
        "recoverable/invalid-utf16-name.pmx": opens(
            bad_utf16, "a bone's English name of odd byte length: read as empty",
            parser=["MMD_TEXT_INVALID_UTF16"]),
        "recoverable/index-out-of-range.pmx": opens(
            bad_index, "a weighted deform of no bone, a missing texture, a "
            "missing parent: each read as none",
            parser=["MMD_PMX_INDEX_OUT_OF_RANGE"] * 3),
        "recoverable/material-faces-short.pmx": opens(
            short, "the materials draw 6 of the 9 face indices",
            parser=["MMD_PMX_MATERIAL_FACES_SHORT"]),
        "recoverable/trailing-bytes.pmx": opens(
            trailing, "16 bytes after the soft-body table",
            parser=["MMD_PMX_TRAILING_BYTES"], importer=soft_body),
    })

    # Fatal: the rest of the file cannot be located, or its structure is wrong.
    def mutated(edit) -> bytes:
        model = sample_model(2.0, UTF16LE, 1, 0)
        edit(model)
        return encode(model)[0]

    one_vertex = empty_model(2.0, UTF16LE)
    one_vertex["bones"] = sample_model(2.0, UTF16LE, 1, 0)["bones"][:1]
    one_vertex["vertices"] = [vertex(0.0, deform("BDEF4", [0, 0, 0, 0],
                                                 [1.0, 0.0, 0.0, 0.0]), 0)]
    vertex_at = encode(one_vertex)[1]["vertices"]
    flip = sample_model(2.1, UTF16LE, 1, 0)
    flip["version"] = 2.0
    flip["softBodies"] = []
    flip["vertices"][4]["deform"] = deform("BDEF1", [0])
    entries.update({
        "malformed/truncated-inside-vertex.pmx": fails(
            encode(one_vertex)[0][:vertex_at + 4 + 50],
            "the file ends inside a BDEF4 vertex's weights",
            "MMD_PMX_TRUNCATED_BUFFER"),
        "malformed/count-exceeds-buffer.pmx": fails(
            patched(empty_model(2.0, UTF16LE), "vertices", 0,
                    struct.pack("<i", 0x7FFFFFFF)),
            "a vertex count no file of this size can hold",
            "MMD_PMX_COUNT_EXCEEDS_BUFFER"),
        "malformed/negative-count.pmx": fails(
            patched(empty_model(2.0, UTF16LE), "textures", 0,
                    struct.pack("<i", -1)),
            "a texture count of -1", "MMD_PMX_COUNT_EXCEEDS_BUFFER"),
        "malformed/text-length-exceeds-buffer.pmx": fails(
            patched(sample_model(2.0, UTF16LE, 1, 0), "materials", 4,
                    struct.pack("<i", 0x7FFFFFF0)),
            "a material name longer than the rest of the file",
            "MMD_PMX_COUNT_EXCEEDS_BUFFER"),
        "malformed/invalid-deform-type.pmx": fails(
            mutated(lambda m: m["vertices"][1]["deform"].update(type=5)),
            "deform type 5", "MMD_PMX_INVALID_DEFORM_TYPE"),
        "malformed/qdef-in-2.0.pmx": fails(
            mutated(lambda m: m["vertices"][2]["deform"].update(type="QDEF")),
            "a QDEF vertex in a PMX 2.0 file", "MMD_PMX_INVALID_DEFORM_TYPE"),
        "malformed/face-count-not-triangles.pmx": fails(
            mutated(lambda m: m.update(faces=m["faces"][:8])),
            "8 face indices", "MMD_PMX_FACE_COUNT_NOT_TRIANGLES"),
        "malformed/face-index-out-of-range.pmx": fails(
            mutated(lambda m: m["faces"].__setitem__(4, 6)),
            "a face that draws vertex 6 of 6", "MMD_PMX_FACE_INDEX_OUT_OF_RANGE"),
        "malformed/material-faces-exceed-table.pmx": fails(
            mutated(lambda m: m["materials"][1].update(faceCount=9)),
            "the materials draw 12 of 9 face indices",
            "MMD_PMX_MATERIAL_FACES_EXCEED_TABLE"),
        "malformed/invalid-toon-reference.pmx": fails(
            mutated(lambda m: m["materials"][0].update(toon=("invalid", 2))),
            "a toon reference of 2", "MMD_PMX_INVALID_LAYOUT_FLAG"),
        "malformed/invalid-morph-type.pmx": fails(
            mutated(lambda m: m["morphs"][2].update(type=11)),
            "morph type 11", "MMD_PMX_INVALID_MORPH_TYPE"),
        "malformed/flip-morph-in-2.0.pmx": fails(
            encode(flip)[0], "a flip morph in a PMX 2.0 file",
            "MMD_PMX_INVALID_MORPH_TYPE"),
        "malformed/invalid-frame-element.pmx": fails(
            mutated(lambda m: m["displayFrames"][2]["elements"].__setitem__(1, (2, 0))),
            "a display-frame element of kind 2", "MMD_PMX_INVALID_LAYOUT_FLAG"),
    })
    return entries


# --- writing and checking ------------------------------------------------------------

def manifest_text(entries: dict[str, tuple[bytes, dict]]) -> str:
    body = {path: expectation for path, (_, expectation) in entries.items()}
    return json.dumps(body, indent=2, sort_keys=True, ensure_ascii=False) + "\n"


def write(out: pathlib.Path) -> int:
    entries = fixtures()
    for relative, (data, _) in entries.items():
        path = out / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    (out / MANIFEST).write_bytes(manifest_text(entries).encode("utf-8"))
    print(f"wrote {len(entries)} fixtures and {MANIFEST} to {out}")
    return 0


def check(out: pathlib.Path) -> int:
    entries = fixtures()
    problems: list[str] = []
    for relative, (data, _) in entries.items():
        path = out / relative
        if not path.is_file():
            problems.append(f"missing: {relative}")
        elif path.read_bytes() != data:
            problems.append(f"differs from the generator: {relative}")
    committed = {p.relative_to(out).as_posix() for p in out.rglob("*.pmx")}
    for extra in sorted(committed - set(entries)):
        problems.append(f"not written by the generator: {extra}")
    manifest = out / MANIFEST
    if not manifest.is_file() or manifest.read_bytes() != manifest_text(
            entries).encode("utf-8"):
        problems.append(f"{MANIFEST} differs from the generator")

    if problems:
        print("\n".join(problems), file=sys.stderr)
        print("run tests/fixtures/generate_fixtures.py and commit the result",
              file=sys.stderr)
        return 1
    print(f"{len(entries)} fixtures in {out} match the generator")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true",
                        help="compare instead of writing")
    parser.add_argument("--out", type=pathlib.Path, default=COMMITTED,
                        help="output directory (default: the committed one)")
    args = parser.parse_args()
    return check(args.out) if args.check else write(args.out)


if __name__ == "__main__":
    raise SystemExit(main())
