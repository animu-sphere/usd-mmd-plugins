# Phase 8 coupling adapter (2026-10-21)

The mode-1 no-feedback interpretation recorded here is corrected by the [2026-10-25 follow-up](2026-10-25-phase8-dynamic-bone-feedback.md).

## Scope and environment

Adds optional `mmdPhysicsAdapter` under
[PHYSICS §5.4](../design/PHYSICS_INTEGRATION.md#54-coupling-lifecycle).
Base revision: `5386cb3` plus this change. Windows x86_64, Visual Studio 18
2026, Release, OpenUSD 26.08; installed motion packages selected by the
existing manifests. No physics package or backend is linked.

The generated consumer follows avatar-runtime's evaluator split:
`BeginPhysics` -> kinematic commands -> caller body snapshot -> local feedback
-> `ResumePhysics`. It implements no runtime callbacks or physics step.
Body paths are the explicit boundary for later caller-owned world handles.

## Generated assertions

- All three modes independently: followBone emits a kinematic update;
  dynamic has no bone feedback; dynamicWithBone changes rotation while
  preserving pre-step local translation.
- Parent/child feedback in canonical hierarchy order, independent of body or
  snapshot order, including non-driven intermediate joints, noncommuting
  rotations, nonzero rest offsets and rotated/translated placement.
- Stage lengths in metres and centimetres produce metre-valued output.
  PMX basis and length conversion are not repeated on the imported stage.
- Uniform/nonuniform scale, reflection, singular transforms and animated
  placement reject. Only rigid unit-scale frames are supported.
- Missing associations (-1) have diagnostics and no coupling. Invalid bone
  indices, varying/time-sampled/blocked/wrong-typed declarations, unknown modes,
  kinematic disagreement, disabled bodies, duplicate/cross-stage prims and
  ambiguous duplicate feedback joints reject.
- Invalid pose sizes/quaternions and unknown, missing, duplicate or non-finite
  snapshot rows reject without returning partial results.
- Bindings survive stage destruction; repeated calls and reset/replay retain
  no feedback history.

`coupling-modes.pmx` is generated entirely by the committed fixture generator.
It has three bodies, three bones and a post-physics append. The existing
sample VMD drives its root. The integration consumer supplies a 60-degree
body rotation and unrelated dynamic-body translation/rotation, then checks
one kinematic command, one feedback joint, unchanged root and bone position,
a 30-degree post append and three identical reset/replays. The same consumer
builds and runs outside the source tree against installed packages.

## Validation

```powershell
cmake --build build/windows-msvc --config Release
ctest --test-dir build/windows-msvc -C Release -R '^(mmdPhysicsAdapter_|mmdControl_|mmdMotionAdapter_|mmdSkeletonAdapter_|workspace_fixtures$|usdMmdFileFormat_(stage_open|physics_roundtrip)$|workspace_installed_consumer$)' --output-on-failure
python scripts/check_docs.py
python -c 'import pathlib, runpy, sys, tempfile; tempfile.tempdir = str(pathlib.Path("build/docs-check-temp").resolve()); sys.argv = ["scripts/check_docs.py", "--selftest"]; runpy.run_path("scripts/check_docs.py", run_name="__main__")'
git diff --check
```

The full Release build and all 50 workspace tests pass, including the
installed-consumer lane, importer opening/physics round-trip, export fixtures,
control robustness and the coupling smoke. Documentation checks match all 73
declared diagnostic codes; formatting checks and `git diff --check` pass.

The sandbox's default temporary directory intermittently denied child-process
writes in the installed-consumer lane. The successful full run uses a
workspace-local temporary directory, as the earlier control reports did for
the docs selftest:

```powershell
New-Item -ItemType Directory -Force -Path build/test-temp | Out-Null
$env:TMP = (Resolve-Path build/test-temp).Path
$env:TEMP = $env:TMP
ctest --test-dir build/windows-msvc -C Release --output-on-failure
```

## Evidence boundary

This establishes generated coupling and an installed PMX/VMD continuation
consumer with synthetic body snapshots. It does not establish live runtime
callbacks, path-to-world-handle integration, a shared physics step, backend
reset, renderer consumption or avatar L3 acceptance. Non-unit scale remains
explicitly unsupported. The [current roadmap](../roadmap/current.md) retains
P1-1 until a runtime consumer validates that boundary, followed by PR-C and
representative scene acceptance.
