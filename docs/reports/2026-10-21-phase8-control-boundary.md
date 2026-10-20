# Phase 8 MMD control boundary probe (2026-10-21)

## Scope and environment

This probe checks the owner evaluation boundary before connecting bone/body
coupling to runtime callbacks. It uses constructed PMX/VMD documents and the
actual `mmdControl`, canonicalization, VMD motion construction and binding
implementations. No third-party assets, physics backend, solver step or
renderer participate.

- MMD base revision: `7ab0543add71aa350d292b9863f0e43997547cba`, plus this
  report's test changes.
- Runtime contract inspected: `usd-avatar-runtime`
  `b59f452aedfe2ed66d1c005f30b0f5472507bc0e`, clean checkout.
- Windows x86_64, Visual Studio 18 2026, Release configuration.
- Tests: `libs/mmdControl/tests/test_evaluate.cpp`,
  `TestPhysicsBoundaryAppends` and `TestThroughBind`.

The runtime's `ArPhysicsParticipant::post_physics_evaluator` selects a cut in
the existing ordered evaluator plan. Prefixes run before the shared Step and
suffixes after it. Its contract permits the cut inside a phase when explicit
dependencies order the callbacks. That scheduling mechanism does not split
an atomic owner call; no runtime execution is claimed by this inspection.

## Reproduced behavior

The append fixture has a source, a pre-physics child and an after-physics
grandchild, both appending half the source rotation and translation. Rest
translations are 1 m for both children. All asserted values below are local
to each joint's parent, in the canonical basis and metres.

The source starts at 60 degrees about Z and 0.25 m translation. A synthetic
feedback edit changes it to 120 degrees and 0.5 m after `Evaluate` returns.

| Operation | Pre control: rotation / local X | Post control: rotation / local X |
| --- | --- | --- |
| Offline `Evaluate` | 30 degrees / 1.125 m | 30 degrees / 1.125 m |
| Edit source only | unchanged | stale at 30 degrees / 1.125 m |
| `Complete` over that output | 90 degrees / 1.375 m | 90 degrees / 1.375 m |
| `Complete`, source and pre held | unchanged | still 90 degrees / 1.375 m |
| `Complete`, all held | unchanged | still stale |
| Required continuation result, analytically derived | unchanged | 60 degrees / 1.25 m |

`Evaluate` already executes after-physics appends before returning.
`Complete` treats the previous append contribution as new motion and appends
again. Holding the pre joint suppresses its duplicate operation, but does
not remove the old post contribution; holding post prevents its update.
The sampled vertex-morph channel weight of 0.25 is also cleared by `Complete`.

The VMD path constructs a leg rig with after-physics IK and an after-physics
append follower. At frame 0, IK reaches the raised target at
`(0, 0.2, 0.05)` m even though no physics step occurred. At frame 10, the
bound VMD disables IK and hides the model: the ankle remains at rest at
`(0, 0, 0)`. Passing that pose to `Complete` enables the chain again,
reaches the raised target and makes the model visible. Repeating `Evaluate`
at the same time reproduces its original pose exactly in both fixtures.

These are assertions of the existing offline/retarget contracts, not failures
of those APIs and not evidence of native MMD trajectory equivalence. They
demonstrate why neither API can be used as a physics continuation unchanged.

## Decision and remaining boundary

Keep MMD control ordering with the format owner. Define the continuation
requirements in [MOTION §11.10](../design/MOTION_CONTRACT.md#1110-intended-physics-continuation-boundary)
before introducing an optional coupling component. Preserve the offline
`Evaluate` and retarget `Complete` meanings.

The inspected runtime plan cut can place ordered callbacks on either side of
Step without a new phase enum. Actual owner continuation and callback
integration must still validate model-specific dependencies, including
post-physics IK affecting earlier links. This probe does not validate the
whole scheduler DAG, add a runtime provider or establish a published API.
All body modes, offsets, placements/scale, malformed associations, reset,
shared-world ordering and final snapshot acceptance remain separate coupling
work under the [current roadmap](../roadmap/current.md).

## Validation

```powershell
cmake --build build/windows-msvc --config Release --target mmdControl_tests mmdControl_robustness_tests
ctest --test-dir build/windows-msvc -C Release -R '^mmdControl_' --output-on-failure
python scripts/check_docs.py --selftest
python scripts/check_docs.py
```

The sandbox's default temporary directory rejected the docs selftest's file
writes. Re-running with a workspace-local Python temporary directory passed:

```powershell
New-Item -ItemType Directory -Force build/docs-check-temp | Out-Null
python -c 'import pathlib, runpy, sys, tempfile; tempfile.tempdir = str(pathlib.Path("build/docs-check-temp").resolve()); sys.argv = ["scripts/check_docs.py", "--selftest"]; runpy.run_path("scripts/check_docs.py", run_name="__main__")'
```

The focused CTest selection passes 3/3: unit, robustness and dependency
boundaries. The unit executable runs the generated append boundary fixture
and VMD-to-bound-motion integration smoke. Documentation checks and their
selftest pass. No backend, composed avatar, installed-consumer or renderer
acceptance is inferred from these results.
