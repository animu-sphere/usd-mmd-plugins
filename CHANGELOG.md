# Changelog

All notable changes to `usd-mmd-plugins` are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). Once it exists, the
release version is the single value in the repository-root `VERSION` file; the
git tag (`vX.Y.Z`) and this changelog mirror it.

The **stage-contract version** is tracked separately from the package version:
it changes only when the downstream interpretation of the authored stage
changes incompatibly
([docs/design/STAGE_CONTRACT.md §2](docs/design/STAGE_CONTRACT.md#2-contract-version)).
Stage-contract version: **1**, authored since the Phase 0 importer.

## [Unreleased]

### Added

- **Phase 0 workspace skeleton.** A root CMake workspace (`VERSION` 0.0.0,
  `CMakePresets.json`, `openstrata.toml`) that builds with `ost` and with
  plain CMake, pinned to OpenUSD 26.08 at configure time.
- **`mmdPmx`**, a plain static library with no OpenUSD dependency: the
  diagnostic record, `Result<T>`, and `mmd::pmx::Read`, which validates the
  PMX signature, version and globals and returns the header. Seven diagnostic
  codes are emitted. A boundary check fails the build's tests if its sources,
  its link line or anything linking it reaches OpenUSD.
- **`usdMmdFileFormat`**, the `.pmx` `SdfFileFormat`, reading through `Ar`:
  `Usd.Stage.Open` on a PMX authors `/Asset` (an `Xform`, `kind =
  component`) as the default prim, Y-up, meters, with
  `mmd:stageContractVersion`, `mmd:sourceFormat`, `mmd:sourceVersion` and any
  recoverable diagnostics in its `customData`. A fatal diagnostic fails the
  open with an error naming its code.
- A committed fixture generator and six generated PMX fixtures (PMX 2.0
  UTF-16LE, PMX 2.1 UTF-8, unknown globals, three malformed headers); stage-open
  and Unicode-path integration tests; the installed-consumer lane; a docs and
  version-mirror check.
- CI: `ost-source-ci.yml` generated from `openstrata.ci.yaml` (a graph cell,
  workspace and standalone-bundle cells on Windows, macOS and Linux), and a
  hand-written `docs-check.yml`.
- Documentation: the building guide and the package contract; the
  architecture, reference and roadmap pages updated to what exists.

- **Documentation baseline.** The design policy and five focused design
  contracts — stage, PMX, material, text encoding, and motion — distilled from
  the 2026-09-15 implementation policy; the workspace contract and external
  dependency policy; a capability matrix, diagnostic catalog and source-mapping
  table that state that nothing is implemented yet; the roadmap with the Phase
  0–8 sequence, the open-decision register, and the Phase 0 plan; and the
  documentation guidelines. No code.
- Apache-2.0 `LICENSE`.
