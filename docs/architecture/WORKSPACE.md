# Workspace contract

This document is the binding contract for how `usd-mmd-plugins` is laid out as
an OpenStrata plugin workspace: component identities, their kinds and
directories, the dependency directions between them, manifests, build modes,
and the invariants every change preserves. **A structural change that
contradicts this document changes this document first, in its own pull
request** — never through a README, a roadmap entry, or code.

Status (2026-09-15): contract adopted. Phases 0–2 exist: `mmdPmx` (the PMX
structural parser: every table of 2.0 and 2.1), `mmdModel` (the canonical
model), `mmd_inspect` (which reports on a PMX through the parser), and
`usdMmdFileFormat` (which registers `.pmx` and authors the canonical stage),
built by `ost` and by plain CMake. Every other identity below is *reserved*
until the Phase that creates it lands (Phases are
[DESIGN_POLICY.md §14](../design/DESIGN_POLICY.md#14-phases)), and its row then
records that.

The shape follows `usd-vrm-plugins`' workspace contract on purpose — the same
plugin/library split, the same manifests, the same two build modes — so that a
contributor, and `usd-avatar-runtime`, can treat the two repositories alike. It
does not copy VRM-specific identities that have no MMD reason to exist, such as
a package resolver.

## 1. Identities

### 1.1 First target

The smallest tree that delivers the first substantial release
([DESIGN_POLICY.md §14.1](../design/DESIGN_POLICY.md#141-first-substantial-release--definition-of-done)):

| Identity | Kind | Directory | Manifest | Role | Created in | Status |
| --- | --- | --- | --- | --- | --- | --- |
| `mmdPmx` | plain static CMake library | `libs/mmdPmx/` | `openstrata.library.yaml` | PMX syntax: header, text decoding, every table, structural validation, syntax diagnostics. No OpenUSD. | Phase 0 (scaffold), Phase 1 (parser) | exists — every table |
| `mmdModel` | plain static CMake library | `libs/mmdModel/` | `openstrata.library.yaml` | Canonical MMD semantics: identities, basis conversion, joint order, deform, morphs, materials, control and physics descriptions, provenance. No OpenUSD. | Phase 2 | exists — what the Phase 2 stage authors |
| `usdMmdFileFormat` | plugin bundle (`usd-fileformat`) | `plugins/usdMmdFileFormat/` | `openstrata.plugin.yaml` | `.pmx` `SdfFileFormat`: registration, read path, USD authoring, source → USD diagnostics. | Phase 0 | exists — the canonical stage |
| `mmd_inspect` | CLI executable | `tools/mmdInspect/` | `openstrata.tool.yaml` | Reports what a PMX contains, without USD. | Phase 1 | exists |

### 1.2 Later, only when their responsibility is real

Named now so the boundaries are designed for them; created only when the
condition in the last column is met. An empty architectural placeholder is not
created ahead of that.

| Identity | Kind | Directory | Role | Created when |
| --- | --- | --- | --- | --- |
| `mmdMaterial` | plain static CMake library | `libs/mmdMaterial/` | Canonical material semantics, extracted from `mmdModel` | material translation outgrows `mmdModel`, or a second consumer needs it alone ([DESIGN_POLICY.md §5.3](../design/DESIGN_POLICY.md#53-mmdmaterial--deferred)) |
| `mmdSchema` | plugin bundle (`usd-schema`) | `plugins/mmdSchema/` | Narrow applied API schemas | an API passes the admission test ([DESIGN_POLICY.md §6](../design/DESIGN_POLICY.md#6-the-schema-admission-test)) |
| `motionVmd` | plain static CMake library | `libs/motionVmd/` | VMD syntax, CP932 decoding, motion source representation; extraction-ready | Phase 7 ([MOTION_CONTRACT.md §2](../design/MOTION_CONTRACT.md#2-components-and-boundaries)) |
| `usdVmdFileFormat` | plugin bundle (`usd-fileformat`) | `plugins/usdVmdFileFormat/` | `.vmd` `SdfFileFormat` over `motionVmd` | the shared motion contract defines a directly opened motion stage |
| `vmd_inspect` | CLI executable | `tools/vmdInspect/` | Reports what a VMD contains | with `motionVmd` |
| `mmd_convert` | CLI executable | `tools/mmdConvert/` | PMX → `.usda`/`.usdc` on disk | `usdcat` over the file format proves insufficient |
| `mmdPmd` | plain static CMake library | `libs/mmdPmd/` | PMD syntax with its own CP932 policy | PMD support is decided ([DESIGN_POLICY.md §16](../design/DESIGN_POLICY.md#16-decisions-deliberately-left-flexible)) |

Naming follows `usd-vrm-plugins`: libraries and bundles are lower-camel
identities equal to their directory name; executables are `snake_case` and
live in a lower-camel directory.

## 2. Dependency directions

### 2.1 Allowed edges

```text
mmdPmx ──────────────→ (nothing in this repository; no OpenUSD)
mmdModel ────────────→ mmdPmx                          (no OpenUSD)
usdMmdFileFormat ────→ mmdModel, mmdPmx, OpenUSD
                       mmdSchema                       (only if it exists)
mmd_inspect ─────────→ mmdPmx                          (no OpenUSD)

                       (later)
mmdMaterial ─────────→ nothing in this repository; mmdModel → mmdMaterial
mmdSchema ───────────→ OpenUSD only
motionVmd ───────────→ the shared motion contract only (no OpenUSD)
usdVmdFileFormat ────→ motionVmd, OpenUSD
mmd_convert ─────────→ usdMmdFileFormat's public entry point, OpenUSD
```

`mmdModel → mmdPmx` is fixed by the canonicalization signature,
`Canonicalize(const pmx::Document&)`. Neither library links OpenUSD, which is
stricter than the implementation policy required: it keeps the parser and the
canonical model usable by a tool, a test harness or another front end with no
USD in the process
([DESIGN_POLICY.md §19](../design/DESIGN_POLICY.md#19-where-this-document-departs-from-the-implementation-policy)).

### 2.2 Forbidden edges

| Edge | Why |
| --- | --- |
| `mmdPmx → OpenUSD` | the parser exposes source facts, not USD policy |
| `mmdPmx → mmdModel`, `mmdPmx → usdMmdFileFormat` | syntax never knows its consumers |
| `mmdModel → OpenUSD`, `mmdModel → Hydra` | canonical semantics are renderer- and USD-independent |
| `motionVmd → usdMmdFileFormat`, `motionVmd → mmdModel`, `motionVmd → mmdPmx` | motion is extraction-ready and never needs a model to parse |
| `usdMmdFileFormat → hydra-toon` | the renderer consumes the stage, never the reverse |
| `usdMmdFileFormat → usd-stage-runner` | the importer has no update loop |
| any component → a physics engine | nothing is simulated ([DESIGN_POLICY.md §8](../design/DESIGN_POLICY.md#8-physics-policy)) |
| any component → OpenExec | nothing is evaluated at import |
| a bundle → a sibling's source tree | siblings are consumed as installed packages (§5) |

### 2.3 Enforcement

A rule in prose is a convention; these are gates, added with the code they
guard:

- **Graph.** Every edge is declared in the component's manifest
  (`requires.libraries`, `requires.bundles`), so `ost plugin test --workspace
  --graph-only` rejects an undeclared or reversed edge before anything builds.
- **Link line.** Each library's tests include a check that its link line holds
  only its allowed edges — in particular, that `mmdPmx` and `mmdModel` link no
  OpenUSD library.
- **Includes.** A boundary check scans each library's sources for forbidden
  includes (`pxr/`, a physics SDK, a sibling's private headers), as
  `usd-vrm-plugins`' `check_boundaries.py` scripts do.

All three run in CI from the Phase that creates the component. For `mmdPmx`
they are: the graph cell of [openstrata.ci.yaml](../../openstrata.ci.yaml);
`mmdPmx_boundaries`, which runs
[scripts/check_library_boundaries.py](../../scripts/check_library_boundaries.py)
over the library's sources, over its link line as CMake resolved it (it must
be empty), and over a test executable that links `mmdPmx` alone (it must
import no OpenUSD library — a static archive has no import table of its own,
so a forbidden edge shows up in what links it); and
`mmdPmx_boundaries_selftest`, which proves each rule still rejects what it
must. The script takes the component's name and allowed edges as arguments,
so other components reuse it rather than copying it (§7): `mmdModel_boundaries`
runs it over `libs/mmdModel` and `mmd_inspect_boundaries` over
`tools/mmdInspect`, each with `mmdPmx::mmdPmx` as the one allowed edge. Both
are added to the root build before OpenUSD is resolved, as `mmdPmx` is.

## 3. Directory layout

```text
usd-mmd-plugins/
├─ .github/workflows/          ost-source-ci.yml (generated from openstrata.ci.yaml); hand-written:
│                              docs-check.yml, parser-sanitizers.yml
├─ cmake/                      UsdMmdOpenUsd.cmake (the OpenUSD pin), UsdMmdTargets.cmake (per-target
│                              compile flags, the UTF-8 code-page manifest helper), utf8-code-page.manifest
├─ docs/                       see docs/README.md
├─ libs/
│  ├─ mmdPmx/                  include/ src/ tests/ fuzz/ cmake/ CMakeLists.txt openstrata.library.yaml
│  └─ mmdModel/                include/ src/ tests/ cmake/ CMakeLists.txt openstrata.library.yaml
├─ plugins/
│  └─ usdMmdFileFormat/
│     ├─ plugin/resources/usdMmdFileFormat/   plugInfo.json.in, buildInfo.json.in (the build writes both .json)
│     ├─ cmake/                WriteBuildInfo.cmake (the build-time buildInfo.json stamp)
│     ├─ src/                  UsdMmdFileFormat.cpp, usd/UsdMmdAuthorer.cpp
│     ├─ tests/fixtures/       the generated PMX fixtures and texture files, fixtures.json, the L5 goldens
│     ├─ CMakeLists.txt
│     └─ openstrata.plugin.yaml
├─ tools/
│  └─ mmdInspect/              src/ tests/ CMakeLists.txt openstrata.tool.yaml (the build stages bin/)
├─ tests/
│  ├─ fixtures/                generate_fixtures.py, the one author of every PMX byte
│  ├─ integration/             stage-open and Unicode-path tests
│  └─ installed_consumer/      a project consumed from outside the tree
├─ scripts/                    check_library_boundaries.py, check_installed_consumer.py, check_docs.py
├─ CMakeLists.txt  CMakePresets.json
├─ VERSION  CHANGELOG.md  LICENSE  THIRD_PARTY_NOTICES.md  README.md
├─ openstrata.toml  openstrata.ci.yaml
```

`tests/baseline/` (compact goldens) and `third_party/` (vendored code,
[DEPENDENCIES.md §4](DEPENDENCIES.md#4-third-party-code)) are created with
their first content.

A component's own unit tests live in that component's `tests/`; the root
`tests/` holds only what spans components. **Fixtures are the one exception to
where things live, and the reason is measured:** `ost` refuses a bundle
manifest's `tests.smoke` path that leaves the bundle
(`INVALID_CONFIG … escapes the bundle with '..'`), so the fixtures the
importer's verification pyramid opens are committed inside
`plugins/usdMmdFileFormat/tests/fixtures/`. Their generator stays workspace
tooling at `tests/fixtures/`, and any other component's tests write what they
need with it (`generate_fixtures.py --out <scratch>`) instead of reading the
bundle's copies.

## 4. Manifests, versioning and build metadata

- **`VERSION`** at the repository root is the single product version. The
  git tag (`vX.Y.Z`), `CHANGELOG.md`, and any manifest that must carry a
  version for a standalone build mirror it; nothing else defines one.
- **`openstrata.toml`** declares the workspace; **`openstrata.ci.yaml`** is
  the CI support matrix from which workflows are generated
  (`ost ci generate github`). Generated workflows are not hand-edited.
- Each component carries its manifest beside it: `openstrata.plugin.yaml` for
  a bundle, `openstrata.library.yaml` for a library, `openstrata.tool.yaml` for
  an executable.
- The **stage-contract version** is separate from the product version and
  changes only under
  [STAGE_CONTRACT.md §2](../design/STAGE_CONTRACT.md#2-contract-version).
- `usdMmdFileFormat` ships a deterministic `buildInfo.json`:

  ```json
  {
    "schema": 1,
    "projectVersion": "…",
    "gitCommit": "…",
    "openusdVersion": "…",
    "compiler": "…",
    "platform": "…",
    "stageContractVersion": 1
  }
  ```

  No timestamp, host name or absolute path — reproducibility-sensitive package
  metadata never contains one. It is written on every build and rewritten only
  when a value changed: `projectVersion` and `stageContractVersion` come from
  files that are configure dependencies (`VERSION`, `UsdMmdAuthorer.h`), and
  `gitCommit` is read at build time, so an installed stamp never names an
  older commit than the one built.

## 5. Build modes

The workspace builds two ways, and both are kept working:

```sh
# OpenStrata
ost plugin build   plugins/usdMmdFileFormat
ost plugin test    plugins/usdMmdFileFormat
ost plugin package plugins/usdMmdFileFormat

# Plain CMake, against any supported OpenUSD install
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/openusd
cmake --build build --config Release
ctest --test-dir build -C Release
```

Both have been run; [guides/building.md](../guides/building.md) records the
exact commands, including the presets in `CMakePresets.json`.

- The root `CMakeLists.txt` composes every component for development. It
  adds `mmdPmx` before resolving OpenUSD, so nothing the library configures
  can see pxr.
- Once a component is packaged, each bundle also builds **standalone**
  against the installed packages of its dependencies
  (`find_package(mmdPmx CONFIG REQUIRED)`). No consumer reaches into a
  sibling's source tree with an ad-hoc `add_subdirectory()`.
- **Windows:** every target compiles with `/utf-8` and `NOMINMAX`; the plugin
  follows `usd-vrm-plugins`' DLL import/export discipline; executables embed a
  UTF-8 `activeCodePage` manifest
  ([TEXT_ENCODING_POLICY.md §4](../design/TEXT_ENCODING_POLICY.md#4-no-locale-anywhere)).

What each installed package promises a consumer — its `find_package` name,
target, header root and required packages — is
[PACKAGE_CONTRACT.md](PACKAGE_CONTRACT.md).

## 6. Tests

| Layer | Where | Proves | Exists |
| --- | --- | --- | --- |
| unit | `libs/*/tests/`, `plugins/*/tests/` | each transition — bytes → document, document → canonical, canonical → USD — in isolation | `mmdPmx_unit`, `mmdModel_unit` |
| robustness | `libs/*/tests/` | the parser: every byte of the sample models overwritten, and every prefix read — no crash, no fatal diagnostic reported as recoverable, no document that breaks its invariants. The canonical model: thousands of generated documents within the parser's invariants, each canonicalized twice — no crash, the same bits both times, every promise of `CanonicalDocument.h` kept | `mmdPmx_robustness`, `mmdModel_robustness` |
| boundary | `libs/*/tests/`, `tools/*/tests/` | §2.3's link-line and include gates | `mmdPmx_boundaries`, `mmdModel_boundaries`, `mmd_inspect_boundaries` |
| tool | `tools/*/tests/` | each tool against the generated fixtures, from an ASCII and a non-ASCII directory | `mmd_inspect_fixtures` |
| fixtures | `tests/fixtures/` | the committed fixtures and texture files are exactly what the generator writes | `workspace_fixtures` |
| integration | `tests/integration/` | `Usd.Stage.Open("*.pmx")` through the registered plugin, against the [stage checklist](../design/STAGE_CONTRACT.md#14-validation-checklist) and the stage `fixtures.json` states for each fixture, and under a non-ASCII directory | `usdMmdFileFormat_stage_open`, `usdMmdFileFormat_unicode_paths`, `usdMmdFileFormat_notice_listeners` |
| pyramid | the bundle manifest's `tests:` | `ost plugin test` L0–L5, from the build tree and from the package | — (`ost`) |
| baseline | the bundle's `tests/fixtures/` | compact goldens do not change silently | the L5 goldens of `minimal.pmx` and `recoverable/unsafe-texture-paths.pmx` |
| installed consumer | `tests/installed_consumer/` | installed packages work from a clean prefix outside the repository | `workspace_installed_consumer` |
| fuzz | `libs/mmdPmx/fuzz/` | malformed input never crashes or over-reads, under ASan and UBSan | `mmdPmx_fuzz` in [parser-sanitizers.yml](../../.github/workflows/parser-sanitizers.yml), which also runs both libraries' unit and robustness tests instrumented |

Fixtures are generated by committed code, never copied from distributed
models ([DESIGN_POLICY.md §13](../design/DESIGN_POLICY.md#13-testing-policy)).
Binary fixtures are marked `binary` and USDA goldens `eol=lf` in
`.gitattributes`, for the reason `usd-vrm-plugins` records there, and so is
`fixtures.json`, which `workspace_fixtures` compares byte for byte.

## 7. Invariants

Every change preserves these; a change that cannot, changes this document
first.

1. The dependency edges are those of §2, declared in manifests and gated in CI.
2. `mmdPmx` and `mmdModel` link no OpenUSD.
3. The file format authors data only: no simulation, IK, constraint, morph or
   motion evaluation at import.
4. The same bytes produce the same stage.
5. The authored stage does not change meaning without a stage-contract bump.
6. Both build modes work, and every bundle builds against installed siblings.
7. No component keeps a private copy of a facility another component owns.
8. A capability is claimed only with a fixture behind it
   ([CAPABILITY_MATRIX.md](../reference/CAPABILITY_MATRIX.md)).
