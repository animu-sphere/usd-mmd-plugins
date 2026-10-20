# Phase 8 installed physics CI wiring (2026-10-20)

Follow-up: [hosted Windows verification](2026-10-20-phase8-physics-hosted.md).

## Scope

P0-2 CI wiring at MMD baseline `a34b3b5`, plus this working-tree change.
The hand-written [Windows workflow](../../.github/workflows/physics-installed.yml)
builds the importer, installs deterministic Jolt and compatible shared physics
packages from pinned test-only sources, then runs the existing bounded reader,
execution and experimental spring suites against installed packages. Runtime
and ost pins are read from the owning CI manifest. Exact shared/Jolt source
revisions live in the workflow, not in the production dependency graph.

The [driver](../../scripts/run_physics_ci.py) requires an already built MMD
workspace and installed physics/SDK prefixes. It creates a new MMD installation,
clears inherited plugin discovery, and explicitly selects representative
fixtures and spring settings. Verbose CTest logs retain per-prim fidelity and
replay evidence; JUnit and CMake caches identify failing configurations.
The workflow uploads this evidence on success or failure, along with runtime
inputs, tool checksum and source revisions. No package publication is performed.

## Local validation

Windows x86_64, MSVC 19.51.36256.0, CMake 4.4, Python 3.13.14,
OpenUSD 26.08 and deterministic Jolt 5.5.0, Release with the dynamic MSVC
runtime. Shared installed packages are the compatible prefix from the
[package report](2026-10-18-phase8-package-compatibility.md) for the first
driver run. A second run rebuilt shared source `54a3406` (the CI-pinned
revision) into a fresh prefix with the same installed deterministic Jolt SDK;
the importer/schema were rebuilt to refresh the MMD build stamp to `a34b3b5`.

| Check | Result |
| --- | --- |
| Reader suite, representative fixtures and checker controls | 7/7 CTests pass |
| Execution with both spring options OFF | 7/7 CTests pass |
| Execution with both spring options ON | 11/11 CTests pass |
| Fresh CI-pinned shared source, all three configurations | Same 25/25 CTests pass |
| Representative PMX set, each configuration | 5/5 inputs accepted and asserted |
| Execution replay | Identical exposed state over 120 fixed steps per input |
| Workflow validation | YAML structure, all PowerShell blocks and runtime-pin extraction pass |
| Documentation checker and its selftest | Pass |

Local MSBuild stalled at the build-system check inside the sandbox; the
successful runs used execution outside that sandbox. The documentation
selftest used a writable build directory for `TMP`/`TEMP`.

```powershell
python scripts/run_physics_ci.py --mmd-build build/windows-msvc `
  --physics-prefix $env:PHYSICS_PREFIX --usd-root $env:USD_ROOT `
  --jolt-root $env:JOLT_ROOT --build-root build/physics-ci-20261020-pinned
```

For the second run, `PHYSICS_PREFIX` is `build/physics-ci-shared-prefix`,
created using the workflow's shared-package configure/build/install commands
against the pinned checkout and the existing SDK prefixes. Local evidence is
under `build/physics-ci-20261020-verified` and
`build/physics-ci-20261020-pinned`, with shared build/install logs named
`build/physics-ci-shared-*.log`. These paths are local outputs, not committed
artifacts. The workflow publishes its equivalent evidence as the
`physics-installed-windows` Actions artifact.

## Evidence boundary

Hosted Windows Server 2022 / Visual Studio 2022 execution has not been run
from this working tree. Local Windows 11 / Visual Studio 2026 results cannot
establish that hosted configuration. P0-2 remains open until that run passes;
current status belongs to [current.md](../roadmap/current.md).
Existing source spring gaps, experimental approximation labels and same-build
replay limits remain unchanged. This lane does not establish bone coupling,
avatar L3 acceptance or cross-platform determinism.
