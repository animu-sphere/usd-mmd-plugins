# usd-mmd-plugins {tag}

OpenUSD plugins for MikuMikuDance assets. `usdMmdFileFormat` opens `.pmx`
models as USD stages; `mmd_inspect` and `vmd_inspect` report what a PMX or VMD
file contains, without USD. The plain libraries — `mmdPmx`, `mmdModel`,
`motionVmd`, `mmdMotionBinding`, `mmdControl` — ship inside the bundle and
tools that link them, and build from the source archive as installable CMake
packages.

- **Stage-contract version:** {stage_contract}
  ([STAGE_CONTRACT.md](https://github.com/animu-sphere/usd-mmd-plugins/blob/{tag}/docs/design/STAGE_CONTRACT.md))
- **OpenUSD:** 26.08, exactly
  ([DEPENDENCIES.md](https://github.com/animu-sphere/usd-mmd-plugins/blob/{tag}/docs/architecture/DEPENDENCIES.md))
- **Capability matrix:** [CAPABILITY_MATRIX.md](https://github.com/animu-sphere/usd-mmd-plugins/blob/{tag}/docs/reference/CAPABILITY_MATRIX.md)
- **Release record:** [docs/releases/{tag}.md](https://github.com/animu-sphere/usd-mmd-plugins/blob/{tag}/docs/releases/{tag}.md)

{changelog}

## Artifacts

`<target>` is `cy2026-windows-x86_64-py313-usd`, `cy2026-macos-arm64-py313-usd`
or `cy2026-linux-x86_64-py313-usd`.

| Artifact | Contents |
| --- | --- |
| `usd-mmd-plugins-{version}-<target>-plugin-product.tar.zst` | the product: the bundle and both tools, with their manifests and checksums — the one to install |
| `usdMmdFileFormat-{version}-<target>.tar.zst` | the `.pmx` file-format bundle alone |
| `mmd_inspect-{version}-<target>.tar.zst` · `vmd_inspect-{version}-<target>.tar.zst` | each tool alone |
| `<name>-{version}-<target>.manifest.json` | the OpenStrata manifest of each archive above |
| `usd-mmd-plugins-{version}-src.tar.gz` | the source at this tag |
| `SHA256SUMS` | the SHA-256 of every file above |

Install the product with `ost plugin product install --prefix <new directory>
<product archive>`, then source the prefix's `activate.sh` or `activate.ps1`.
On each target, before this draft was assembled, the bundle's verification
pyramid passed against its package, the product was installed into a fresh
prefix and used from it alone, and packaging was repeated to the same digests.

## SHA-256 checksums

```text
{checksums}
```
