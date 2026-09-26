#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""mmd_export against the generated fixtures (docs/design/PACKAGING_POLICY.md §14).

The fixtures are written into a scratch directory by the workspace generator
(docs/architecture/WORKSPACE.md §3), then packaged by the tool, which finds
the importer on PXR_PLUGINPATH_NAME as any host does:

  * a fixture that packages exits 0 and writes the package, relaying the
    importer's diagnostics first and naming each converted texture once;
  * a separate process with no MMD plugin opens each package, checks §8 and
    §9's post-write list, and `usdchecker` passes it;
  * the .pmx opened with the plugins and the .usdz opened without them hold
    the same prims, schemas, metadata and values, asset paths compared
    through §7's renaming, which this test derives on its own;
  * a converted texture decodes to the generator's pixels, and a kept one is
    the source's bytes; the archive's first entry is the root layer and the
    rest are in byte order;
  * a missing, unsupported or colliding texture, and a model the importer
    refuses, write nothing and leave an existing output as it was;
  * the whole run again from a directory, and into one, that no single ANSI
    code page can spell (without `usdchecker` on Windows, which cannot open
    such a path).
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import os
import pathlib
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile
import zlib

UNICODE_DIR = "ユニコード-é"
PROBE = pathlib.Path(__file__).resolve().parent / "stage_probe.py"


def load_generator(path: pathlib.Path):
    spec = importlib.util.spec_from_file_location("generate_fixtures", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class Context:
    def __init__(self, args: argparse.Namespace) -> None:
        self.tool = args.tool
        self.usdchecker = args.usdchecker
        self.with_plugins = dict(os.environ)
        self.with_plugins["PXR_PLUGINPATH_NAME"] = os.pathsep.join(
            str(p) for p in args.plugin_path)
        self.without_plugins = dict(os.environ)
        self.without_plugins.pop("PXR_PLUGINPATH_NAME", None)

    def package(self, pmx: pathlib.Path, usdz: pathlib.Path) -> tuple[int, list[str]]:
        result = subprocess.run([str(self.tool), str(pmx), str(usdz)],
                                env=self.with_plugins, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE)
        return result.returncode, result.stderr.decode("utf-8").splitlines()

    def probe(self, stage: pathlib.Path, out: pathlib.Path, package: bool) -> dict:
        command = [sys.executable, str(PROBE), str(stage), "--out", str(out)]
        if package:
            command.append("--package")
        result = subprocess.run(command, env=self.without_plugins if package
                                else self.with_plugins,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        assert result.returncode == 0, (
            f"{stage}: the {'plugin-free' if package else 'plugin'} probe failed\n"
            + result.stdout.decode("utf-8", "replace")
            + result.stderr.decode("utf-8", "replace"))
        return json.loads(out.read_text(encoding="utf-8"))


# --- §7, restated here, so the tool is checked against a second reading ---------

def expected_archive_name(relative: str, data: bytes) -> str:
    """The archive name §7 gives the file the stage names as `relative`."""
    extension = relative.rsplit(".", 1)[-1].lower() if "." in relative.rsplit("/", 1)[-1] else ""
    if extension in ("exr", "avif"):
        return relative
    if data.startswith(b"\x89PNG\r\n\x1a\n"):
        return relative if extension == "png" else relative + ".png"
    if data.startswith(b"\xff\xd8\xff"):
        return relative if extension in ("jpg", "jpeg") else relative + ".jpg"
    if data.startswith(b"BM") or extension == "tga":
        return relative + ".png"
    raise AssertionError(f"{relative} is not a texture packaging accepts")


def converted(relative: str, data: bytes) -> bool:
    """Decoded and written anew, rather than stored or renamed."""
    return (expected_archive_name(relative, data) != relative
            and not data.startswith((b"\x89PNG\r\n\x1a\n", b"\xff\xd8\xff")))


# --- the archive ------------------------------------------------------------------

def entries(usdz: pathlib.Path) -> list[tuple[str, bytes]]:
    """Every entry, in order, with its UTF-8 name: OpenUSD's writer does not
    set the ZIP UTF-8 flag, so zipfile decodes the name as CP437 (PKG-O2)."""
    out = []
    with zipfile.ZipFile(usdz) as archive:
        for info in archive.infolist():
            name = info.filename
            if not info.flag_bits & 0x800:
                name = name.encode("cp437").decode("utf-8")
            out.append((name, archive.read(info)))
    return out


def decode_png(data: bytes) -> list[list[tuple[int, int, int, int]]]:
    """An 8-bit, non-interlaced PNG as RGBA rows, top to bottom."""
    assert data.startswith(b"\x89PNG\r\n\x1a\n")
    at, idat = 8, b""
    while at < len(data):
        length, tag = struct.unpack(">I4s", data[at:at + 8])
        body = data[at + 8:at + 8 + length]
        if tag == b"IHDR":
            width, height, depth, color, _, _, interlace = struct.unpack(">IIBBBBB", body)
        elif tag == b"IDAT":
            idat += body
        at += 12 + length
    assert depth == 8 and interlace == 0, "only 8-bit, non-interlaced PNG is expected"
    channels = {0: 1, 2: 3, 4: 2, 6: 4}[color]
    raw = zlib.decompress(idat)
    stride = width * channels
    rows, previous, at = [], bytearray(stride), 0
    for _ in range(height):
        kind, line = raw[at], bytearray(raw[at + 1:at + 1 + stride])
        at += 1 + stride
        for i in range(stride):
            a = line[i - channels] if i >= channels else 0
            b = previous[i]
            c = previous[i - channels] if i >= channels else 0
            if kind == 1:
                line[i] = (line[i] + a) & 0xFF
            elif kind == 2:
                line[i] = (line[i] + b) & 0xFF
            elif kind == 3:
                line[i] = (line[i] + (a + b) // 2) & 0xFF
            elif kind == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 0xFF
        rows.append(line)
        previous = line
    out = []
    for line in rows:
        pixels = []
        for x in range(width):
            p = line[x * channels:(x + 1) * channels]
            if channels == 1:
                pixels.append((p[0], p[0], p[0], 255))
            elif channels == 2:
                pixels.append((p[0], p[0], p[0], p[1]))
            elif channels == 3:
                pixels.append((p[0], p[1], p[2], 255))
            else:
                pixels.append(tuple(p))
        out.append(pixels)
    return out


# --- the checks -------------------------------------------------------------------

def texture_paths(expectation: dict) -> list[str]:
    """The asset paths the stage authors for a fixture, as fixtures.json has
    them: "./tex/髪.png"."""
    paths = []
    for material in expectation["stage"]["materials"]:
        for slot in material["textures"].values():
            if slot["asset"] and slot["asset"] not in paths:
                paths.append(slot["asset"])
    return paths


def check_packages(ctx: Context, fixtures: pathlib.Path, manifest: dict, generator,
                   out_dir: pathlib.Path, work: pathlib.Path, names: list[str],
                   usdchecker: bool = True) -> None:
    for name in names:
        expectation = manifest[name]
        pmx = fixtures / name
        usdz = out_dir / (pathlib.PurePosixPath(name).stem + ".usdz")
        status, lines = ctx.package(pmx, usdz)
        where = f"{name} -> {usdz.name}"
        assert status == 0, f"{where}: exit {status}\n" + "\n".join(lines)
        assert usdz.is_file(), f"{where}: no package"

        # The importer's diagnostics first, as recorded; then one info per
        # converted texture.
        codes = [line.split(":", 1)[0] for line in lines]
        relayed = expectation["diagnostics"]
        assert codes[:len(relayed)] == relayed, f"{where}: relayed {codes}"
        folder = pmx.parent
        sources = {p[2:]: (folder / p[2:]).read_bytes() for p in texture_paths(expectation)}
        conversions = sorted(r for r, data in sources.items() if converted(r, data))
        assert codes[len(relayed):] == ["MMD_PKG_TEXTURE_CONVERTED"] * len(conversions), \
            f"{where}: {lines[len(relayed):]}"

        # The archive: the root layer, then every texture once, in byte order.
        names_in = {r: expected_archive_name(r, data) for r, data in sources.items()}
        archive = entries(usdz)
        stem = usdz.stem
        assert archive[0][0] == stem + ".usdc", f"{where}: first entry {archive[0][0]}"
        assert [n for n, _ in archive[1:]] == sorted(
            names_in.values(), key=lambda s: s.encode("utf-8")), \
            f"{where}: entries {[n for n, _ in archive]}"
        stored = dict(archive)
        for relative, data in sources.items():
            if relative in conversions:
                prefix = folder.relative_to(fixtures).as_posix()
                key = relative if prefix == "." else f"{prefix}/{relative}"
                image = generator.PACKAGING_IMAGES.get(key)
                if image:
                    assert decode_png(stored[names_in[relative]]) == image["pixels"], \
                        f"{where}: {relative} does not decode to the source's pixels"
            else:
                assert stored[names_in[relative]] == data, \
                    f"{where}: {relative} is not stored byte for byte"

        # Opened without any MMD plugin, it is the .pmx's stage.
        packaged = ctx.probe(usdz, work / (stem + ".usdz.json"), package=True)
        imported = ctx.probe(pmx, work / (stem + ".pmx.json"), package=False)
        renames = {"./" + r: "./" + n for r, n in names_in.items()}
        assert rename_assets(imported, renames) == packaged, \
            f"{where}: the package's stage is not the model's\n" + \
            first_difference(rename_assets(imported, renames), packaged)

        if ctx.usdchecker and usdchecker:
            result = subprocess.run([str(ctx.usdchecker), str(usdz)],
                                    env=ctx.without_plugins, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT)
            assert result.returncode == 0, (
                f"{where}: usdchecker failed\n" + result.stdout.decode("utf-8", "replace"))
        print(f"ok  {name}: {len(sources)} textures, {len(conversions)} converted")


def rename_assets(value, renames: dict):
    if isinstance(value, dict):
        if set(value) == {"asset"}:
            return {"asset": renames.get(value["asset"], value["asset"])}
        return {k: rename_assets(v, renames) for k, v in value.items()}
    if isinstance(value, list):
        return [rename_assets(v, renames) for v in value]
    return value


def first_difference(a, b, where: str = "") -> str:
    if isinstance(a, dict) and isinstance(b, dict):
        for key in sorted(set(a) | set(b)):
            if a.get(key) != b.get(key):
                return first_difference(a.get(key), b.get(key), f"{where}/{key}")
    return f"{where}: {a!r} != {b!r}"


def check_refusals(ctx: Context, fixtures: pathlib.Path, out_dir: pathlib.Path) -> None:
    cases = {
        "packaging/missing-texture.pmx": (1, "MMD_PKG_MISSING_ASSET"),
        "packaging/unsupported-texture.pmx": (1, "MMD_PKG_UNSUPPORTED_TEXTURE"),
        "packaging/name-collision.pmx": (1, "MMD_PKG_ASSET_NAME_COLLISION"),
        "malformed/bad-signature.pmx": (2, "MMD_PKG_INPUT_UNREADABLE"),
        "absent.pmx": (2, "MMD_PKG_INPUT_UNREADABLE"),
    }
    sentinel = b"an older package"
    for name, (status, code) in cases.items():
        usdz = out_dir / "refused.usdz"
        usdz.write_bytes(sentinel)
        got, lines = ctx.package(fixtures / name, usdz)
        codes = [line.split(":", 1)[0] for line in lines]
        assert got == status, f"{name}: exit {got}, expected {status}\n" + "\n".join(lines)
        assert code in codes, f"{name}: {code} not among {codes}"
        assert usdz.read_bytes() == sentinel, f"{name}: the existing output changed"
        assert sorted(p.name for p in out_dir.iterdir() if p.name.startswith("refused")) == \
            ["refused.usdz"], f"{name}: a partial file was left beside the output"
        if name.startswith("malformed/"):
            assert codes.index("MMD_PMX_BAD_SIGNATURE") < codes.index(code), \
                f"{name}: the importer's fatal code is not printed first"
        print(f"ok  {name}: {code}, nothing written")

    usage = subprocess.run([str(ctx.tool), str(fixtures / "minimal.pmx"),
                            str(out_dir / "minimal.usda")], env=ctx.with_plugins,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    assert usage.returncode == 3 and not (out_dir / "minimal.usda").exists(), \
        "an output that is not .usdz is not a usage error"
    without = subprocess.run([str(ctx.tool), str(fixtures / "minimal.pmx"),
                              str(out_dir / "minimal.usdz")], env=ctx.without_plugins,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    assert without.returncode == 2 and b"MMD_PKG_INPUT_UNREADABLE" in without.stderr, \
        "without the importer on the plugin path, the input is not unreadable"
    print("ok  usage errors and a missing importer")


def main() -> int:
    sys.stdout.reconfigure(errors="backslashreplace")
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--tool", required=True, type=pathlib.Path)
    parser.add_argument("--generator", required=True, type=pathlib.Path)
    parser.add_argument("--plugin-path", action="append", required=True, type=pathlib.Path)
    parser.add_argument("--usdchecker", type=pathlib.Path)
    args = parser.parse_args()
    ctx = Context(args)
    if not ctx.usdchecker:
        print("note: no usdchecker in this OpenUSD; the in-tool validators still run")
    generator = load_generator(args.generator)

    packaged = ["packaging/textures.pmx", "sample-2.0-utf16.pmx", "minimal.pmx"]
    with tempfile.TemporaryDirectory(prefix="mmd-export-") as scratch:
        root = pathlib.Path(scratch)
        fixtures = root / "fixtures"
        subprocess.run([sys.executable, str(args.generator), "--out", str(fixtures)],
                       check=True, stdout=subprocess.DEVNULL)
        manifest = json.loads((fixtures / "fixtures.json").read_text(encoding="utf-8"))
        work = root / "work"
        work.mkdir()

        out_dir = root / "out"
        out_dir.mkdir()
        check_packages(ctx, fixtures, manifest, generator, out_dir, work, packaged)
        check_refusals(ctx, fixtures, out_dir)

        # From, and into, directories no single ANSI code page can spell.
        unicode_fixtures = root / UNICODE_DIR / "fixtures"
        shutil.copytree(fixtures, unicode_fixtures)
        unicode_out = root / "出力-é"
        unicode_out.mkdir()
        # OpenUSD's usdchecker is not built with a UTF-8 code page, so on
        # Windows it cannot open this path at all; the tool ran the same
        # validators in process, and the plugin-free probe still opens it.
        check_packages(ctx, unicode_fixtures, manifest, generator, unicode_out, work,
                       ["packaging/textures.pmx"], usdchecker=sys.platform != "win32")
    print("mmd_export fixture tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
