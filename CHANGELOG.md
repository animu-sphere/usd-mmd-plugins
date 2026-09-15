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
Proposed stage-contract version: **1** (not yet authored by any code).

## [Unreleased]

### Added

- **Documentation baseline.** The design policy and five focused design
  contracts — stage, PMX, material, text encoding, and motion — distilled from
  the 2026-09-15 implementation policy; the workspace contract and external
  dependency policy; a capability matrix, diagnostic catalog and source-mapping
  table that state that nothing is implemented yet; the roadmap with the Phase
  0–8 sequence, the open-decision register, and the Phase 0 plan; and the
  documentation guidelines. No code.
- Apache-2.0 `LICENSE`.
