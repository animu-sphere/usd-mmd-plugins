# Phase 0 — workspace skeleton

Status: 🚧 in progress — implemented and verified on Windows; waiting for its
first CI run on Windows, macOS and Linux.

The repository builds, tests and packages before any feature work: an empty
but real `.pmx` file format that opens a stage, built by both `ost` and plain
CMake, verified from an installed prefix, and gated in CI. Nothing here parses
PMX beyond recognizing it.

## Outcome

```text
Usd.Stage.Open("minimal.pmx")
    → usdMmdFileFormat (registered for .pmx)
    → mmdPmx reads the header only
    → a stage with defaultPrim = "Asset", upAxis = "Y", metersPerUnit = 1,
      /Asset.customData["mmd:stageContractVersion"] = 1
```

What exists is recorded where it belongs: the components and their gates in
[WORKSPACE.md](../architecture/WORKSPACE.md), the closed decisions (OpenUSD
26.08, CMake 3.22, no unit-test framework, `ost` 0.22.10) in
[DEPENDENCIES.md](../architecture/DEPENDENCIES.md), the packages in
[PACKAGE_CONTRACT.md](../architecture/PACKAGE_CONTRACT.md), the capabilities in
[CAPABILITY_MATRIX.md](../reference/CAPABILITY_MATRIX.md), the commands in
[guides/building.md](../guides/building.md), and the shipped scope in the
[changelog](../../CHANGELOG.md).

## What remains

- 🚧 The first run of `ost-source-ci.yml` and `docs-check.yml`, green on every
  cell: the graph cell, the three workspace cells (whose CTest suite includes
  the installed-consumer lane), and the three standalone bundle cells.
  Nothing has run on macOS or Linux yet, so a platform difference found there
  is fixed as part of this Phase.
- ⬜ Once CI is green: this page emptied and replaced by the Phase 1 plan, and
  the roadmap's status table updated.

## Completion criteria

- `Usd.Stage.Open("minimal.pmx")` returns a stage whose default prim is
  `/Asset`, with the stage metadata above, on Windows, macOS and Linux.
  *Windows: met. macOS, Linux: CI.*
- The same holds from an installed prefix outside the repository.
  *Windows: met (`workspace_installed_consumer`, `--from-package`).*
- `ost plugin test --workspace` and plain `ctest` both pass. *Windows: met.*
- `mmdPmx`'s link line contains no OpenUSD library, and CI would fail if it
  did. *Met: `mmdPmx_boundaries` and its self-test run in every workspace
  cell.*
- Every command in the building guide has been run. *Met, on Windows.*
