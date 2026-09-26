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
starts with), `counts` (the size of every table), `modelName`, and `stage`:
what the canonical stage must hold -- identifiers, joint paths and order,
material face ranges, texture asset paths and whether each resolves, and a
vertex and a joint through the coordinate conversion. One that fails names its
`fatal` code. Tests read expectations from there rather than restating them.

The layout follows docs/design/PMX_CONTRACT.md. This encoder is written
independently of the C++ one in libs/mmdPmx/tests/PmxEncoder.h, so the two
check each other: the parser reads these bytes, and the tools and the importer
must agree with what this file says they hold. The stage expectations are
likewise computed here, from the design documents' rules, and not by the C++
canonicalizer they check.

Beside the fixtures the generator writes the texture files the sample models
name -- one-pixel images, also synthetic -- so that a Japanese texture
filename is proven to resolve (docs/design/STAGE_CONTRACT.md §14).
"""

from __future__ import annotations

import argparse
import heapq
import json
import math
import pathlib
import re
import struct
import sys
import zlib

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
LOCAL_APPEND = 0x0080
APPEND_ROTATION = 0x0100
APPEND_TRANSLATION = 0x0200
FIXED_AXIS = 0x0400
LOCAL_AXES = 0x0800
DEFORM_AFTER_PHYSICS = 0x1000
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


def rigid_body(name: str, english: str, bone_index: int, group: int, shape: int,
               size: tuple, position: tuple, rotation: tuple, mass: float,
               mode: int) -> dict:
    return {"name": name, "englishName": english, "bone": bone_index, "group": group,
            "nonCollisionMask": 0xFFFF & ~(1 << group), "shape": shape,
            "size": size, "position": position, "rotation": rotation, "mass": mass,
            "linearDamping": 0.5, "angularDamping": 0.25, "restitution": 0.125,
            "friction": 0.75, "physicsMode": mode}


def joint(name: str, kind: int, bodies: tuple, position: tuple, rotation: tuple,
          translation: tuple, rotation_limits: tuple, springs: tuple) -> dict:
    return {"name": name, "englishName": "", "type": kind, "rigidBodies": bodies,
            "vectors": [position, rotation, *translation, *rotation_limits, *springs]}


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
             TAIL_IS_BONE | ROTATABLE | VISIBLE | OPERABLE | DEFORM_AFTER_PHYSICS, 2),
        bone("左ひじ", "LeftElbow", (3.0, 12.0, 0.0), 1,
             ROTATABLE | VISIBLE | APPEND_ROTATION | APPEND_TRANSLATION | LOCAL_APPEND
             | FIXED_AXIS | LOCAL_AXES | EXTERNAL_PARENT, (1.0, 0.25, -0.5), layer=1,
             append=(1, 0.5), fixedAxis=(0.8, 0.0, -0.6),
             localAxes=((0.8, 0.0, 0.6), (-0.6, 0.0, 0.8)), externalParentKey=3),
        bone("左手首ＩＫ", "LeftWrist IK", (4.5, 11.0, 0.0), 0,
             TAIL_IS_BONE | ROTATABLE | TRANSLATABLE | VISIBLE | OPERABLE | IK,
             -1, ik={"target": 2, "loopCount": 40, "limitAngle": 2.0,
                     "links": [(1, ((-3.1, -0.5, -0.25), (-0.01, 0.5, 0.75))),
                               (0, None)]}),
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
    # One body of each shape and mode, each turned about all three axes, so a
    # wrong Euler order or a missing mirror cannot pass (PMX-O1).
    model["rigidBodies"] = [
        rigid_body("頭", "head", 1, 1, 0, (1.5, 0.0, 0.0), (1.5, 14.0, -0.5),
                   (0.3, -0.2, 0.1), 1.0, 0),
        rigid_body("髪", "", -1, 2, 2, (0.5, 2.0, 0.0), (2.0, 12.5, 0.75),
                   (0.5, 0.25, -0.125), 0.5, 1),
        rigid_body("胸", "chest", 2, 2, 1, (1.0, 0.5, 0.25), (3.0, 12.0, -0.25),
                   (-0.4, 1.2, 0.7), 0.0, 2),
    ]
    model["joints"] = [
        # Translation along Y is free: its lower limit is above its upper.
        joint("頭-髪", 0, (0, 1), (1.75, 13.25, 0.25), (0.1, 0.2, 0.3),
              ((-0.5, 1.0, -1.0), (0.5, 0.0, 0.25)),
              ((-0.2, -0.3, -0.4), (0.1, 0.2, 0.3)),
              ((0.0, 10.0, 20.0), (5.0, 6.0, 7.0))),
        joint("髪-胸", 1 if v21 else 0, (1, 2), (2.5, 12.25, 0.25), (-0.6, 0.0, 0.9),
              ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0)),
              ((-1.0, -0.5, 0.0), (1.0, 0.5, 0.0)),
              ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0))),
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


# --- the texture files -------------------------------------------------------------

def png(rgba: tuple[int, int, int, int]) -> bytes:
    """A 1x1 RGBA PNG. The image data is a stored (uncompressed) deflate block
    written here, so the bytes do not depend on the zlib that runs this."""
    raw = b"\x00" + bytes(rgba)  # filter type 0, one pixel
    stored = (b"\x78\x01" + b"\x01" + struct.pack("<HH", len(raw), len(raw) ^ 0xFFFF)
              + raw + struct.pack(">I", zlib.adler32(raw)))

    def chunk(tag: bytes, data: bytes) -> bytes:
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data)))

    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", stored) + chunk(b"IEND", b""))


def bmp(rgb: tuple[int, int, int]) -> bytes:
    """A 1x1 24-bit BMP, the format MMD's toon and sphere textures use."""
    pixel = bytes((rgb[2], rgb[1], rgb[0], 0))  # BGR, padded to 4 bytes
    info = struct.pack("<IiiHHIIiiII", 40, 1, 1, 1, 24, 0, len(pixel), 2835, 2835, 0, 0)
    header = struct.pack("<2sIHHI", b"BM", 14 + len(info) + len(pixel), 0, 0,
                         14 + len(info))
    return header + info + pixel


def bmp_image(pixels: list[list[tuple]], alpha: bool) -> bytes:
    """A BMP of `pixels` (rows top to bottom, RGBA), stored bottom-up as BMP
    does: 24-bit with each row padded to 4 bytes, or 32-bit BGRA, whose alpha
    byte a BI_RGB reader takes as alpha unless every one is zero."""
    height, width = len(pixels), len(pixels[0])
    rows = b""
    for row in reversed(pixels):
        if alpha:
            data = b"".join(bytes((b, g, r, a)) for r, g, b, a in row)
        else:
            data = b"".join(bytes((b, g, r)) for r, g, b, _ in row)
        rows += data + b"\0" * (-len(data) % 4)
    info = struct.pack("<IiiHHIIiiII", 40, width, height, 1, 32 if alpha else 24, 0,
                       len(rows), 2835, 2835, 0, 0)
    header = struct.pack("<2sIHHI", b"BM", 14 + len(info) + len(rows), 0, 0,
                         14 + len(info))
    return header + info + rows


def tga_image(pixels: list[list[tuple]]) -> bytes:
    """An uncompressed 32-bit truecolor TGA of `pixels` (rows top to bottom,
    RGBA), stored from the bottom-left origin, TGA's default."""
    height, width = len(pixels), len(pixels[0])
    header = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, width, height, 32,
                         0x08)  # eight alpha bits, bottom-left origin
    return header + b"".join(bytes((b, g, r, a)) for row in reversed(pixels)
                             for r, g, b, a in row)


def jpeg_gray() -> bytes:
    """A 1x1 baseline JPEG, mid grey: one quantization table of ones, one
    Huffman code per table, and a scan of one DC difference of zero and an
    end-of-block. Packaging stores a JPEG byte for byte, so nothing decodes
    it; it has to be a JPEG, not a particular one."""
    def segment(marker: int, body: bytes) -> bytes:
        return struct.pack(">HH", marker, len(body) + 2) + body

    one_code = bytes([1] + [0] * 15)  # one code of length 1
    return (b"\xff\xd8"
            + segment(0xFFDB, b"\x00" + b"\x01" * 64)
            + segment(0xFFC0, struct.pack(">BHHB", 8, 1, 1, 1) + b"\x01\x11\x00")
            + segment(0xFFC4, b"\x00" + one_code + b"\x00"    # DC: category 0
                      + b"\x10" + one_code + b"\x00")         # AC: end of block
            + segment(0xFFDA, b"\x01\x01\x00\x00\x3f\x00")
            + b"\x3f"  # the codes 0, 0, padded with ones
            + b"\xff\xd9")


# A 1x1 GIF: a format USDZ does not hold and packaging does not convert.
GIF = (b"GIF89a\x01\x00\x01\x00\x80\x00\x00\xff\xff\xff\x00\x00\x00"
       b",\x00\x00\x00\x00\x01\x00\x01\x00\x00\x02\x02D\x01\x00;")

# The images packaging converts, as their pixels: 2x2, so a row order or a
# channel order that is wrong cannot pass (PACKAGING_POLICY.md §7, §14). Rows
# top to bottom, RGBA; `alpha` is whether the file stores an alpha channel.
PACKAGING_IMAGES = {
    "packaging/spa/光沢.spa": {"alpha": False, "pixels": [
        [(255, 0, 0, 255), (0, 255, 0, 255)],
        [(0, 0, 255, 255), (255, 255, 0, 255)]]},
    "packaging/toon/トゥーン.tga": {"alpha": True, "pixels": [
        [(10, 20, 30, 255), (40, 50, 60, 128)],
        [(70, 80, 90, 0), (200, 150, 100, 64)]]},
    "packaging/tex/目.bmp": {"alpha": True, "pixels": [
        [(1, 2, 3, 255), (4, 5, 6, 200)],
        [(7, 8, 9, 100), (250, 240, 230, 0)]]},
}

# Every texture the sample models name, beside the fixtures at the top of the
# fixture directory -- so the samples' textures resolve, and the fixtures in
# subdirectories name the same paths and do not. The packaging fixtures name
# theirs in packaging/, one file of each kind PACKAGING_POLICY.md §7 decides
# on; packaging/tex/無い.png is named and missing on purpose.
TEXTURES = {
    "tex/髪.png": png((64, 40, 32, 255)),
    "tex/肌.png": png((250, 220, 200, 255)),
    "sph/光沢.sph": bmp((255, 255, 255)),
    "toon/ト ゥ ー ン.bmp": bmp((200, 200, 200)),
    "packaging/tex/髪.png": png((64, 40, 32, 255)),
    "packaging/spa/光沢.spa": bmp_image(
        PACKAGING_IMAGES["packaging/spa/光沢.spa"]["pixels"], alpha=False),
    "packaging/toon/トゥーン.tga": tga_image(
        PACKAGING_IMAGES["packaging/toon/トゥーン.tga"]["pixels"]),
    "packaging/tex/肌.jpg": jpeg_gray(),
    "packaging/sph/反射.sph": png((128, 128, 255, 255)),
    "packaging/tex/目.bmp": bmp_image(
        PACKAGING_IMAGES["packaging/tex/目.bmp"]["pixels"], alpha=True),
    "packaging/tex/動く.gif": GIF,
    "packaging/toon/影.bmp": bmp((90, 90, 90)),
    "packaging/toon/影.bmp.png": png((90, 90, 90, 255)),
}


# --- what the stage must say ----------------------------------------------------------
#
# The rules of the design documents, restated in Python: stable identifiers
# (TEXT_ENCODING_POLICY.md §6), the canonical joint order (STAGE_CONTRACT.md
# §9.1), texture paths (TEXT_ENCODING_POLICY.md §7) and the coordinate
# conversion (STAGE_CONTRACT.md §6). They are what the C++ canonicalizer is
# checked against, so they are written from the documents, not from it.

METERS_PER_UNIT = 0.08


def f32(value: float) -> float:
    """`value` rounded to binary32, as a point3f holds it."""
    return struct.unpack("<f", struct.pack("<f", value))[0]


def point(v: tuple) -> list[float]:
    """A PMX position as the stage's point3f: meters, Z mirrored."""
    return [f32(f32(v[0]) * METERS_PER_UNIT), f32(f32(v[1]) * METERS_PER_UNIT),
            -f32(f32(v[2]) * METERS_PER_UNIT) + 0.0]


def point_d(v: tuple) -> list[float]:
    """A PMX position as a joint translation: the same, kept in double."""
    return [f32(v[0]) * METERS_PER_UNIT + 0.0, f32(v[1]) * METERS_PER_UNIT + 0.0,
            -(f32(v[2]) * METERS_PER_UNIT) + 0.0]


def quaternion(q: tuple) -> list[float]:
    """A PMX rotation as the stage's quatf: (x, y, z, w) -> (-x, -y, z, w)."""
    return [-f32(q[0]) + 0.0, -f32(q[1]) + 0.0, f32(q[2]) + 0.0, f32(q[3]) + 0.0]


def axial(v: tuple) -> list[float]:
    """An axial vector -- a torque: (x, y, z) -> (-x, -y, z), unscaled."""
    return [-f32(v[0]) + 0.0, -f32(v[1]) + 0.0, f32(v[2]) + 0.0]


def in_table(index: int, size: int) -> bool:
    """Whether an index names an element. The parser replaces an out-of-range
    one with -1, and the offset that held it is dropped (PMX §4, §10)."""
    return 0 <= index < size


def display_name(text) -> str:
    """A decoded name with trailing U+0000 padding dropped; text that does not
    decode is read as empty."""
    return text.rstrip("\0") if isinstance(text, str) else ""


def identifiers(kind: str, english: list[str]) -> list[str]:
    taken: set[str] = set()
    out = []
    for i, name in enumerate(english):
        candidate = re.sub(r"[^A-Za-z0-9_]+", "_", name).strip("_")[:64]
        if candidate[:1].isdigit():
            candidate = "_" + candidate
        candidate = candidate or f"{kind}_{i:04d}"
        chosen = candidate
        if chosen.lower() in taken:
            chosen = f"{candidate}_{i}"
            if chosen.lower() in taken:
                chosen = f"{kind}_{i:04d}"
                n = 2
                while chosen.lower() in taken:
                    chosen = f"{kind}_{i:04d}_{n}"
                    n += 1
        taken.add(chosen.lower())
        out.append(chosen)
    return out


def joint_order(parents: list[int]) -> tuple[list[int], list[int]]:
    """(repaired parents, canonical order as source indices)."""
    n = len(parents)
    fixed = [p if 0 <= p < n and p != i else -1 for i, p in enumerate(parents)]
    state = [0] * n  # 0 unvisited, 1 on the walk, 2 settled
    for start in range(n):
        walk, current = [], start
        while current != -1 and state[current] == 0:
            state[current] = 1
            walk.append(current)
            current = fixed[current]
        if current != -1 and state[current] == 1:
            fixed[min(walk[walk.index(current):])] = -1
        for bone in walk:
            state[bone] = 2
    children: list[list[int]] = [[] for _ in range(n)]
    ready: list[int] = []
    for i, p in enumerate(fixed):
        if p == -1:
            heapq.heappush(ready, i)
        else:
            children[p].append(i)
    order = []
    while ready:
        bone = heapq.heappop(ready)
        order.append(bone)
        for child in children[bone]:
            heapq.heappush(ready, child)
    return fixed, order


def asset_path(source) -> str | None:
    """The anchored logical path a texture string authors, or None when it
    authors none (unsafe, undecodable, or empty)."""
    if not isinstance(source, str):
        return None
    text = source.rstrip("\0")
    if re.search(r"[\x00-\x1f\x7f-\x9f]", text):  # a control character: C0, DEL, C1
        return None
    path = text.replace("\\", "/")
    if path.startswith("/") or re.match(r"[A-Za-z][A-Za-z0-9+.-]*:", path):
        return None
    segments: list[str] = []
    for segment in path.split("/"):
        if segment in ("", "."):
            continue
        if segment == ".." and segments and segments[-1] != "..":
            segments.pop()
            continue
        segments.append(segment)
    if not segments or segments[0] == "..":
        return None
    return "./" + "/".join(segments)


MORPH_TYPE_NAMES = {"group": "group", "vertex": "vertex", "bone": "bone", "uv": "uv",
                    "additionalUv1": "uv1", "additionalUv2": "uv2",
                    "additionalUv3": "uv3", "additionalUv4": "uv4",
                    "material": "material", "flip": "flip", "impulse": "impulse"}
MORPH_PANELS = ("hidden", "eyebrow", "eye", "mouth", "other")


def break_group_cycles(members: list[list[int]], composite: list[bool]) -> None:
    """Drop every member that closes a cycle through group or flip morphs
    (PMX_CONTRACT.md §10), in the walk order the canonicalizer uses: morphs in
    order, each one's members in theirs."""
    state = [0] * len(members)  # 0 unvisited, 1 on the walk, 2 settled
    for start in range(len(members)):
        if state[start] != 0 or not composite[start]:
            continue
        state[start] = 1
        walk = [[start, 0]]
        while walk:
            morph, nxt = walk[-1]
            if nxt >= len(members[morph]):
                state[morph] = 2
                walk.pop()
                continue
            walk[-1][1] += 1
            target = members[morph][nxt]
            if target == -1 or not composite[target]:
                continue
            if state[target] == 1:
                members[morph][nxt] = -1
                continue
            if state[target] == 2:
                continue
            state[target] = 1
            walk.append([target, 0])


def morph_expectation(model: dict, canonical_of: list[int]) -> list[dict]:
    """What each morph prim holds (STAGE_CONTRACT.md §11): its identity, its
    type and panel, and the offsets of the type it is, with every index
    remapped and every spatial value converted."""
    morphs = model["morphs"]
    ids = identifiers("morph", [display_name(m["englishName"]) for m in morphs])
    composite = [m["type"] in ("group", "flip") for m in morphs]
    vertices, bones = len(model["vertices"]), len(model["bones"])
    materials, bodies = len(model["materials"]), len(model["rigidBodies"])

    # Group and flip members, kept only where they name a morph, then with
    # every cycle broken.
    members = [[o[0] for o in m["offsets"] if in_table(o[0], len(morphs))]
               if composite[i] else [] for i, m in enumerate(morphs)]
    weights = [[o[1] for o in m["offsets"] if in_table(o[0], len(morphs))]
               if composite[i] else [] for i, m in enumerate(morphs)]
    break_group_cycles(members, composite)

    out = []
    for i, m in enumerate(morphs):
        kind = m["type"]
        panel = m["panel"]
        want = {"id": ids[i], "sourceIndex": i, "name": display_name(m["name"]),
                "englishName": display_name(m["englishName"]),
                "type": MORPH_TYPE_NAMES[kind],
                "panel": MORPH_PANELS[panel] if panel < len(MORPH_PANELS) else "other"}
        if kind in ("group", "flip"):
            kept = [k for k, member in enumerate(members[i]) if member != -1]
            want["members"] = [ids[members[i][k]] for k in kept]
            want["weights"] = [f32(weights[i][k]) for k in kept]
        elif kind == "vertex":
            kept = [o for o in m["offsets"] if in_table(o[0], vertices)]
            want["pointIndices"] = [o[0] for o in kept]
            want["offsets"] = [point(o[1]) for o in kept]
        elif kind == "bone":
            kept = [o for o in m["offsets"] if in_table(o[0], bones)]
            want["joints"] = [canonical_of[o[0]] for o in kept]
            want["translations"] = [point(o[1]) for o in kept]
            want["rotations"] = [quaternion(o[2]) for o in kept]
        elif kind in MORPH_TYPE_NAMES and kind.startswith(("uv", "additionalUv")):
            kept = [o for o in m["offsets"] if in_table(o[0], vertices)]
            want["pointIndices"] = [o[0] for o in kept]
            want["uvOffsets"] = [[f32(c) for c in o[1]] for o in kept]
        elif kind == "material":
            want["materialIndices"] = [o["material"] if in_table(o["material"], materials)
                                       else -1 for o in m["offsets"]]
            want["materialOperations"] = ["add" if o["operation"] == 1 else "multiply"
                                          for o in m["offsets"]]
            want["diffuseColors"] = [[f32(c) for c in o["diffuse"]] for o in m["offsets"]]
        elif kind == "impulse":
            kept = [o for o in m["offsets"] if in_table(o[0], bodies)]
            want["rigidBodyIndices"] = [o[0] for o in kept]
            want["impulseLocal"] = [bool(o[1]) for o in kept]
            want["velocities"] = [point(o[2]) for o in kept]
            want["torques"] = [axial(o[3]) for o in kept]
        out.append(want)
    return out


def rig_expectation(model: dict, canonical_of: list[int], order: list[int],
                    bone_ids: list[str]) -> dict:
    """What /Asset/rig holds (STAGE_CONTRACT.md §12): every joint's control
    semantics in canonical joint order, and one chain per IK bone whose
    effector names a bone, in bone-table order. A relation whose bone the
    parser rejects is dropped (PMX_CONTRACT.md §9)."""
    bones = model["bones"]

    def joint(index: int) -> int:
        return canonical_of[index] if in_table(index, len(bones)) else -1

    def mirrored(v: tuple) -> list[float]:
        """A local X axis: Z mirrored, neither scaled nor normalized."""
        return [f32(v[0]) + 0.0, f32(v[1]) + 0.0, -f32(v[2]) + 0.0]

    zero = [0.0, 0.0, 0.0]
    controls = []
    for source in order:
        b = bones[source]
        flags = b["flags"]
        tail_is_bone = bool(flags & TAIL_IS_BONE)
        appends = bool(flags & (APPEND_ROTATION | APPEND_TRANSLATION))
        append_source = joint(b["append"][0]) if appends else -1
        appended = append_source != -1
        local_axes = flags & LOCAL_AXES
        controls.append({
            "transformLayer": b["transformLayer"],
            "deformAfterPhysics": bool(flags & DEFORM_AFTER_PHYSICS),
            "rotatable": bool(flags & ROTATABLE),
            "translatable": bool(flags & TRANSLATABLE),
            "visible": bool(flags & VISIBLE),
            "operable": bool(flags & OPERABLE),
            "tailJoint": joint(b["tail"]) if tail_is_bone else -1,
            "tailOffset": zero if tail_is_bone else point(b["tail"]),
            "appendSource": append_source,
            "appendRatio": f32(b["append"][1]) if appended else 0.0,
            "appendRotation": appended and bool(flags & APPEND_ROTATION),
            "appendTranslation": appended and bool(flags & APPEND_TRANSLATION),
            "appendLocal": appended and bool(flags & LOCAL_APPEND),
            "hasFixedAxis": bool(flags & FIXED_AXIS),
            "fixedAxis": axial(b["fixedAxis"]) if flags & FIXED_AXIS else zero,
            "hasLocalAxes": bool(local_axes),
            "localAxisX": mirrored(b["localAxes"][0]) if local_axes else zero,
            "localAxisZ": axial(b["localAxes"][1]) if local_axes else zero,
            "hasExternalParent": bool(flags & EXTERNAL_PARENT),
            "externalParentKey": b["externalParentKey"] if flags & EXTERNAL_PARENT else 0,
        })

    chains = []
    for i, b in enumerate(bones):
        if not b["flags"] & IK or joint(b["ik"]["target"]) == -1:
            continue
        links = []
        for index, limits in b["ik"]["links"]:
            if joint(index) == -1:
                continue
            lower, upper = limits if limits is not None else (zero, zero)
            links.append({
                "joint": joint(index), "hasLimits": limits is not None,
                # About X and Y [min, max] -> [-max, -min]; Z unchanged.
                "lower": [-f32(upper[0]) + 0.0, -f32(upper[1]) + 0.0, f32(lower[2]) + 0.0],
                "upper": [-f32(lower[0]) + 0.0, -f32(lower[1]) + 0.0, f32(upper[2]) + 0.0],
            })
        chains.append({"id": bone_ids[i], "sourceIndex": i,
                       "name": display_name(b["name"]),
                       "englishName": display_name(b["englishName"]),
                       "joint": joint(i), "effector": joint(b["ik"]["target"]),
                       "loopCount": b["ik"]["loopCount"],
                       "limitAngle": f32(b["ik"]["limitAngle"]), "links": links})

    # The acceptance of Phase 5, in the source's own terms: every IK chain as
    # (IK bone, effector, links) and every append as (bone, source), by
    # bone-table index, for a check that reconstructs them from the stage.
    relations = {
        "ikChains": [[c["sourceIndex"], b["ik"]["target"],
                      [index for index, _ in b["ik"]["links"] if in_table(index, len(bones))]]
                     for c, b in ((c, bones[c["sourceIndex"]]) for c in chains)],
        "appends": [[i, b["append"][0]] for i, b in enumerate(bones)
                    if b["flags"] & (APPEND_ROTATION | APPEND_TRANSLATION)
                    and in_table(b["append"][0], len(bones))],
    }
    return {"bones": controls, "ikChains": chains, "sourceRelations": relations}


RIGID_BODY_SHAPES = ("sphere", "box", "capsule")
PHYSICS_MODES = ("followBone", "dynamic", "dynamicWithBone")
JOINT_TYPES = ("spring6Dof", "sixDof", "pointToPoint", "coneTwist", "slider", "hinge")


def quat_mul(a: list, b: list) -> list:
    """Hamilton product, (x, y, z, w), acting on column vectors."""
    return [a[3] * b[0] + b[3] * a[0] + (a[1] * b[2] - a[2] * b[1]),
            a[3] * b[1] + b[3] * a[1] + (a[2] * b[0] - a[0] * b[2]),
            a[3] * b[2] + b[3] * a[2] + (a[0] * b[1] - a[1] * b[0]),
            a[3] * b[3] - (a[0] * b[0] + a[1] * b[1] + a[2] * b[2])]


def quat_conj(q: list) -> list:
    return [-q[0], -q[1], -q[2], q[3]]


def euler_rotation(v: tuple) -> list:
    """A PMX Euler triple as the converted rotation, in double: R = Ry * Rx *
    Rz (PMX_CONTRACT.md §13, PMX-O1), then mirrored as S * R * S."""
    def about(axis: int, angle: float) -> list:
        half = f32(angle) * 0.5
        q = [0.0, 0.0, 0.0, math.cos(half)]
        q[axis] = math.sin(half)
        return q
    q = quat_mul(quat_mul(about(1, v[1]), about(0, v[0])), about(2, v[2]))
    return [-q[0], -q[1], q[2], q[3]]


def rounded(q: list) -> list[float]:
    """A quaternion as the stage's quatf: w never negative, no zero negative."""
    sign = -1.0 if q[3] < 0.0 else 1.0
    return [f32(sign * c) + 0.0 for c in q]


def scaled(v: float) -> float:
    return f32(f32(v) * METERS_PER_UNIT) + 0.0


def physics_expectation(model: dict, canonical_of: list[int]) -> dict | None:
    """What /Asset/physics holds (STAGE_CONTRACT.md §13): every rigid body in
    source order, and every joint whose bodies both exist, with every value
    through the conversion and the UsdPhysics realization beside it."""
    bodies = model["rigidBodies"]
    if not bodies:
        return None
    v21 = model["version"] == 2.1
    body_ids = identifiers("rigidBody", [display_name(b["englishName"]) for b in bodies])
    joint_ids = identifiers("joint", [display_name(j["englishName"]) for j in model["joints"]])
    frames = []
    out_bodies = []
    for i, b in enumerate(bodies):
        position = point_d(b["position"])
        rotation = euler_rotation(b["rotation"])
        frames.append((position, rotation))
        shape = b["shape"] if b["shape"] <= 2 else 0
        mode = b["physicsMode"] if b["physicsMode"] <= 2 else 0
        size = [f32(c) * METERS_PER_UNIT + 0.0 for c in b["size"]]
        out_bodies.append({
            "id": body_ids[i], "sourceIndex": i, "name": display_name(b["name"]),
            "englishName": display_name(b["englishName"]),
            "bone": canonical_of[b["bone"]] if in_table(b["bone"], len(canonical_of)) else -1,
            "shape": RIGID_BODY_SHAPES[shape], "size": size,
            "sizeFloat": [f32(c) for c in size],
            "position": position, "orientation": rounded(rotation),
            "collisionGroup": b["group"], "collisionMask": b["nonCollisionMask"],
            "mass": f32(b["mass"]), "linearDamping": f32(b["linearDamping"]),
            "angularDamping": f32(b["angularDamping"]),
            "restitution": f32(b["restitution"]), "friction": f32(b["friction"]),
            "mode": PHYSICS_MODES[mode], "kinematic": mode == 0,
        })

    out_joints = []
    for i, j in enumerate(model["joints"]):
        a, b = j["rigidBodies"]
        if not in_table(a, len(bodies)) or not in_table(b, len(bodies)):
            continue
        position_src, rotation_src, tmin, tmax, rmin, rmax, tspring, rspring = j["vectors"]
        kind = j["type"] if j["type"] <= (5 if v21 else 0) else 0
        position = point_d(position_src)
        rotation = euler_rotation(rotation_src)

        def local(body: int) -> tuple[list[float], list[float]]:
            body_position, body_rotation = frames[body]
            inverse = quat_conj(body_rotation)
            offset = [position[k] - body_position[k] for k in range(3)]
            p = quat_mul(quat_mul(inverse, [*offset, 0.0]), quat_conj(inverse))
            return [f32(c) + 0.0 for c in p[:3]], rounded(quat_mul(inverse, rotation))

        pos0, rot0 = local(a)
        pos1, rot1 = local(b)
        translation_lower = [scaled(tmin[0]), scaled(tmin[1]), -scaled(tmax[2]) + 0.0]
        translation_upper = [scaled(tmax[0]), scaled(tmax[1]), -scaled(tmin[2]) + 0.0]
        rotation_lower = [-f32(rmax[0]) + 0.0, -f32(rmax[1]) + 0.0, f32(rmin[2]) + 0.0]
        rotation_upper = [-f32(rmin[0]) + 0.0, -f32(rmin[1]) + 0.0, f32(rmax[2]) + 0.0]
        # UsdPhysics limits: only where the source's axis is not free, and
        # rotations in degrees.
        limits = {}
        for k, axis in enumerate(("transX", "transY", "transZ", "rotX", "rotY", "rotZ")):
            rotation_axis = k >= 3
            lower = (rotation_lower if rotation_axis else translation_lower)[k % 3]
            upper = (rotation_upper if rotation_axis else translation_upper)[k % 3]
            if lower <= upper:
                scale = 180.0 / math.pi if rotation_axis else 1.0
                limits[axis] = [f32(lower * scale) + 0.0, f32(upper * scale) + 0.0]
        out_joints.append({
            "id": joint_ids[i], "sourceIndex": i, "name": display_name(j["name"]),
            "englishName": display_name(j["englishName"]),
            "type": JOINT_TYPES[kind], "usdPhysics": kind <= 1,
            "rigidBodyA": a, "rigidBodyB": b, "body0": body_ids[a], "body1": body_ids[b],
            "position": position, "orientation": rounded(rotation),
            "translationLowerLimit": translation_lower,
            "translationUpperLimit": translation_upper,
            "rotationLowerLimit": rotation_lower, "rotationUpperLimit": rotation_upper,
            "translationSpring": [f32(c) for c in tspring],
            "rotationSpring": [f32(c) for c in rspring],
            "localPos0": pos0, "localRot0": rot0, "localPos1": pos1, "localRot1": rot1,
            "limits": limits,
        })

    # Phase 6's acceptance in the source's own terms: every rigid body as
    # (index, bone) and every joint as (index, body A, body B), by table
    # index, for a check that recovers them from the stage.
    relations = {
        "rigidBodies": [[i, b["bone"] if in_table(b["bone"], len(canonical_of)) else -1]
                        for i, b in enumerate(bodies)],
        "joints": [[j["sourceIndex"], j["rigidBodyA"], j["rigidBodyB"]] for j in out_joints],
    }
    return {"rigidBodies": out_bodies, "joints": out_joints, "sourceRelations": relations}


def stage_expectation(model: dict, relative: str) -> dict:
    """What the canonical stage of `model`, written at `relative`, holds."""
    bones = model["bones"]
    skinned = bool(bones)
    folder = pathlib.PurePosixPath(relative).parent

    def resolves(path: str | None) -> bool:
        return path is not None and (folder / path[2:]).as_posix() in TEXTURES

    parents, order = joint_order([b["parent"] for b in bones])
    bone_ids = identifiers("bone", [display_name(b["englishName"]) for b in bones])
    canonical_of = {source: c for c, source in enumerate(order)}
    paths: dict[int, str] = {}
    joints = []
    for source in order:
        parent = parents[source]
        paths[source] = bone_ids[source] if parent == -1 \
            else paths[parent] + "/" + bone_ids[source]
        joints.append({"path": paths[source], "sourceIndex": source,
                       "name": display_name(bones[source]["name"]),
                       "englishName": display_name(bones[source]["englishName"]),
                       "bind": point_d(bones[source]["position"])})

    materials = []
    material_ids = identifiers("material", [display_name(m["englishName"])
                                            for m in model["materials"]])
    first = 0
    faces = len(model["faces"]) // 3
    double_sided = False
    for i, m in enumerate(model["materials"]):
        count = min(m["faceCount"] // 3, faces - first)
        slots = {}
        toon_kind, toon_value = m["toon"]
        for slot, index in (("texture", m["texture"]), ("sphereTexture", m["sphereTexture"]),
                            ("toonTexture", toon_value if toon_kind == "texture" else -1)):
            if 0 <= index < len(model["textures"]):
                source = model["textures"][index]
                asset = asset_path(source)
                slots[slot] = {"source": source if isinstance(source, str) else "",
                               "asset": asset, "resolves": resolves(asset)}
        materials.append({"id": material_ids[i], "sourceIndex": i,
                          "name": display_name(m["name"]),
                          "englishName": display_name(m["englishName"]),
                          "faces": [first, count], "doubleSided": bool(m["flags"] & 0x01),
                          "textures": slots})
        double_sided |= bool(m["flags"] & 0x01) and count > 0
        first += count

    mesh = None
    if model["vertices"]:
        deforms = [v["deform"]["type"] for v in model["vertices"]]
        influences = 0
        if skinned:
            influences = max({"BDEF1": 1, "BDEF2": 2, "SDEF": 2}.get(d, 4) for d in deforms)
        v1 = model["vertices"][min(1, len(model["vertices"]) - 1)]
        normal = [f32(c) for c in v1["normal"]]
        normal[2] = -normal[2] + 0.0
        mesh = {
            "points": len(model["vertices"]), "faces": faces,
            "doubleSided": double_sided,
            "familyType": None if not any(m["faces"][1] for m in materials)
            else "partition" if first == faces else "nonOverlapping",
            "influences": influences,
            "sdef": skinned and "SDEF" in deforms,
            "additionalUv": model["additionalVec4"],
            # A vertex through the conversion (STAGE_CONTRACT.md §6.3).
            "vertex1": {"point": point(v1["position"]), "normal": normal,
                        "st": [f32(v1["uv"][0]), f32(1.0 - f32(v1["uv"][1]))]},
        }
    joint_of_source = [canonical_of[i] for i in range(len(bones))]
    morphs = morph_expectation(model, joint_of_source)
    if mesh is not None:
        # Blend shapes deform only beneath a SkelRoot (STAGE_CONTRACT.md §4.1).
        mesh["blendShapes"] = [m["id"] for m in morphs
                               if m["type"] == "vertex"] if skinned else []
    rig = rig_expectation(model, joint_of_source, order, bone_ids) if skinned else None
    physics = physics_expectation(model, joint_of_source)
    return {"skinned": skinned, "mesh": mesh, "materials": materials,
            "joints": joints, "morphs": morphs, "rig": rig, "physics": physics,
            "jointOfSourceBone": joint_of_source}


# --- the fixtures ------------------------------------------------------------------

def opens(model: dict, purpose: str, parser: list[str] | None = None,
          canonical: list[str] | None = None,
          importer: list[str] | None = None) -> tuple[bytes, dict, dict]:
    """A fixture that opens. The stage records the parser's codes, then the
    canonical model's, then the importer's (DIAGNOSTICS.md §4)."""
    data, _ = encode(model)
    parser = parser or []
    name = model["info"][0]
    return data, {
        "purpose": purpose, "opens": True,
        "sourceVersion": f"{model['version']:.1f}",
        "modelName": name if isinstance(name, str) else "",
        "counts": counts(model),
        "parserDiagnostics": parser,
        "diagnostics": parser + (canonical or []) + (importer or []),
    }, model


def fails(data: bytes, purpose: str, fatal: str) -> tuple[bytes, dict, None]:
    return data, {"purpose": purpose, "opens": False, "fatal": fatal}, None


def patched(model: dict, table: str, delta: int, value: bytes) -> bytes:
    """The model's bytes with `value` written at a table's count offset plus
    `delta`."""
    data, offsets = encode(model)
    at = offsets[table] + delta
    return data[:at] + value + data[at + len(value):]


def fixtures() -> dict[str, tuple[bytes, dict]]:
    """Every fixture: relative path -> (bytes, expectation)."""
    entries = {}
    for relative, (data, expectation, model) in _fixtures().items():
        if model is not None:
            expectation["stage"] = stage_expectation(model, relative)
        entries[relative] = (data, expectation)
    return entries


def _fixtures() -> dict[str, tuple[bytes, dict, dict | None]]:
    # The importer's once-per-import codes for the sample models: their SDEF
    # vertex, their QDEF vertex (2.1), their soft body (2.1).
    sdef = ["MMD_SKEL_SDEF_APPROXIMATED"]
    v21 = sdef + ["MMD_SKEL_QDEF_APPROXIMATED", "MMD_PHYSICS_SOFT_BODY_UNSUPPORTED"]
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
            "record variant, Japanese names", importer=sdef),
        "sample-2.1-utf8.pmx": opens(
            sample_model(2.1, UTF8, 2, 2),
            "PMX 2.1, UTF-8, index width 2, two additional vec4: QDEF, flip "
            "and impulse morphs, a soft body",
            importer=v21),
        "sample-2.1-utf16-wide.pmx": opens(
            sample_model(2.1, UTF16LE, 4, 4),
            "PMX 2.1, UTF-16LE, index width 4, four additional vec4",
            importer=v21),
    }

    # Phase 2: what canonicalization repairs, each on its own. The ones that
    # only normalize (info) sit beside the samples, so their textures resolve.
    reordered = sample_model(2.0, UTF16LE, 1, 0)
    reordered["bones"][1]["parent"] = 3  # listed before its parent
    named = sample_model(2.0, UTF16LE, 1, 0)
    named["info"][0] = "サンプル2.0\0\0"          # padded with U+0000
    named["materials"][1]["englishName"] = "Hair"  # "hair" is material 0's
    named["bones"][1]["englishName"] = ""          # falls back to bone_0001
    named["bones"][2]["englishName"] = "2nd elbow"
    named["bones"][3]["englishName"] = "CENTER"    # "center" is bone 0's
    cycle = sample_model(2.0, UTF16LE, 1, 0)
    cycle["bones"][1]["parent"] = 2  # 1 -> 2 -> 1
    cycle["bones"][2]["parent"] = 1
    cycle["bones"][3]["parent"] = 3  # its own parent
    weights = sample_model(2.0, UTF16LE, 1, 0)
    weights["vertices"][2]["deform"]["weights"] = [1.0, 0.5, 0.5, 0.0]  # sums to 2
    weights["vertices"][0]["deform"] = deform("BDEF4", [0, 1, -1, -1],
                                              [0.0, 0.0, 0.0, 0.0])
    unsafe = sample_model(2.0, UTF16LE, 1, 0)
    unsafe["textures"] = ["..\\toon\\共有.bmp", "C:\\tex\\肌.png", "/tex/光沢.sph",
                          "http://example.com/トゥーン.bmp", "sph\\光\t沢.sph",
                          "toon\\\u0085.bmp"]
    unsafe["materials"][1]["sphereTexture"] = 4  # a C0 control, a tab
    unsafe["materials"][1]["toon"] = ("texture", 5)  # a C1 control, U+0085
    panel = sample_model(2.0, UTF16LE, 1, 0)
    panel["morphs"][3]["panel"] = 7  # only 0-4 are defined
    cycles = sample_model(2.0, UTF16LE, 1, 0)
    cycles["morphs"][1]["offsets"] = [(1, 1.0), (0, 1.0)]  # names itself, then a vertex morph
    cycles["morphs"] += [
        {"name": "輪1", "englishName": "", "panel": 4, "type": "group",
         "offsets": [(7, 1.0)]},
        {"name": "輪2", "englishName": "", "panel": 4, "type": "group",
         "offsets": [(6, 1.0)]},  # 6 -> 7 -> 6
    ]
    boneless = sample_model(2.0, UTF16LE, 1, 0)
    boneless["bones"] = []
    boneless["morphs"] = []
    boneless["displayFrames"] = []
    boneless["rigidBodies"] = []
    boneless["joints"] = []
    for v in boneless["vertices"][:3]:
        v["deform"] = deform("BDEF1", [-1])
    boneless["vertices"] = boneless["vertices"][:3]
    boneless["faces"] = [0, 1, 2]
    boneless["materials"] = boneless["materials"][:1]
    morphs_unskinned = {**boneless, "morphs": [
        {"name": "まばたき", "englishName": "blink", "panel": 2, "type": "vertex",
         "offsets": [(0, (0.0, -0.1, 0.0)), (2, (0.0, -0.05, 0.02))]},
        {"name": "材質", "englishName": "material", "panel": 4, "type": "material",
         "offsets": [{"material": 0, "operation": 1,
                      "diffuse": (0.0, 0.0, 0.0, -0.5), "specular": (0.0, 0.0, 0.0),
                      "specularPower": 0.0, "ambient": (0.0, 0.0, 0.0),
                      "edgeColor": (0.0, 0.0, 0.0, 0.0), "edgeSize": 0.0,
                      "textureTint": (0.0, 0.0, 0.0, 0.0),
                      "sphereTint": (0.0, 0.0, 0.0, 0.0),
                      "toonTint": (0.0, 0.0, 0.0, 0.0)}]},
    ]}
    # Bones but no mesh: /Asset is a SkelRoot, and there is still nothing to
    # name a blend shape (STAGE_CONTRACT.md §11.1).
    morphs_no_mesh = sample_model(2.0, UTF16LE, 1, 0)
    morphs_no_mesh.update({"vertices": [], "faces": [], "materials": [], "textures": [],
                           "displayFrames": [], "rigidBodies": [], "joints": []})
    morphs_no_mesh["morphs"] = [
        {"name": "まばたき", "englishName": "blink", "panel": 2, "type": "vertex",
         "offsets": [(0, (0.0, -0.1, 0.0))]},  # vertex 0 of an empty table
    ]
    # Control relations that name bones the table does not hold, and an IK
    # bone whose effector is none: each relation is dropped, the rest kept.
    broken = sample_model(2.0, UTF16LE, 1, 0)
    broken["bones"][1]["tail"] = 9
    broken["bones"][2]["append"] = (9, 0.5)
    broken["bones"][3]["ik"]["links"].insert(1, (9, None))
    broken["bones"] += [
        bone("足ＩＫ", "LegIK", (1.0, 1.0, 0.0), 0, TAIL_IS_BONE | IK, -1,
             ik={"target": 9, "loopCount": 40, "limitAngle": 2.0, "links": [(0, None)]}),
        bone("つま先ＩＫ", "ToeIK", (1.0, 0.0, -1.0), 0, TAIL_IS_BONE | IK, -1,
             ik={"target": -1, "loopCount": 3, "limitAngle": 4.0, "links": []}),
    ]
    # Physics the source does not define or cannot connect: a body whose bone
    # the table does not hold stays, unattached; a shape, a mode and a joint
    # type out of range fall back; a joint UsdPhysics has no type for is
    # preserved without one; a joint whose body is missing, or none, goes.
    physics = sample_model(2.1, UTF8, 1, 0)
    physics["rigidBodies"][0]["bone"] = 9
    physics["rigidBodies"][1]["shape"] = 3
    physics["rigidBodies"][2]["physicsMode"] = 5
    physics["joints"][1]["type"] = 6
    hinge = physics["joints"][0]
    physics["joints"] += [
        {**hinge, "name": "蝶番", "englishName": "hinge", "type": 5, "rigidBodies": (0, 2)},
        {**hinge, "name": "無し", "type": 0, "rigidBodies": (0, 7)},
        {**hinge, "name": "空", "type": 0, "rigidBodies": (-1, 1)},
    ]
    entries.update({
        "recoverable/physics-repairs.pmx": opens(
            physics, "a rigid body whose bone is missing, a shape, a physics mode and "
            "a joint type out of range, a hinge joint, and joints whose body is "
            "missing or none: each repaired, preserved or dropped",
            parser=["MMD_PMX_INDEX_OUT_OF_RANGE"] * 2,
            canonical=["MMD_PHYSICS_UNKNOWN_SHAPE", "MMD_PHYSICS_UNKNOWN_MODE",
                       "MMD_PHYSICS_UNKNOWN_JOINT_TYPE"],
            importer=sdef + ["MMD_SKEL_QDEF_APPROXIMATED", "MMD_PHYSICS_JOINT_UNMAPPED",
                             "MMD_PHYSICS_SOFT_BODY_UNSUPPORTED"]),
        "recoverable/rig-broken-relations.pmx": opens(
            broken, "a tail, an append source, an IK link and an IK effector "
            "that name no bone, and an IK bone whose effector is none: each "
            "relation is dropped and the rest of the rig kept",
            parser=["MMD_PMX_INDEX_OUT_OF_RANGE"] * 4, importer=sdef),
        "bones-reordered.pmx": opens(
            reordered, "a bone listed before its parent: the joints are reordered",
            canonical=["MMD_SKEL_JOINTS_REORDERED"], importer=sdef),
        "identifiers.pmx": opens(
            named, "names padded with U+0000, an empty English name, one that "
            "starts with a digit, and two that collide ignoring case",
            canonical=["MMD_TEXT_TRAILING_NUL", "MMD_USD_IDENTIFIER_COLLISION",
                       "MMD_USD_IDENTIFIER_COLLISION"], importer=sdef),
        "recoverable/bone-parent-cycle.pmx": opens(
            cycle, "two bones each other's parent, and one its own",
            canonical=["MMD_SKEL_INVALID_PARENT", "MMD_SKEL_PARENT_CYCLE"],
            importer=sdef),
        "recoverable/weights-normalized.pmx": opens(
            weights, "BDEF4 weights that sum to 2, and ones that sum to 0",
            canonical=["MMD_SKEL_WEIGHTS_NORMALIZED", "MMD_SKEL_ZERO_WEIGHTS"],
            importer=sdef),
        "recoverable/unsafe-texture-paths.pmx": opens(
            unsafe, "texture paths that leave the model's directory, name a "
            "drive, are absolute, carry a scheme, and hold a C0 and a C1 "
            "control character",
            canonical=["MMD_PATH_UNSAFE_TEXTURE_PATH"] * 6, importer=sdef),
        "recoverable/no-bones.pmx": opens(
            boneless, "a mesh and a material, and no bones: /Asset is an Xform "
            "and the mesh is unskinned",
            parser=["MMD_PMX_INDEX_OUT_OF_RANGE"] * 3),
        "recoverable/morph-unknown-panel.pmx": opens(
            panel, "a morph panel of 7: preserved as \"other\"",
            canonical=["MMD_MORPH_UNKNOWN_PANEL"], importer=sdef),
        "recoverable/morph-group-cycle.pmx": opens(
            cycles, "a group morph that names itself, and two that name each "
            "other: each cyclic member is dropped",
            canonical=["MMD_MORPH_GROUP_CYCLE"] * 2, importer=sdef),
        "recoverable/morph-no-skeleton.pmx": opens(
            morphs_unskinned, "a vertex morph in a model with no bones: "
            "preserved without a UsdSkelBlendShape",
            parser=["MMD_PMX_INDEX_OUT_OF_RANGE"] * 3,
            importer=["MMD_MORPH_NO_SKELETON"]),
        "recoverable/morph-no-mesh.pmx": opens(
            morphs_no_mesh, "a vertex morph in a model with bones and no "
            "mesh: nothing can name a blend shape, and the offset of a "
            "vertex the empty table does not hold is dropped",
            parser=["MMD_PMX_INDEX_OUT_OF_RANGE"],
            importer=["MMD_MORPH_NO_SKELETON"]),
    })

    # Phase 10: what packaging does with each kind of texture
    # (PACKAGING_POLICY.md §14). Each opens as the sample does; the textures
    # it names are in packaging/.
    def packaging(textures: list[str], materials: list[dict]) -> dict:
        model = sample_model(2.0, UTF16LE, 1, 0)
        model["textures"] = textures
        model["materials"] = materials
        return model

    entries.update({
        "packaging/textures.pmx": opens(packaging(
            ["tex\\髪.png", "spa\\光沢.spa", "toon\\トゥーン.tga", "tex\\肌.jpg",
             "sph\\反射.sph", "tex\\目.bmp"],
            [material("髪", "hair", 3, ("texture", 2), texture=0, sphere=1, flags=0x01),
             material("肌", "skin", 3, ("texture", 2), texture=3, sphere=4),
             material("目", "eye", 3, ("shared", 0), texture=5)]),
            "every texture kind packaging decides on: PNG and JPEG kept, BMP "
            "under .spa and .bmp and a TGA converted, a PNG under .sph renamed, "
            "one toon two materials share", importer=sdef),
        "packaging/missing-texture.pmx": opens(packaging(
            ["tex\\髪.png", "tex\\無い.png"],
            [material("髪", "hair", 3, ("shared", 0), texture=0),
             material("無い", "missing", 6, ("shared", 0), texture=1)]),
            "a texture that does not resolve: no package", importer=sdef),
        "packaging/unsupported-texture.pmx": opens(packaging(
            ["tex\\髪.png", "tex\\動く.gif"],
            [material("髪", "hair", 3, ("shared", 0), texture=0),
             material("動く", "animated", 6, ("shared", 0), texture=1)]),
            "a GIF, which USDZ does not hold and packaging does not convert: "
            "no package", importer=sdef),
        "packaging/name-collision.pmx": opens(packaging(
            ["toon\\影.bmp", "toon\\影.bmp.png"],
            [material("影", "shade", 3, ("texture", 0)),
             material("影2", "shade2", 6, ("texture", 1))]),
            "a BMP whose converted name is another texture's: no package",
            importer=sdef),
    })

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
            parser=["MMD_TEXT_INVALID_UTF8"], importer=sdef),
        "recoverable/invalid-utf16-name.pmx": opens(
            bad_utf16, "a bone's English name of odd byte length: read as empty",
            parser=["MMD_TEXT_INVALID_UTF16"], importer=sdef),
        # The weighted none is dropped, and the vertex's one remaining weight
        # rescaled to 1.
        "recoverable/index-out-of-range.pmx": opens(
            bad_index, "a weighted deform of no bone, a missing texture, a "
            "missing parent: each read as none",
            parser=["MMD_PMX_INDEX_OUT_OF_RANGE"] * 3,
            canonical=["MMD_SKEL_WEIGHTS_NORMALIZED"], importer=sdef),
        "recoverable/material-faces-short.pmx": opens(
            short, "the materials draw 6 of the 9 face indices",
            parser=["MMD_PMX_MATERIAL_FACES_SHORT"], importer=sdef),
        "recoverable/trailing-bytes.pmx": opens(
            trailing, "16 bytes after the soft-body table",
            parser=["MMD_PMX_TRAILING_BYTES"], importer=v21),
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


def files() -> dict[str, bytes]:
    """Every file the generator writes but the manifest: relative path ->
    bytes."""
    written = {relative: data for relative, (data, _) in fixtures().items()}
    written.update(TEXTURES)
    return written


def write(out: pathlib.Path) -> int:
    entries = fixtures()
    for relative, data in files().items():
        path = out / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    (out / MANIFEST).write_bytes(manifest_text(entries).encode("utf-8"))
    print(f"wrote {len(entries)} fixtures, {len(TEXTURES)} textures and {MANIFEST} "
          f"to {out}")
    return 0


def check(out: pathlib.Path) -> int:
    entries = fixtures()
    expected = files()
    problems: list[str] = []
    for relative, data in expected.items():
        path = out / relative
        if not path.is_file():
            problems.append(f"missing: {relative}")
        elif path.read_bytes() != data:
            problems.append(f"differs from the generator: {relative}")
    # Everything else in the directory is the generator's too, except the
    # manifest and the importer's goldens.
    committed = {p.relative_to(out).as_posix() for p in out.rglob("*")
                 if p.is_file() and p.name != MANIFEST
                 and not p.name.endswith(".golden.usda")}
    for extra in sorted(committed - set(expected)):
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
    print(f"{len(entries)} fixtures and {len(TEXTURES)} textures in {out} match the "
          f"generator")
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
