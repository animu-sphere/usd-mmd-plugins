# Exporting a model as USDZ

`mmd_export` writes the stage the importer authors for a `.pmx` out as
conventional OpenUSD, in the format its output's extension names. This
version writes one format: a USDZ that holds the stage and every texture it
names, and opens in an OpenUSD installation that has none of this
repository's plugins
([PACKAGING_POLICY.md](../design/PACKAGING_POLICY.md)). Every command on this
page has been run, on Windows 11 in PowerShell, on 2026-09-26.

The build puts it in `tools/mmdExport/bin/`, and an install in the prefix's
`bin/` ([building.md](building.md)).

## Running it

The tool opens the model through the registered importer, as any OpenUSD
host does, so the importer and its schema must be on OpenUSD's plugin path,
and OpenUSD's libraries on `PATH`. In a development tree, name the two
bundles by absolute path (Plug does not resolve a relative one); an
OpenStrata session over the installed product arranges both itself.

```powershell
$U = "C:/usd/openusd-26.08-usdview"
$env:PATH = "$U/bin;$U/lib;$env:PATH"
$env:PXR_PLUGINPATH_NAME = "$PWD\plugins\mmdSchema\plugin\resources\mmdSchema;$PWD\plugins\usdMmdFileFormat\plugin\resources\usdMmdFileFormat"
tools\mmdExport\bin\mmd_export.exe plugins\usdMmdFileFormat\tests\fixtures\sample-2.0-utf16.pmx build\export\sample.usdz
```

```text
MMD_SKEL_SDEF_APPROXIMATED: 1 vertex is SDEF, skinned here by linear blending; C, R0 and R1 are preserved in primvars:mmd:sdefC, sdefR0 and sdefR1 (vertices)
MMD_PKG_TEXTURE_CONVERTED: 'sph/光沢.sph' is stored as the PNG 'sph/光沢.sph.png'
MMD_PKG_TEXTURE_CONVERTED: 'toon/ト ゥ ー ン.bmp' is stored as the PNG 'toon/ト ゥ ー ン.bmp.png'
```

The first line is the importer's, relayed as the stage records it. The
others name each texture USDZ cannot hold, here a BMP and a sphere map that
holds one, converted to PNG without loss
([§7](../design/PACKAGING_POLICY.md#7-texture-formats)). The package is the
root layer first, then the textures at the paths the stage names them by:

```text
sample.usdc              16905
sph/光沢.sph.png            69
tex/肌.png                  73
tex/髪.png                  73
toon/ト ゥ ー ン.bmp.png     69
```

The output is written only once the package has passed validation, the
validators `usdchecker` runs included
([§9](../design/PACKAGING_POLICY.md#9-validation)). With no MMD plugin on
the path at all, it opens and checks clean:

```powershell
Remove-Item Env:PXR_PLUGINPATH_NAME
usdchecker build\export\sample.usdz
```

```text
Validation Result with no explicit variants set
Success!
```

On Windows, OpenUSD 26.08's `usdchecker` cannot open a path with characters
outside the ANSI code page. `mmd_export` can, and it has already run the
same validators before writing.

## When it writes nothing

A texture that does not resolve, one USDZ cannot hold and the tool does not
convert, or a converted name the model already has, stops the run. The
output path is left as it was:

```powershell
tools\mmdExport\bin\mmd_export.exe plugins\usdMmdFileFormat\tests\fixtures\packaging\missing-texture.pmx build\export\missing.usdz
```

```text
MMD_SKEL_SDEF_APPROXIMATED: 1 vertex is SDEF, skinned here by linear blending; C, R0 and R1 are preserved in primvars:mmd:sdefC, sdefR0 and sdefR1 (vertices)
MMD_PKG_MISSING_ASSET: './tex/無い.png' does not resolve beside the model
```

Every diagnostic goes to standard error, `CODE: message`
([reference/DIAGNOSTICS.md](../reference/DIAGNOSTICS.md)). The exit status
is `0` when the package was written, `1` when an error stopped it, `2` when
the model could not be read or the output not written, and `3` for a usage
error, such as an output that is not `.usdz`
([§11](../design/PACKAGING_POLICY.md#11-diagnostics-and-exit-status)).

## What a consumer sees

The package needs neither `usdMmdFileFormat` nor `mmdSchema`. Materials still
carry `MmdMaterialAPI` in their `apiSchemas`, and its
`inputs:mmd:material:*` values; an installation without `mmdSchema` shows
them as ordinary attributes, and the `preview` and `mtlx` realizations draw
from them as they do from the `.pmx`
([§8](../design/PACKAGING_POLICY.md#8-what-a-consumer-without-this-repository-sees)).
