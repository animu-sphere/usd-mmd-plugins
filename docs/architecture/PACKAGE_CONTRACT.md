# Package contract

What each installed package promises a consumer: the name it is found by, the
target it links, the headers it installs, and what it needs besides. A
consumer relies on this page and on nothing else in the build tree; the
installed-consumer lane
([WORKSPACE.md §6](WORKSPACE.md#6-tests)) builds against a clean repository
prefix plus the explicitly pinned external motion packages to keep it true.

Status (2026-09-21): the two Phase 0 packages exist, `mmd_inspect` installs
with the workspace since Phase 1, `mmdModel` since Phase 2, `motionVmd`,
`mmdMotionBinding` and `vmd_inspect` since Phase 7, and `mmdControl`,
`mmdSkeletonAdapter` and `mmdMotionAdapter` since Phase 9. Identities and
dependency edges are [WORKSPACE.md](WORKSPACE.md)'s; this page does not
restate them.

## `mmdPmx`

| | |
| --- | --- |
| `find_package` | `find_package(mmdPmx 0.1 CONFIG REQUIRED)` |
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
| `find_package` | `find_package(mmdModel 0.1 CONFIG REQUIRED)` |
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

## `motionVmd`

| | |
| --- | --- |
| `find_package` | `find_package(motionVmd 0.1 CONFIG REQUIRED)` |
| Imported target | `motionVmd::motionVmd` (static library) |
| Headers | `include/motionVmd/` — `Reader.h`, `Document.h`, `Motion.h`, `Cp932.h`, `Diagnostic.h`, `Result.h`, `Codes.h` |
| Required packages | none: the package's config names no `find_dependency` |
| Language | C++20 (`cxx_std_20` is a usage requirement) |
| Version compatibility | `SameMinorVersion`, as `mmdPmx` |
| Installed files | `${CMAKE_INSTALL_LIBDIR}/` (the archive), `${CMAKE_INSTALL_LIBDIR}/cmake/motionVmd/`, and `include/motionVmd/` |

The same surface is declared as `package_contract` in
[libs/motionVmd/openstrata.library.yaml](../../libs/motionVmd/openstrata.library.yaml),
and `ost library verify-consumer libs/motionVmd` builds a consumer that
includes `motionVmd/Reader.h` and names `motionVmd::Read` against the
installed prefix alone.

## `mmdMotionBinding`

| | |
| --- | --- |
| `find_package` | `find_package(mmdMotionBinding 0.1 CONFIG REQUIRED)` |
| Imported target | `mmdMotionBinding::mmdMotionBinding` (static library), which links `mmdModel::mmdModel` and `motionVmd::motionVmd` publicly |
| Headers | `include/mmdMotionBinding/` — `Bind.h`, `Codes.h` |
| Required packages | `mmdModel` and `motionVmd`, found by the package's config (`find_dependency`) unless the consumer already has the targets; `mmdModel`'s finds `mmdPmx` |
| Language | C++20 (`cxx_std_20` is a usage requirement) |
| Version compatibility | `SameMinorVersion`, as `mmdPmx` |
| Installed files | `${CMAKE_INSTALL_LIBDIR}/` (the archive), `${CMAKE_INSTALL_LIBDIR}/cmake/mmdMotionBinding/`, and `include/mmdMotionBinding/` |

Nothing in the product links it: it is a package for a motion or avatar
runtime, so its manifest marks it `aggregate_member: false`. `ost library
verify-consumer libs/mmdMotionBinding` builds a consumer that includes
`mmdMotionBinding/Bind.h` and names `mmd::binding::Bind` against a prefix
holding its closure of four packages alone. The installed-consumer lane's
`vmd_probe` finds `mmdMotionBinding` only, reads every VMD fixture through it
and binds one to a PMX fixture.

## `mmdControl`

| | |
| --- | --- |
| `find_package` | `find_package(mmdControl 0.1 CONFIG REQUIRED)` |
| Imported target | `mmdControl::mmdControl` (static library), which links `mmdMotionBinding::mmdMotionBinding` and `mmdModel::mmdModel` publicly |
| Headers | `include/mmdControl/` — `Evaluator.h`, `Sample.h`, `Codes.h` |
| Required packages | `mmdMotionBinding` and `mmdModel`, found by the package's config (`find_dependency`) unless the consumer already has the targets; they find `motionVmd` and `mmdPmx` |
| Language | C++20 (`cxx_std_20` is a usage requirement) |
| Version compatibility | `SameMinorVersion`, as `mmdPmx` |
| Installed files | `${CMAKE_INSTALL_LIBDIR}/` (the archive), `${CMAKE_INSTALL_LIBDIR}/cmake/mmdControl/`, and `include/mmdControl/` |

Nothing in the product links it: like `mmdMotionBinding`, it is a package for
a motion or avatar runtime, which schedules it, so its manifest marks it
`aggregate_member: false`. `ost library verify-consumer libs/mmdControl`
builds a consumer that includes `mmdControl/Evaluator.h` and names
`mmd::control::Evaluator::Prepare` against a prefix holding its closure of
five packages alone. The installed-consumer lane's `control_probe` finds
`mmdControl` only, binds a VMD fixture to a PMX fixture and evaluates it.

## `mmdSkeletonAdapter`

| | |
| --- | --- |
| `find_package` | `find_package(mmdSkeletonAdapter 0.1 CONFIG REQUIRED)` |
| Imported target | `mmdSkeletonAdapter::mmdSkeletonAdapter` (static library), which links `mmdModel::mmdModel` and `motionRetarget::motionRetarget` publicly |
| Headers | `include/mmdSkeletonAdapter/` — `Adapter.h` |
| Required packages | `mmdModel` and released `motionRetarget >=0.5,<0.6`; the external package finds `motionCore` and the same OpenUSD foundation runtime |
| Language | C++20 (`cxx_std_20` is a usage requirement) |
| Version compatibility | `SameMinorVersion`, as `mmdPmx` |
| Installed files | `${CMAKE_INSTALL_LIBDIR}/` (the archive), `${CMAKE_INSTALL_LIBDIR}/cmake/mmdSkeletonAdapter/`, and `include/mmdSkeletonAdapter/` |

Its manifest pins `motionRetarget` and that package's `motionCore` dependency
by archive and OCI digest for each supported target, so its isolated artifact
closure is complete. The installed-consumer lane verifies the package from
outside the source tree against those external packages.

## `mmdMotionAdapter`

| | |
| --- | --- |
| `find_package` | `find_package(mmdMotionAdapter 0.1 CONFIG REQUIRED)` |
| Imported target | `mmdMotionAdapter::mmdMotionAdapter` (static library), which links `mmdControl`, `mmdModel`, `mmdSkeletonAdapter` and `motionCore` publicly |
| Headers | `include/mmdMotionAdapter/` — `Adapter.h`, `Codes.h` |
| Required packages | the three repository packages above and released `motionCore >=0.5,<0.6` |
| Language | C++20 (`cxx_std_20` is a usage requirement) |
| Version compatibility | `SameMinorVersion`, as `mmdPmx` |
| Installed files | `${CMAKE_INSTALL_LIBDIR}/` (the archive), `${CMAKE_INSTALL_LIBDIR}/cmake/mmdMotionAdapter/`, and `include/mmdMotionAdapter/` |

Its manifest pins `motionCore` per target, and pins `motionUsd` only to supply
the Phase 9 acceptance test; `motionUsd` is not in the exported target or
installed package contract. The installed-consumer lane binds a
generated VMD to a generated PMX, evaluates it, and builds a shared
`MotionClip` through both installed adapters.

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

## `vmd_inspect`

An executable, found on `PATH` rather than by CMake, like `mmd_inspect`.

| Installed path | Content |
| --- | --- |
| `${CMAKE_INSTALL_BINDIR}/vmd_inspect` (`.exe` on Windows) | the tool; `motionVmd` is linked in statically, so it needs no other file, no model library and no OpenUSD at run time |

It embeds the same UTF-8 `activeCodePage` manifest on Windows. The
installed-consumer lane runs it from the prefix over every VMD fixture.
