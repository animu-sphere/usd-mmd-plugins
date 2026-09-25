#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Open a PMX in a session that registers the importer but not mmdSchema.

From stage-contract v2 every material applies MmdMaterialAPI
(docs/design/MATERIAL_POLICY.md §4.3). The importer's plugInfo.json names
the schema type as a plugin dependency, so Plug loads mmdSchema's library
first -- which is also how the importer's own library finds it with no
search path set -- and refuses to load the importer when the type is
unknown. A host that leaves the bundle out gets an error naming the schema,
never a stage whose materials lost it.
"""

from __future__ import annotations

import argparse
import pathlib
import sys

from pxr import Tf, Usd


def main() -> int:
    sys.stdout.reconfigure(errors="backslashreplace")
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--fixtures", required=True, type=pathlib.Path)
    args = parser.parse_args()

    registry = Usd.SchemaRegistry()
    assert registry.FindAppliedAPIPrimDefinition("MmdMaterialAPI") is None, \
        "MmdMaterialAPI is registered; this test needs a session without mmdSchema"

    path = args.fixtures / "sample-2.1-utf8.pmx"
    try:
        stage = Usd.Stage.Open(str(path))
    except Tf.ErrorException as error:
        assert "unknown dependent class 'UsdMmdMaterialAPI'" in str(error), \
            f"the error does not name the missing schema: {error}"
        print("ok  the importer did not load, naming the missing UsdMmdMaterialAPI")
        return 0
    raise AssertionError(f"{path.name} opened (as {stage}) without mmdSchema registered")


if __name__ == "__main__":
    sys.exit(main())
