# Package contract

What each installed package promises a consumer: the name it is found by, the
target it links, the headers it installs, and what it needs besides. A
consumer relies on this page and on nothing else in the build tree; the
installed-consumer lane
([WORKSPACE.md §6](WORKSPACE.md#6-tests)) builds against a clean prefix to
keep it true.

Status (2026-09-15): the two Phase 0 packages exist, `mmd_inspect` installs
with the workspace since Phase 1, and `mmdModel` since Phase 2. Identities and
dependency edges are [WORKSPACE.md](WORKSPACE.md)'s; this page does not
restate them.

## `mmdPmx`

| | |
| --- | --- |
| `find_package` | `find_package(mmdPmx 0.0 CONFIG REQUIRED)` |
| Imported target | `mmdPmx::mmdPmx` (static library) |
| Headers | `include/mmdPmx/` — `Reader.h`, `Document.h`, `Diagnostic.h`, `DiagnosticList.h`, `Result.h`, `Codes.h` |
| Required packages | none: the package's config names no `find_dependency` |
| Language | C++20 (`cxx_std_20` is a usage requirement) |
| Version compatibility | `SameMinorVersion`: before 1.0 a minor version may change the API |
| Installed files | `${CMAKE_INSTALL_LIBDIR}/` (the archive) and `${CMAKE_INSTALL_LIBDIR}/cmake/mmdPmx/` — `lib`, or `lib64` on Linux distributions whose GNUInstallDirs default says so — and `include/mmdPmx/` |

The same surface is declared as `package_contract` in
[libs/mmdPmx/openstrata.library.yaml](../../libs/mmdPmx/openstrata.library.yaml),
and `ost library verify-consumer libs/mmdPmx` builds a consumer that includes
`mmdPmx/Reader.h` and names `mmd::pmx::Read` against the installed prefix
alone.

## `mmdModel`

| | |
| --- | --- |
| `find_package` | `find_package(mmdModel 0.0 CONFIG REQUIRED)` |
| Imported target | `mmdModel::mmdModel` (static library), which links `mmdPmx::mmdPmx` publicly |
| Headers | `include/mmdModel/` — `Canonicalize.h`, `CanonicalDocument.h`, `Basis.h`, `Codes.h` |
| Required packages | `mmdPmx`, found by the package's config (`find_dependency(mmdPmx CONFIG)`) unless the consumer already has the target |
| Language | C++20 (`cxx_std_20` is a usage requirement) |
| Version compatibility | `SameMinorVersion`, as `mmdPmx` |
| Installed files | `${CMAKE_INSTALL_LIBDIR}/` (the archive), `${CMAKE_INSTALL_LIBDIR}/cmake/mmdModel/`, and `include/mmdModel/` |

The same surface is declared as `package_contract` in
[libs/mmdModel/openstrata.library.yaml](../../libs/mmdModel/openstrata.library.yaml),
and `ost library verify-consumer libs/mmdModel` builds a consumer that
includes `mmdModel/Canonicalize.h` and names `mmd::Canonicalize` against a
prefix holding `mmdModel` and `mmdPmx` alone. The installed-consumer lane's
C++ consumer finds `mmdModel` only, and reads and canonicalizes every fixture
through it.

## `usdMmdFileFormat`

A plugin bundle, found by OpenUSD's plug registry rather than by CMake. It
exports no CMake package and no headers. Its `lib/` is always `lib`, whatever
`CMAKE_INSTALL_LIBDIR` is: `plugInfo.json` names the library by a path
relative to itself, and the installed bundle keeps the source bundle's shape.

| Installed path | Content |
| --- | --- |
| `lib/libUsdMmdFileFormat.{dll,dylib,so}` | the plugin library; `mmdPmx` and `mmdModel` are linked in statically |
| `plugin/resources/usdMmdFileFormat/plugInfo.json` | registration: format id and extension `pmx`, target `usd`; `LibraryPath` is relative (`../../../lib/…`) |
| `plugin/resources/usdMmdFileFormat/buildInfo.json` | build metadata ([WORKSPACE.md §4](WORKSPACE.md#4-manifests-versioning-and-build-metadata)), stamped at build time so the git commit is the one built |
| `openstrata.plugin.yaml` | the bundle manifest |

A host makes the plugin available by putting
`plugin/resources/usdMmdFileFormat` on `PXR_PLUGINPATH_NAME`, with OpenUSD
26.08's libraries on the loader path. It needs no other package at run time.

## `mmd_inspect`

An executable, found on `PATH` rather than by CMake. It exports no CMake
package and no headers.

| Installed path | Content |
| --- | --- |
| `${CMAKE_INSTALL_BINDIR}/mmd_inspect` (`.exe` on Windows) | the tool; `mmdPmx` is linked in statically, so it needs no other file and no OpenUSD at run time |

On Windows it embeds a UTF-8 `activeCodePage` manifest, so a path given on
its command line may name any directory
([TEXT_ENCODING_POLICY.md §4](../design/TEXT_ENCODING_POLICY.md#4-no-locale-anywhere)).
The installed-consumer lane runs it from the prefix over every fixture.
