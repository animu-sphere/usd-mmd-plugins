# v0.3.0 installed-product and release verification

Date: 2026-10-31. Scope: generated fixtures and the three hosted release
targets, not representative private-avatar or realtime acceptance.

## Exact inputs and runs

- Prepared source commit: `346fe139ee71d3cc91903326b34dbb48e95e65d5`,
  merged without source changes by [PR #119](https://github.com/animu-sphere/usd-mmd-plugins/pull/119)
  as `9f4ec40cb23e318a59599a008b8e77ccb342b011`.
- [Required CI](https://github.com/animu-sphere/usd-mmd-plugins/actions/runs/41181571935):
  success, including all source cells, installed physics, parser sanitizers
  and fuzzing, docs and the aggregate `required-ci` gate.
- [Preparation dry run](https://github.com/animu-sphere/usd-mmd-plugins/actions/runs/41181593469):
  success on that prepared source commit. It published no registry packages
  and created no GitHub Release.
- Package version 0.3.0, stage-contract 2, pinned `ost` 0.23.13 and OpenUSD
  26.08; runtime and external-library identities are those in the source
  manifests and release workflow at this commit.

The final main commit must repeat the release dry run before it is tagged.
The final main/tag commit, dry-run URL, tag-run URL and published artifact
identities are recorded in the [v0.3.0 Release notes](https://github.com/animu-sphere/usd-mmd-plugins/releases/tag/v0.3.0).
This report records the earlier preparation evidence separately, so it does
not mistake a branch run for final-tag evidence.

## Results

| Target | Workspace CTest, including installed consumers | Release product/package checks |
| --- | --- | --- |
| Windows x86_64 / Python 3.13 | 53/53 passed | Passed |
| macOS arm64 / Python 3.13 | 53/53 passed | Passed |
| Linux x86_64 / Python 3.13 | 53/53 passed | Passed |

Each release cell built the root workspace and three bundles, ran its
configured build-tree and packaged verification pyramids, packaged twice
with matching archive digests, installed the aggregate product into a fresh
prefix and passed `python scripts/product_smoke.py --product <product>`.
The installed-consumer workspace test separately built C++ consumers against
a clean install, including VMD binding/evaluation and owner coupling.

The product smoke used only installed tools and explicitly selected installed
bundle paths to inspect a PMX/VMD and open the PMX. It checked the installed
importer, schema, imaging registration/loading and version stamp. It copied
the generated PMX and textures under a Japanese path, then:

1. Ran installed `mmd_export <pmx> <Japanese output>.usdz`.
2. Repeated with `--portable-paths`, requiring `model.usdc` first and every
   ZIP entry name to be ASCII.
3. Reopened each output in a fresh `ost plugin run --no-inject` process,
   without installed MMD bundle paths, using the independent
   `tools/mmdExport/tests/stage_probe.py --package` probe.
4. Confirmed no PMX file format or material schema was registered, `/Asset`
   was the default prim, composition succeeded, canonical material inputs
   remained authored and all four texture asset values resolved inside each
   package. The exporter ran its validation before writing either output.

This closes **P2-1** for the generated installed-user workflow and USDZ
portability contract. It adds no representative-model, runtime or renderer
acceptance claim.

## Artifact identity and independent checks

The preparation run's `release-dry-run` workflow artifact has digest
`sha256:54c25c7cd32e28f3034b80e1645082492fd339ec2c792002e5596e2c442809e5`.
After downloading it, every one of the 45 entries in `SHA256SUMS` was
recomputed successfully. All 18 bundle/tool archive digests in the pin table
matched both their manifests and their actual archive bytes, across exactly
three targets. The release set held 21 package/product manifests: three
bundles, three tools and one aggregate product per target.

- Preparation `SHA256SUMS` digest:
  `sha256:ce2338ed1bd07dd9b5dce5eb1440f8e591885e82b9122f480928bea60a3124d2`.
- Preparation `package-pins.json` digest:
  `sha256:c84baf0b8a566ab5ffb8f32ffd43e43fa244b3d4dbb73baa6eef3dd4e5650d2d`.
- The preparation pin table correctly says `published: false` and contains
  no invented OCI sources. Final tag-run identities are a separate table
  with `published: true`, archive digests and actual GHCR OCI sources.

## Limits

P1-C/P1-2/P1-3 remain open. This verification does not bundle the avatar
runtime or renderer, establish Bullet equivalence, accept every material/UV
channel, or demonstrate arbitrary-model fidelity or production realtime
performance. `mmdPhysicsAdapter` remains a source library outside the aggregate
bundle/tool product. CMake consumers must request minor 0.3 and rebuild.
