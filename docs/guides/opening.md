# Opening a PMX as a stage

With the plugin built ([building.md](building.md)), a `.pmx` is an ordinary
USD layer: any OpenUSD host whose plugin path includes the bundle opens it,
and what it authors is the stage contract
([STAGE_CONTRACT.md](../design/STAGE_CONTRACT.md)). Every command on this page
has been run, on Windows 11 in PowerShell, on 2026-09-15.

## Through the plugin's runtime session

`ost plugin run` starts a command with the bundle on `PXR_PLUGINPATH_NAME`
and the runtime's OpenUSD on the loader path:

```powershell
ost plugin run plugins\usdMmdFileFormat -- usdcat plugins\usdMmdFileFormat\tests\fixtures\sample-2.0-utf16.pmx
```

prints the stage as `.usda`. `--flatten --out <file>` writes it to a file,
which is how the goldens are made; flattening turns every asset path into an
absolute one, so a flattened file is tied to the machine that wrote it.

```powershell
ost plugin run plugins\usdMmdFileFormat -- usdchecker plugins\usdMmdFileFormat\tests\fixtures\sample-2.1-utf8.pmx
```

runs every validator OpenUSD registers — UsdGeom's, UsdShade's, UsdSkel's and
the rest — over the stage, and prints `Success!`. The integration tests run
the same validators over every fixture. A texture that is not beside the
model fails it (`MissingReferenceValidator`): copy a model with its texture
folders.

On Windows, OpenUSD's command-line tools receive their arguments in the
system's ANSI code page and read them as UTF-8, so a path the code page
spells differently — any non-ASCII path on a CP932 or CP1252 host — arrives
mangled and the tool reports `Could not open layer` before the plugin is
reached. That is the host, not the importer
([TEXT_ENCODING_POLICY.md §4](../design/TEXT_ENCODING_POLICY.md#4-no-locale-anywhere));
open such a file from Python instead, which passes paths as Unicode.

## From Python, or any other host

A host needs the bundle's registration directory on `PXR_PLUGINPATH_NAME` and
OpenUSD 26.08 on its loader path
([PACKAGE_CONTRACT.md](../architecture/PACKAGE_CONTRACT.md#usdmmdfileformat)):

```powershell
$usd = "<OpenUSD 26.08 install>"
$env:PATH = "$usd\bin;$usd\lib;$env:PATH"
$env:PYTHONPATH = "$usd\lib\python"
$env:PXR_PLUGINPATH_NAME = "$PWD\plugins\usdMmdFileFormat\plugin\resources\usdMmdFileFormat"
python -c "from pxr import Usd; print(Usd.Stage.Open('model.pmx').GetRootLayer().ExportToString())"
```

The Python must be the one OpenUSD was built against (3.13 for 26.08).

With an OpenUSD install that includes imaging — the `ost` runtime session
has none — the same environment renders a stage with Storm:

```powershell
python "$usd\bin\usdrecord" --imageWidth 600 model.pmx model.png
```

Phase 3 authors VRM-like unlit `preview` and MaterialX `mtlx` material graphs,
so the portable display path carries source colors through emission while MMD
specific sphere, toon, edge and other semantics remain on the material prim.

## What to look at

- `/Asset.customData` holds the model's names and comment, the contract
  version, and `mmd:diagnostics` — every repair the import made, as
  `CODE: message`; the codes are in
  [DIAGNOSTICS.md](../reference/DIAGNOSTICS.md).
- `/Asset/skel/Skeleton` carries each joint's source name in
  `mmd:bone:sourceName`, in joint order; joint paths use stable ASCII
  identifiers ([TEXT_ENCODING_POLICY.md §6](../design/TEXT_ENCODING_POLICY.md#6-stable-identifiers)).
- A texture whose path is refused keeps its source string in the material's
  `customData` and has no `mmd:material:*` asset attribute
  ([TEXT_ENCODING_POLICY.md §7.2](../design/TEXT_ENCODING_POLICY.md#72-unsafe-paths)).
- `/Asset/morph` holds one prim per PMX morph. A vertex morph is a
  `UsdSkelBlendShape` the mesh lists in `skel:blendShapes`, so any UsdSkel
  consumer that authors blend-shape weights over the stage — the importer
  authors none — deforms the mesh with it; every other type is a typeless
  prim whose `mmd:morph:*` properties preserve the source semantics without
  evaluating them ([STAGE_CONTRACT.md §11](../design/STAGE_CONTRACT.md#11-morphs)).
- `/Asset/rig` holds each bone's control semantics, never solved:
  `/Asset/rig/Bones` has one `mmd:rig:*` array per property, element `j`
  describing joint `j` of the Skeleton, and `/Asset/rig/ik` one prim per IK
  chain, named by its IK bone, whose `mmd:rig:effector` and
  `mmd:rig:linkJoints` are joint indices too
  ([STAGE_CONTRACT.md §12](../design/STAGE_CONTRACT.md#12-control-rig)).
