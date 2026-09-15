# Phase 0 — workspace skeleton

Status: ⬜ not started

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

## Scope

### Workspace

- ⬜ Root `CMakeLists.txt` and `CMakePresets.json` composing every component.
- ⬜ `VERSION` (`0.0.0` until the first release is scheduled) and
  `CHANGELOG.md` mirroring it.
- ⬜ `openstrata.toml` and `openstrata.ci.yaml`.
- ⬜ `cmake/UsdMmdOpenUsd.cmake`: the exact OpenUSD pin, enforced at configure
  time for both build modes
  ([DEPENDENCIES.md §1](../architecture/DEPENDENCIES.md#1-openusd)).
- ⬜ `cmake/Dependencies.cmake`; Windows flags `/utf-8` and `NOMINMAX` on
  every target; the UTF-8 `activeCodePage` manifest helper for executables.
- ⬜ `.gitattributes` (`*.pmx binary`, `*.usda text eol=lf`) and `.gitignore`.
- ⬜ `THIRD_PARTY_NOTICES.md` (empty list, present from the start).

### `libs/mmdPmx` (scaffold)

- ⬜ `Read(std::span<const std::byte>)` that validates the signature and
  version and returns a document holding only the header.
- ⬜ The `Diagnostic` record and `Result<T>`
  ([DIAGNOSTICS.md §1](../reference/DIAGNOSTICS.md#1-the-record)).
- ⬜ `openstrata.library.yaml`, package config, unit tests.
- ⬜ A link-line check proving no OpenUSD
  ([WORKSPACE.md §2.3](../architecture/WORKSPACE.md#23-enforcement)).

### `plugins/usdMmdFileFormat` (scaffold)

- ⬜ `SdfFileFormat` registered for `.pmx` via `plugInfo.json`; reads through
  `ArGetResolver().OpenAsset()`
  ([TEXT_ENCODING_POLICY.md §4](../design/TEXT_ENCODING_POLICY.md#4-no-locale-anywhere)).
- ⬜ Authors the Phase 0 stage above; fatal diagnostics fail `Read`.
- ⬜ `buildInfo.json` with no timestamp
  ([WORKSPACE.md §4](../architecture/WORKSPACE.md#4-manifests-versioning-and-build-metadata)).
- ⬜ `openstrata.plugin.yaml` declaring `requires.libraries: mmdPmx`.

### Fixtures and tests

- ⬜ A committed fixture generator that writes `minimal.pmx` (PMX 2.0, UTF-16LE,
  empty tables) and `minimal-2.1-utf8.pmx`.
- ⬜ Stage-open test through the registered plugin, from Python.
- ⬜ A Unicode-path leg: the same open under a non-ASCII directory, plus a
  direct `Sdf.FileFormat.FindByExtension("pmx").CanRead(path)` call.
- ⬜ `tests/installed_consumer/`: a project outside the tree that
  `find_package`s the installed `mmdPmx` and opens a fixture through the
  installed plugin.

### CI and docs

- ⬜ CI generated from `openstrata.ci.yaml`: a graph cell, workspace cells on
  Windows, macOS and Linux, and the installed-consumer lane.
- ⬜ A hand-written docs check (relative links and anchors resolve), kept
  separate from generated workflows.
- ⬜ `docs/guides/building.md`, written only from commands that were run.
- ⬜ Architecture and reference pages updated to what exists; this page emptied
  and replaced by Phase 1.

## Recommended pull-request sequence

1. Workspace files, OpenUSD pin, CMake presets, `.gitattributes` — configures
   with nothing to build.
2. `mmdPmx` scaffold, `Diagnostic`, `Result<T>`, unit tests, link-line check.
3. `usdMmdFileFormat` scaffold, fixture generator, stage-open and Unicode-path
   tests.
4. Packaging and the installed-consumer lane.
5. CI generation, docs check, building guide.

## Decisions to close in Phase 0

- The exact OpenUSD release (expected: the ecosystem's current pin).
- The CMake minimum and the unit-test framework (expected: those of
  `usd-vrm-plugins`).
- The fixture generator's form: a Python script under `tests/fixtures/` or a
  small C++ writer. A writer in C++ is reusable by the fuzz corpus; a Python
  script is easier to read in review.

## Completion criteria

- `Usd.Stage.Open("minimal.pmx")` returns a stage whose default prim is
  `/Asset`, with the stage metadata above, on Windows, macOS and Linux.
- The same holds from an installed prefix outside the repository.
- `ost plugin test --workspace` and plain `ctest` both pass.
- `mmdPmx`'s link line contains no OpenUSD library, and CI would fail if it
  did.
- Every command in the building guide has been run.
