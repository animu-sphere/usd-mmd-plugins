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

## How a release is cut

1. On a branch: set `VERSION`, and every manifest and CMake fallback that
   mirrors it, including the sibling ranges the manifests require —
   `scripts/check_docs.py` fails until they agree. Move the changelog's
   `[Unreleased]` entries under `## [X.Y.Z] - YYYY-MM-DD`, write the record
   here, and give the Phases it carries their release in the
   [roadmap](../roadmap/README.md#status-at-a-glance). Merge.
2. Run [release.yml](../../.github/workflows/release.yml) by hand on `main`
   (`gh workflow run release.yml`): the dry run builds, verifies and packages
   everything a tag would, and uploads it as workflow artifacts.
3. Tag the merge commit `vX.Y.Z` and push the tag. The workflow checks the tag
   against `VERSION` and the changelog heading, repeats the dry run's lanes,
   and creates a **draft** release.
4. Read the draft and publish it. Publishing is a human decision; nothing
   publishes automatically.

On each of Windows x86_64, macOS arm64 and Linux x86_64, the workflow builds
the root tree and then the bundle, runs the bundle's verification pyramid
against the build tree and against its package, packages the bundle, both
tools and the aggregate product twice and requires the same digests, installs
the product into a fresh prefix and uses it from there alone
(`scripts/product_smoke.py`), and checks the release set against the tree's
descriptors (`scripts/stage_release.py`). Its runtime and `ost` pins mirror
[openstrata.ci.yaml](../../openstrata.ci.yaml) and are re-pinned with it.
