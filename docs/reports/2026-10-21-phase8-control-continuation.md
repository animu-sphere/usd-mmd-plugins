# Phase 8 owner control continuation (2026-10-21)

## Scope and environment

This change implements the owner continuation required by
[MOTION §11.10](../design/MOTION_CONTRACT.md#1110-intended-physics-continuation-boundary).
It follows the [boundary probe](2026-10-21-phase8-control-boundary.md), preserving
the meanings of offline `Evaluate` and independent retarget `Complete`.

- Base MMD revision: `2494bf4c70df75a33471caa773de6ce07beae78b`, plus this
  report's implementation and tests.
- Windows x86_64, Visual Studio 18 2026, Release configuration, OpenUSD 26.08.
- Generated PMX/VMD documents and committed generated fixture assets only.
- Shared motion dependencies: the installed packages selected by the component
  manifests; no sibling source checkout is composed into the build.

`BeginPhysics` samples motion and runs the pre-physics control partition.
Its owned snapshot exposes the barrier pose. `ResumePhysics` reconciles
sparse joint-local feedback, then runs the post-physics partition over a fresh
copy. The snapshot retains sampled morph contributions/channels, IK enables
and visibility. It borrows no model, motion or evaluator storage. The owning
contract defines feedback reconciliation and fatal validation diagnostics.

## Generated assertions

| Case | Asserted outcome |
| --- | --- |
| Source at 60 degrees / 0.25 m, then feedback at 120 degrees / 0.5 m | Pre append remains 30 degrees / 1.125 m; post append is 60 degrees / 1.25 m; sampled channel stays 0.25 |
| Feedback replaces a pre joint carrying a Z append with an X rotation | The pre result equals the edited transform; the post follower reads the new X rotation once, without retaining the old Z contribution |
| Feedback on a post append joint | Feedback is the barrier base; its scheduled post append executes once |
| Bound VMD with post IK on at frame 0, off/hidden at frame 10 | Barrier ankle is at rest; resume solves only the enabled chain, can rotate pre links, and preserves sampled visibility |
| Changed post IK goal | Enabled chain reaches the new goal and its post follower sees the solved rotation; the disabled chain leaves the ankle at rest |
| Same snapshot resumed repeatedly; fresh begin at the same time | Identical results; no feedback history accumulates and the barrier pose stays unchanged |
| Empty rig, evaluator copies/moves, destroyed motion/original evaluator copy | Snapshot resumes without borrowed storage |
| Default/foreign snapshot, duplicate/out-of-range indices, zero/infinite quaternion, NaN translation | Fatal diagnostics; no output pose or mutation of the snapshot |
| 20,000 generated rigs at five times, including non-finite source data | 100,000 cases: empty-feedback resume agrees bit for bit with offline evaluation and replay; existing finite/unit-rotation checks still pass |

## Validation

```powershell
cmake --build build/windows-msvc --config Release
ctest --test-dir build/windows-msvc -C Release -R '^(mmdControl_|mmdMotionAdapter_|mmdSkeletonAdapter_|workspace_installed_consumer$)' --output-on-failure
python scripts/check_docs.py --selftest
python scripts/check_docs.py
git diff --check
```

The initial broader build found an existing local dependency-cache mismatch:
the selected `motionSource` referenced basis-conversion symbols absent from
the selected `motionCore`. Reconfiguring those two package directories to the
installed artifacts pinned by `mmdSkeletonAdapter`/`mmdMotionAdapter` resolved
the link failure. No source, dependency manifest or package pin was changed
to repair the local cache.

After reconfiguration, the full Release build and all 10 selected tests pass,
including the isolated installed-consumer lane. Documentation links, version
mirrors, the 68-code diagnostic catalog and `git diff --check` pass.
The docs selftest's default sandbox temporary directory denied writes;
the same workspace-local temporary-directory workaround recorded by the
boundary probe passes:

```powershell
python -c 'import pathlib, runpy, sys, tempfile; tempfile.tempdir = str(pathlib.Path("build/docs-check-temp").resolve()); sys.argv = ["scripts/check_docs.py", "--selftest"]; runpy.run_path("scripts/check_docs.py", run_name="__main__")'
```

## Evidence boundary

The installed `control_probe` compares offline and empty-feedback split
evaluation after binding the generated VMD/PMX fixtures. The generated unit
suite covers actual sparse feedback and post IK dependencies. These establish
an owner API and a small model/motion integration smoke, without introducing
a physics library dependency into `mmdControl` or the importer.

No coupling adapter, body association, model/world placement or scale
conversion, backend step/reset, runtime callback, shared-world ordering,
renderer or avatar L3 acceptance is established here. Those remain the
separate gates in the [current roadmap](../roadmap/current.md).
