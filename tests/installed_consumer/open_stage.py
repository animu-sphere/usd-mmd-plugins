# SPDX-License-Identifier: Apache-2.0
"""Open a PMX through the installed plugin, as a consumer outside the
repository would: the only plugin path is the install prefix's.

  open_stage.py <prefix> <file.pmx>

Fails unless the .pmx format is served by the plugin under <prefix> and the
stage carries the contract's skeleton of a skinned model: /Asset the
SkelRoot, the mesh bound to the skeleton, a texture resolving beside the file
(docs/design/STAGE_CONTRACT.md §14). The integration tests assert the rest.
"""

import os
import sys

from pxr import Plug, Sdf, Usd, UsdGeom, UsdSkel

prefix = os.path.normcase(os.path.realpath(sys.argv[1]))
path = sys.argv[2]

plugin = Plug.Registry().GetPluginWithName("UsdMmdFileFormat")
assert plugin is not None, "UsdMmdFileFormat is not registered"
loaded_from = os.path.normcase(os.path.realpath(plugin.path))
assert loaded_from.startswith(prefix + os.sep), \
    f"the plugin was found at {plugin.path}, outside {sys.argv[1]}"

assert Sdf.FileFormat.FindByExtension("pmx").CanRead(path), "CanRead is false"
stage = Usd.Stage.Open(path)
assert stage is not None, "the stage did not open"
assert stage.GetRootLayer().defaultPrim == "Asset"
assert UsdGeom.GetStageUpAxis(stage) == UsdGeom.Tokens.y
assert UsdGeom.GetStageMetersPerUnit(stage) == 1.0
asset = stage.GetPrimAtPath("/Asset")
assert asset.GetCustomDataByKey("mmd:stageContractVersion") == 1
assert asset.IsA(UsdSkel.Root), f"/Asset is a {asset.GetTypeName()}"
mesh = stage.GetPrimAtPath("/Asset/geo/Mesh")
assert UsdSkel.BindingAPI(mesh).GetSkeleton().GetPath() == Sdf.Path("/Asset/skel/Skeleton")
texture = stage.GetPrimAtPath("/Asset/mtl/hair").GetAttribute("mmd:material:texture").Get()
assert texture.resolvedPath, f"{texture.path} does not resolve beside {path}"
print(f"opened {os.path.basename(path)} through {plugin.path}")
