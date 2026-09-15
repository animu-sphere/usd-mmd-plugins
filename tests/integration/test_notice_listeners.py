#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Open a PMX from a Python host that listens to every change notice.

The importer authors the stage on a worker thread (UsdMmdFileFormat.cpp says
why) and waits for it. A global Python notice listener then runs on that
worker thread and needs the GIL -- which the calling thread holds while it
waits, unless the importer releases it. This test registers listeners for
the notices authoring sends, opens every fixture that opens, and fails --
rather than hanging -- if an open does not return.
"""

from __future__ import annotations

import argparse
import faulthandler
import pathlib
import sys
import threading

from pxr import Sdf, Tf, Usd

import stage_checks

TIMEOUT_SECONDS = 60


def main() -> int:
    sys.stdout.reconfigure(errors="backslashreplace")
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--fixtures", required=True, type=pathlib.Path)
    args = parser.parse_args()

    # A hang is the failure this test exists for: dump every thread's stack
    # and exit non-zero instead of waiting for the CTest timeout.
    faulthandler.dump_traceback_later(TIMEOUT_SECONDS, exit=True)

    heard: dict[str, set[int]] = {"ObjectsChanged": set(), "LayersDidChange": set()}

    def on_objects_changed(notice, sender):
        heard["ObjectsChanged"].add(threading.get_ident())

    def on_layers_did_change(notice, sender):
        heard["LayersDidChange"].add(threading.get_ident())

    listeners = [
        Tf.Notice.RegisterGlobally(Usd.Notice.ObjectsChanged, on_objects_changed),
        Tf.Notice.RegisterGlobally(Sdf.Notice.LayersDidChange, on_layers_did_change),
    ]

    manifest = stage_checks.load_manifest(args.fixtures)
    opening = {k: v for k, v in sorted(manifest.items()) if v["opens"]}
    for relative, expectation in opening.items():
        path = str(args.fixtures / relative)
        # Every Python entry point that reaches the importer's Read: each
        # wrapper decides for itself whether it releases the GIL.
        stage = Usd.Stage.Open(path)
        stage_checks.check_opened_stage(stage, expectation, relative)
        layer = Sdf.Layer.FindOrOpen(path)
        assert layer and layer.Reload(force=True), f"{relative}: reload failed"
        anonymous = Sdf.Layer.OpenAsAnonymous(path)
        assert anonymous and anonymous.defaultPrim == "Asset", \
            f"{relative}: OpenAsAnonymous did not read the layer"
        print(f"ok  {relative}")

    faulthandler.cancel_dump_traceback_later()
    del listeners
    assert heard["LayersDidChange"], \
        "no LayersDidChange notice reached Python: the test listened to nothing"
    print(f"{len(opening)} fixtures opened with global notice listeners "
          f"(heard on {len(heard['LayersDidChange'])} thread(s))")
    return 0


if __name__ == "__main__":
    sys.exit(main())
