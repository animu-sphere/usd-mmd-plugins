# Release records

Each released version gets one record here: what it set out to do, what it
shipped, what it is compatible with, and what it does not do. A record is
history — once its version is released it is not rewritten; a later finding
goes into a new record or a dated [report](../reports/). Incomplete work lives
in the [roadmap](../roadmap/README.md), and which release carries a Phase is
stated only in its
[status table](../roadmap/README.md#status-at-a-glance).

| Version | Record | Theme |
| --- | --- | --- |
| v0.1.0 | [v0.1.0.md](v0.1.0.md) | The first release: PMX import as the canonical stage, with morphs, rig and physics preserved, and VMD motion read and bound — Phases 0–7 |
| v0.2.0 | [v0.2.0.md](v0.2.0.md) | `MmdMaterialAPI` and its Hydra view (stage-contract 2), MMD motion evaluated and handed to the shared motion core, and USDZ export — this repository's part of Phases 8, 9 and 10 |

## How a release is cut

1. On a branch: set `VERSION`, and every manifest that mirrors it (CMake
   reads it directly), including the sibling ranges the manifests require —
   `scripts/check_docs.py` fails until they agree. Move the changelog's
   `[Unreleased]` entries under `## [X.Y.Z] - YYYY-MM-DD`, write the record
   here, and give the Phases it carries their release in the
   [roadmap](../roadmap/README.md#status-at-a-glance). Merge.
2. Run [release.yml](../../.github/workflows/release.yml) by hand on `main`
   (`gh workflow run release.yml`): the dry run builds, verifies and packages
   everything a tag would, and uploads it as workflow artifacts.
3. Tag the merge commit `vX.Y.Z` and push the tag. The workflow checks the tag
   against `VERSION` and the changelog heading, repeats the dry run's lanes,
   pushes the bundle and tool packages to GHCR, and creates a **draft**
   release carrying their pin table.
4. Read the draft and publish it. Publishing the release is a human decision;
   the packages are already in the registry, pinned by digest, when it is
   made.

On each of Windows x86_64, macOS arm64 and Linux x86_64, the workflow builds
the root tree and then the bundles, runs each bundle's verification pyramid
against the build tree and against its package, packages the bundles, the
tools and the aggregate product twice and requires the same digests, installs
the product into a fresh prefix and uses it from there alone
(`scripts/product_smoke.py`), and checks the release set against the tree's
descriptors (`scripts/stage_release.py`). Its runtime and `ost` pins mirror
[openstrata.ci.yaml](../../openstrata.ci.yaml) and are re-pinned with it.

Every bundle and tool package, on every target, is pushed to the one public
GHCR repository `ghcr.io/animu-sphere/usd-mmd-plugins`, tagged
`<name>-<version>-<target>`, as usd-motion-plugins publishes its own. The
release carries `package-pins.json`, and its notes the same table: per
package and target, the archive digest a consumer names as `artifact` and the
OCI digest to pull it from (`scripts/make_package_pins.py`). The aggregate
product is a release asset only. v0.2.0's packages were pushed after its
release, from its published assets unchanged, and its pin table was added to
the release then.
