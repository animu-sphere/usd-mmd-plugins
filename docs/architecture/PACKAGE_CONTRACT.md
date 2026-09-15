# Package contract

What each installed package promises a consumer: the name it is found by, the
target it links, the headers it installs, and what it needs besides. A
consumer relies on this page and on nothing else in the build tree; the
installed-consumer lane
([WORKSPACE.md §6](WORKSPACE.md#6-tests)) builds against a clean prefix to
keep it true.

Status (2026-09-15): both Phase 0 packages exist. Identities and dependency
edges are [WORKSPACE.md](WORKSPACE.md)'s; this page does not restate them.

## `mmdPmx`

| | |
| --- | --- |
| `find_package` | `find_package(mmdPmx 0.0 CONFIG REQUIRED)` |
| Imported target | `mmdPmx::mmdPmx` (static library) |
| Headers | `include/mmdPmx/` — `Reader.h`, `Document.h`, `Diagnostic.h`, `Result.h`, `Codes.h` |
| Required packages | none: the package's config names no `find_dependency` |
| Language | C++20 (`cxx_std_20` is a usage requirement) |
| Version compatibility | `SameMinorVersion`: before 1.0 a minor version may change the API |
| Installed files | `lib/` (the archive), `lib/cmake/mmdPmx/`, `include/mmdPmx/` |

The same surface is declared as `package_contract` in
[libs/mmdPmx/openstrata.library.yaml](../../libs/mmdPmx/openstrata.library.yaml),
and `ost library verify-consumer libs/mmdPmx` builds a consumer that includes
`mmdPmx/Reader.h` and names `mmd::pmx::Read` against the installed prefix
alone.

## `usdMmdFileFormat`

A plugin bundle, found by OpenUSD's plug registry rather than by CMake. It
exports no CMake package and no headers.

| Installed path | Content |
| --- | --- |
| `lib/libUsdMmdFileFormat.{dll,dylib,so}` | the plugin library; `mmdPmx` is linked in statically |
| `plugin/resources/usdMmdFileFormat/plugInfo.json` | registration: format id and extension `pmx`, target `usd`; `LibraryPath` is relative (`../../../lib/…`) |
| `plugin/resources/usdMmdFileFormat/buildInfo.json` | build metadata ([WORKSPACE.md §4](WORKSPACE.md#4-manifests-versioning-and-build-metadata)) |
| `openstrata.plugin.yaml` | the bundle manifest |

A host makes the plugin available by putting
`plugin/resources/usdMmdFileFormat` on `PXR_PLUGINPATH_NAME`, with OpenUSD
26.08's libraries on the loader path. It needs no other package at run time.
