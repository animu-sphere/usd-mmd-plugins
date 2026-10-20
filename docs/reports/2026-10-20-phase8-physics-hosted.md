# Phase 8 hosted installed physics verification (2026-10-20)

## Scope and provenance

The corrected [Windows Actions run](https://github.com/animu-sphere/usd-mmd-plugins/actions/runs/39359197058)
completed successfully on 2026-10-20. It tests PR #74 head
`d7d1253c326d4b8cd6d2a45d8639440948f48f4d` through GitHub's synthetic merge
commit `5493172601805e426e6bc0634862b39f0e5d2609`. Both the recorded checkout
SHA and installed MMD build stamp identify that synthetic merge. PR #74 was
subsequently merged as `4a4e431b7521b6f6cce45381049d4444139ee6fc`.

The subsequent [main run](https://github.com/animu-sphere/usd-mmd-plugins/actions/runs/39359833947)
also completed successfully. Its uploaded checkout revision and installed
build stamp both identify `4a4e431b7521b6f6cce45381049d4444139ee6fc`.
Its JUnit results confirm the same 7/7, 7/7 and 11/11 passing cases without
skips, and its logs confirm all five representative inputs and identical
120-step replay in both execution configurations.

This supplies hosted evidence missing from the
[local wiring report](2026-10-20-phase8-physics-ci.md), following the
[Python SDK correction](2026-10-20-phase8-physics-ci-python.md).
The evidence is the run log and its
[physics-installed-windows artifact](https://github.com/animu-sphere/usd-mmd-plugins/actions/runs/39359197058/artifacts/12236941287):
JUnit results, configure/build/test logs, CMake caches, configuration JSON,
installed build information and source revisions. The uploaded archive digest
reported by Actions is
`ac773ba36ca57f9c06d7e02db71a95d1939aa449ad5e75cdf1d586524b9757dc`.

## Environment and results

Windows Server 2022 x86_64, runner image `20261012.333.1`, Visual Studio 17
2022 generator, MSVC `19.44.35229.0`, Python `3.13.16`, OpenUSD `26.08`,
Release configuration. Workflow pins and runtime inputs remain owned by
[the workflow](../../.github/workflows/physics-installed.yml) and
[the CI manifest](../../openstrata.ci.yaml); the artifact records their
resolved values. The selected host Python executable, headers and import
library appear in all consumer configurations.

The installed shared packages were built from
`54a3406fb42fb47c926737b36ef10e4b771cbe6c`, with deterministic Jolt source
`23dadd0e603f1b321142d4c74df07fce85064989`. These are observed source
revisions, not new production dependency requirements.

| Configuration | Execution | Translation / rotation springs | CTests |
| --- | --- | --- | --- |
| reader | OFF | OFF / OFF | 7/7 pass |
| execution | ON | OFF / OFF | 7/7 pass |
| springs | ON | ON / ON | 11/11 pass |

All 25 JUnit cases ran without failures, disabled cases or skips. Each
configuration asserted the five representative PMX inputs: `sample-2.0-utf16`,
`sample-2.1-utf8`, `recoverable/physics-repairs`, `bones-reordered` and
`minimal`. Both execution configurations report identical exposed-state replay
over 120 fixed steps for each input; the empty `minimal` case has zero body
samples. Nonempty fixtures report 360 replay samples each.

The separate foreign-Python-SDK regression step also passed. The uploaded
caches confirm `MMD_PHYSICS_FULL_CORPUS=OFF`, the intended execution/spring
options and consumption of the fresh installed MMD prefix.

## Evidence boundary

This verifies the bounded installed-package lane on the hosted Windows
configuration. Source spring gaps, experimental spring approximation labels
and same-build replay limits remain as described in the
[capability matrix](../reference/CAPABILITY_MATRIX.md#physics-preservation-and-execution).
It does not establish published shared packages, cross-platform determinism,
Bullet trajectory equivalence, bone/body coupling or avatar L3 acceptance.
Active work belongs to [current.md](../roadmap/current.md).
