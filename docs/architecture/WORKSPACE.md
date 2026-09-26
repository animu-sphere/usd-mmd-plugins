# Workspace contract

This document is the binding contract for how `usd-mmd-plugins` is laid out as
an OpenStrata plugin workspace: component identities, their kinds and
directories, the dependency directions between them, manifests, build modes,
and the invariants every change preserves. **A structural change that
contradicts this document changes this document first, in its own pull
request** — never through a README, a roadmap entry, or code.

Status (2026-09-21): contract adopted. `mmdPmx` (the PMX structural parser:
every table of 2.0 and 2.1), `mmdModel` (the canonical model), `mmd_inspect`
(which reports on a PMX through the parser) and `usdMmdFileFormat` (which
registers `.pmx` and authors the canonical stage) exist since Phases 0–2;
`motionVmd` (the VMD reader), `mmdMotionBinding` (which binds a motion to a
model) and `vmd_inspect` (which reports on a VMD) since Phase 7; `mmdControl`,
`mmdSkeletonAdapter` and `mmdMotionAdapter` since Phase 9; `mmdSchema` (which
registers `MmdMaterialAPI`) since Phase 8. All are built by `ost` and by plain
CMake. Every other identity below is *reserved*
until the Phase that creates it lands (Phases are
[DESIGN_POLICY.md §14](../design/DESIGN_POLICY.md#14-phases)), and its row then
records that. `mmdControl`, `mmdMotionAdapter` and the edge into
`usd-motion-plugins` were reserved on 2026-09-17, when the motion architecture
was settled; `mmdSkeletonAdapter` split the skeleton side into its own narrow
edge on 2026-09-21 (§2.4,
[DESIGN_POLICY.md §20](../design/DESIGN_POLICY.md#20-alignment-with-the-usd-motion-plugins-design-policy)).
`mmd_export` replaced the reserved `mmd_usdz` and `mmd_convert` on
2026-09-26: one tool writes the imported stage out in every format, as
`usd-vrm-plugins`' `vrm_export` does, and USDZ is its first.

The shape follows `usd-vrm-plugins`' workspace contract on purpose — the same
plugin/library split, the same manifests, the same build modes — so that a
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

And the motion components Phase 7 created
([MOTION_CONTRACT.md §2](../design/MOTION_CONTRACT.md#2-components-and-boundaries)):

| Identity | Kind | Directory | Manifest | Role | Created in | Status |
| --- | --- | --- | --- | --- | --- | --- |
| `motionVmd` | plain static CMake library | `libs/motionVmd/` | `openstrata.library.yaml` | VMD syntax, CP932 decoding, the motion source representation. Model-independent: no dependency at all. VMD is MMD's format, so it stays in this repository. | Phase 7 | exists — every section |
| `mmdMotionBinding` | plain static CMake library | `libs/mmdMotionBinding/` | `openstrata.library.yaml` | Binds a `motionVmd` motion to an `mmdModel` model by source name, in the model's basis. No OpenUSD, no evaluation. | Phase 7 | exists |
| `vmd_inspect` | CLI executable | `tools/vmdInspect/` | `openstrata.tool.yaml` | Reports what a VMD contains, without a model or USD. | Phase 7 | exists |

And the evaluator Phase 9 created
([MOTION_CONTRACT.md §11](../design/MOTION_CONTRACT.md#11-evaluating-the-control-rig)):

| Identity | Kind | Directory | Manifest | Role | Created in | Status |
| --- | --- | --- | --- | --- | --- | --- |
| `mmdControl` | plain static CMake library | `libs/mmdControl/` | `openstrata.library.yaml` | MMD control evaluation: samples a bound motion's Bézier curves at an explicit time, and evaluates bone morphs, append transforms and IK chains over `mmdModel`'s control semantics into deformation-joint local transforms. No OpenUSD, no `usd-motion-plugins`. | Phase 9 | exists |
| `mmdSkeletonAdapter` | plain static CMake library | `libs/mmdSkeletonAdapter/` | `openstrata.library.yaml` | Exposes a canonical PMX skeleton as `SkeletonDescriptor`, `RetargetMap` and `SourceRestPose`; owns role-table version 2 and no retarget algorithm. | Phase 9 | exists |
| `mmdMotionAdapter` | plain static CMake library | `libs/mmdMotionAdapter/` | `openstrata.library.yaml` | Converts fully evaluated `mmdControl` output into `MotionClip`, using `mmdSkeletonAdapter` for roles and source rest; owns no target-avatar knowledge. | Phase 9 | exists |

And the schema bundle Phase 8 created
([MATERIAL_POLICY.md §4.3](../design/MATERIAL_POLICY.md#43-mmdmaterialapi)):

| Identity | Kind | Directory | Manifest | Role | Created in | Status |
| --- | --- | --- | --- | --- | --- | --- |
| `mmdSchema` | plugin bundle (`usd-schema`) | `plugins/mmdSchema/` | `openstrata.plugin.yaml` | `MmdMaterialAPI` and its tokens, with generated C++ accessors, also installed as a CMake package; no catch-all PMX schema. Its own bundle because the UsdImaging adapter and `hydra-toon` read it without the importer. It passed the admission test on 2026-09-22 for the `hydra-toon` consumer ([DESIGN_POLICY.md §6](../design/DESIGN_POLICY.md#6-the-schema-admission-test)). | Phase 8 | exists |

### 1.2 Later, only when their responsibility is real

Named now so the boundaries are designed for them; created only when the
condition in the last column is met. An empty architectural placeholder is not
created ahead of that.

| Identity | Kind | Directory | Role | Created when |
| --- | --- | --- | --- | --- |
| `mmdMaterial` | plain static CMake library | `libs/mmdMaterial/` | Canonical material semantics, extracted from `mmdModel` | material translation outgrows `mmdModel`, or a second consumer needs it alone ([DESIGN_POLICY.md §5.3](../design/DESIGN_POLICY.md#53-mmdmaterial--deferred)) |
| `mmdImaging` | plugin bundle (`usd-imaging`) | `plugins/mmdImaging/` | Exposes `MmdMaterialAPI` through UsdImaging data; no PMX parsing and no renderer-private GPU representation | Phase 8, after `MmdMaterialAPI` exists ([MATERIAL_POLICY.md §12](../design/MATERIAL_POLICY.md#12-rendering-and-integration-belong-elsewhere)) |
| `usdVmdFileFormat` | plugin bundle (`usd-fileformat`) | `plugins/usdVmdFileFormat/` | `.vmd` `SdfFileFormat` over `motionVmd` | MOT-O2 is resolved against `usd-motion-plugins`' standalone motion stage (`/Animation`) ([MOTION_CONTRACT.md §9](../design/MOTION_CONTRACT.md#9-open-questions)) |
| `mmd_export` | CLI executable | `tools/mmdExport/` | Writes the stage the importer authors out, in the format the output's extension names. First, `.usdz`: the stage materialized as a `.usdc` and packaged with the textures it names, as a standard USDZ that opens without this repository's plugins. `.usdc` and `.usda` on disk follow when `usdcat` over the file format proves insufficient, each with its own section in [PACKAGING_POLICY.md](../design/PACKAGING_POLICY.md) first. Reaches the importer through Plug at run time and links OpenUSD only. The counterpart of `usd-vrm-plugins`' `vrm_export`. | the USDZ packaging Phase begins ([DESIGN_POLICY.md §14](../design/DESIGN_POLICY.md#14-phases)) |
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
                       mmdSchema                       (since Phase 8)
mmd_inspect ─────────→ mmdPmx                          (no OpenUSD)
motionVmd ───────────→ nothing                         (no OpenUSD)
mmdMotionBinding ────→ mmdModel, motionVmd             (no OpenUSD)
vmd_inspect ─────────→ motionVmd                       (no OpenUSD)
mmdControl ──────────→ mmdMotionBinding, mmdModel      (no OpenUSD)
mmdSchema ───────────→ OpenUSD only

                       (later)
mmdMaterial ─────────→ nothing in this repository; mmdModel → mmdMaterial
mmdImaging ──────────→ mmdSchema, OpenUSD UsdImaging only
mmdSkeletonAdapter ──→ mmdModel,
                       usd-motion-plugins motionRetarget (OpenUSD foundation
                                                          types only, through it)
mmdMotionAdapter ────→ mmdControl, mmdModel, mmdSkeletonAdapter,
                       usd-motion-plugins motionCore     (OpenUSD foundation
                                                          types only, through it)
mmdMotionAdapter acceptance test
                  ────→ usd-motion-plugins motionRetarget, motionUsd
usdVmdFileFormat ────→ motionVmd, OpenUSD; usd-motion-plugins motionUsd if
                       MOT-O2 says so
mmd_export ──────────→ OpenUSD only (sdf, usd, usdUtils, usdValidation, hio);
                       usdMmdFileFormat at run time, through Plug, never linked
```

`mmd_export` writes exactly what the importer authors. It opens `.pmx`
through the registered `SdfFileFormat`, as any OpenUSD host does. So it links
neither the importer nor the parser nor the canonical model, and it cannot
read a PMX differently from the importer.

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
| `mmdImaging → mmdPmx`, `mmdModel`, `usdMmdFileFormat`, `hydra-toon` | the adapter reads the composed `MmdMaterialAPI` contract and publishes imaging data; it neither reparses source data nor owns renderer code |
| `motionVmd → usdMmdFileFormat`, `motionVmd → mmdModel`, `motionVmd → mmdPmx`, `vmd_inspect → mmdModel`, `vmd_inspect → mmdPmx` | a VMD never needs a model to parse |
| `mmdModel → motionVmd`, `mmdModel → mmdMotionBinding` | a model never knows the motions bound to it; binding is its own step ([MOTION_CONTRACT.md §8](../design/MOTION_CONTRACT.md#8-binding-a-vmd-to-a-pmx-model)) |
| `mmdMotionBinding → OpenUSD` | binding produces data a runtime consumes, not a stage |
| `mmdControl → OpenUSD`, `mmdControl → usd-motion-plugins`, `mmdModel → mmdControl`, `mmdMotionBinding → mmdControl` | evaluation produces MMD-domain transforms; the model and the binding stay data, and normalization is the adapter's alone |
| `mmdPmx`, `mmdModel`, `motionVmd`, `mmdMotionBinding`, `mmdControl` or `usdMmdFileFormat` `→ usd-motion-plugins` | only the two narrow adapter components cross into the shared motion core (§2.4), so parsing and evaluation remain independent |
| any component → `motion-connectors`, a device SDK, a network transport | live input is normalized by `motion-connectors` into the shared core, never read here |
| `usdMmdFileFormat → hydra-toon` | the renderer consumes `MmdMaterialAPI` through the USD/UsdImaging contract, never the importer or PMX |
| `usdMmdFileFormat → usd-stage-runner` | the importer has no update loop |
| `mmd_export → mmdPmx`, `mmdModel`, or a link to `usdMmdFileFormat`; `usdMmdFileFormat → mmd_export` | export is a distribution step over the authored stage. A second reading of PMX could disagree with the importer, and the file format never writes a package or a file |
| parser, canonical model or importer → `usd-physics-plugins` | the static path only preserves physics; only a future MMD runtime adapter may consume the optional shared package ([PHYSICS_INTEGRATION.md §8](../design/PHYSICS_INTEGRATION.md#8-dependency-policy)) |
| any component → Jolt, PhysX, Bullet or another physics backend | backend ownership is `usd-physics-plugins`'; even the future MMD coupling adapter depends only on the shared contract |
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
`tools/mmdInspect`, each with `mmdPmx::mmdPmx` as the one allowed edge;
`motionVmd_boundaries` over `libs/motionVmd` with none, and
`vmd_inspect_boundaries` over `tools/vmdInspect` with `motionVmd::motionVmd`,
both also refusing any `mmdPmx/` or `mmdModel/` include (`--forbid-include`);
`mmdMotionBinding_boundaries` over `libs/mmdMotionBinding` with
`mmdModel::mmdModel` and `motionVmd::motionVmd`; and `mmdControl_boundaries`
over `libs/mmdControl` with `mmdMotionBinding::mmdMotionBinding` and
`mmdModel::mmdModel`, also refusing any `motionCore/` include.
`mmdSkeletonAdapter_boundaries` runs with that library, allowing
`motionRetarget`; `mmdMotionAdapter_boundaries` allows `motionCore` and the
skeleton adapter. Those are the two narrow external adapter edges (§2.4).
The separate `mmdMotionAdapter_acceptance` executable intentionally links
`motionRetarget` and stage-level `motionUsd`; it is not the library target the
boundary test inspects.
The five OpenUSD-free libraries are added before OpenUSD is resolved; the
adapters follow it because their shared packages expose OpenUSD foundation
types and reuse the root's already-resolved targets. The binary-import part
allows only the foundation closure (`arch`, `tf`, `gf`, `js`, `trace`, `work`,
`plug`, `vt` and the private `boost`/`python` support libraries some shared
macOS runtimes attach to it); stage, schema and imaging libraries still fail
the gate.

### 2.4 Edges out of this repository

The ecosystem's dependency direction is fixed by the `usd-motion-plugins`
design policy (§19.3, §39) and restated here because this repository must keep
it:

```text
usd-avatar-runtime ─→ usd-mmd-plugins
usd-mmd-plugins ─────→ usd-motion-plugins   (motion/skeleton adapters only)
usd-mmd-plugins ─────→ usd-physics-plugins  (future MMD physics adapter only)
motion-connectors ───→ usd-motion-plugins
usd-stage-runner ────→ usd-physics-plugins
```

| Rule | Detail |
| --- | --- |
| Narrow crossings | `mmdMotionAdapter` depends on `motionCore`; `mmdSkeletonAdapter` depends on `motionRetarget`, which owns `SkeletonDescriptor`, `RetargetMap` and `SourceRestPose` ([MOTION_CONTRACT.md §10.4](../design/MOTION_CONTRACT.md#104-skeleton-and-humanoid-map)). The Phase 9 acceptance executable, not either exported target, uses `motionRetarget` and `motionUsd`. `usdVmdFileFormat` may add `motionUsd` if MOT-O2 says so. No parser, canonical model, evaluator or importer crosses. |
| Installed packages only | The edge is a `find_package` on an installed package with a declared version range, never a sibling checkout, a submodule or a vendored copy (§5, [DEPENDENCIES.md §6](DEPENDENCIES.md#6-usd-motion-plugins)). |
| Never the reverse | `usd-motion-plugins` never depends on any component here, and nothing here is designed to be moved there: VMD is MMD's format (the motion policy's §26). |
| Same OpenUSD | `motionCore`, `motionRetarget` and test-only `motionUsd` are built against the OpenUSD release this repository pins ([DEPENDENCIES.md §1](DEPENDENCIES.md#1-openusd)); a mismatch is a configure error, not a warning. |

The two adapter edges are active since `usd-motion-plugins` v0.5.0: their
manifests pin `motionCore` and `motionRetarget` artifacts by target and digest;
the skeleton adapter states `motionRetarget`'s `motionCore` artifact closure
explicitly so it can build in isolation.
The `mmdMotionAdapter` manifest additionally pins `motionUsd` for its test-only
acceptance edge; the adapter's link interface remains unchanged.
A future MMD-specific physics adapter may similarly consume
`usd-physics-plugins`; its identity and edge are added here only when the first
runtime consumer makes them concrete
([PHYSICS_INTEGRATION.md §8](../design/PHYSICS_INTEGRATION.md#8-dependency-policy)).

## 3. Directory layout

```text
usd-mmd-plugins/
├─ .github/workflows/          ost-source-ci.yml (generated from openstrata.ci.yaml); hand-written:
│                              docs-check.yml, parser-sanitizers.yml, release.yml
├─ cmake/                      UsdMmdProject, UsdMmdTargets, UsdMmdSanitizers, UsdMmdPackage,
│                              UsdMmdTesting, UsdMmdOpenUsd (.cmake: §5), utf8-code-page.manifest
├─ docs/                       see docs/README.md
├─ libs/
│  ├─ mmdPmx/                  include/ src/ tests/ fuzz/ cmake/ CMakeLists.txt openstrata.library.yaml
│  ├─ mmdModel/                include/ src/ tests/ cmake/ CMakeLists.txt openstrata.library.yaml
│  ├─ motionVmd/               include/ src/ tests/ fuzz/ cmake/ CMakeLists.txt openstrata.library.yaml;
│  │                           tools/generate_cp932_table.py, the one author of src/Cp932Table.inc
│  ├─ mmdMotionBinding/        include/ src/ tests/ cmake/ CMakeLists.txt openstrata.library.yaml
│  ├─ mmdControl/              include/ src/ tests/ cmake/ CMakeLists.txt openstrata.library.yaml
│  ├─ mmdSkeletonAdapter/      include/ src/ tests/ cmake/ CMakeLists.txt openstrata.library.yaml
│  └─ mmdMotionAdapter/        include/ src/ tests/ cmake/ CMakeLists.txt openstrata.library.yaml
├─ plugins/
│  ├─ mmdSchema/
│  │  ├─ schema/               schema.usda, the one source of MmdMaterialAPI
│  │  ├─ src/mmdSchema/        usdGenSchema's C++ output, committed
│  │  ├─ plugin/resources/mmdSchema/   plugInfo.json.in, generatedSchema.usda (the build writes plugInfo.json)
│  │  ├─ tools/                generate_schema.py, the one author of the committed usdGenSchema output
│  │  ├─ tests/                the C++ and Python schema tests; fixtures/basic.usda and its L5 golden
│  │  └─ cmake/ CMakeLists.txt openstrata.plugin.yaml
│  └─ usdMmdFileFormat/
│     ├─ plugin/resources/usdMmdFileFormat/   plugInfo.json.in, buildInfo.json.in (the build writes both .json)
│     ├─ cmake/                WriteBuildInfo.cmake (the build-time buildInfo.json stamp)
│     ├─ src/                  UsdMmdFileFormat.cpp, usd/UsdMmdAuthorer.cpp
│     ├─ tests/fixtures/       the generated PMX fixtures and texture files, fixtures.json, the L5 goldens
│     ├─ CMakeLists.txt
│     └─ openstrata.plugin.yaml
├─ tools/
│  ├─ mmdInspect/              src/ tests/ CMakeLists.txt openstrata.tool.yaml (the build stages bin/)
│  └─ vmdInspect/              src/ tests/ CMakeLists.txt openstrata.tool.yaml (the build stages bin/)
├─ tests/
│  ├─ fixtures/                generate_fixtures.py, the one author of every PMX byte;
│  │                           generate_vmd_fixtures.py, of every VMD byte (written, never committed)
│  ├─ integration/             stage-open and Unicode-path tests
│  └─ installed_consumer/      a project consumed from outside the tree
├─ scripts/                    check_library_boundaries.py, check_installed_consumer.py, check_docs.py;
│                              make_release_notes.py, stage_release.py, product_smoke.py (release.yml)
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

One CMake contract, driven three ways. The difference between them is only
who prepares the **dependency prefix** — the installed OpenUSD 26.08 and the
installed `usd-motion-plugins` packages — never what the CMake does with it:

```sh
# Plain CMake, the whole repository: the prefix is the caller's
cmake -S . -B build -DCMAKE_PREFIX_PATH=<dependency-prefix>
cmake --build build --config Release
ctest --test-dir build -C Release

# Plain CMake, one component: the same prefix, plus the installed packages
# of the component's in-repository edges
cmake -S libs/mmdMotionAdapter -B build/mmdMotionAdapter       -DCMAKE_PREFIX_PATH="<dependency-prefix>;<usd-mmd-plugins install>"

# OpenStrata: `ost` composes the prefix and configures the same files
ost build
ost plugin build   plugins/usdMmdFileFormat
ost library build  libs/mmdMotionAdapter
```

All three have been run; [guides/building.md](../guides/building.md) records
the exact commands, including the presets in `CMakePresets.json`.

The contract every `CMakeLists.txt` keeps:

| Rule | Detail |
| --- | --- |
| In-repository edges | the in-tree target when it exists, the installed package otherwise: `if(NOT TARGET mmdPmx::mmdPmx) find_package(mmdPmx CONFIG REQUIRED)`. So the same file configures composed by the root and on its own. |
| Edges out of the repository | always `find_package(<package> <version> CONFIG REQUIRED)` on an installed package. Never a sibling checkout: no `add_subdirectory()` of a path outside this repository, and no probing for one (`if(EXISTS ../usd-motion-plugins)`), so the checkout layout is never an input to the graph. |
| Who declares an edge | the component that links it, next to its target. The root `CMakeLists.txt` names components and their order, and resolves OpenUSD once as the boundary the pure libraries are added before; it lists no other external package. |
| Transitive edges | a package's `<name>Config.cmake` finds what its public link interface needs with `find_dependency()`, each behind a target check, so a consumer never lists a transitive dependency ([PACKAGE_CONTRACT.md](PACKAGE_CONTRACT.md)). |
| OpenUSD | resolved through `usdmmd_find_openusd()` ([cmake/UsdMmdOpenUsd.cmake](../../cmake/UsdMmdOpenUsd.cmake)), which runs `pxrConfig.cmake` only where OpenUSD's targets are not yet visible — the check `usd-motion-plugins`' package configs make — and holds every entry point to the pin. |
| OpenStrata | nothing in CMake knows it. `ost` writes a toolchain whose `CMAKE_PREFIX_PATH` holds the runtime, the workspace prefix and the digest-pinned external artifacts; a plain-CMake caller passes the same prefixes. |
| Build settings | per target, never per directory: `usdmmd_target_defaults()` applies the compile flags and the sanitizer instrumentation as `PRIVATE` properties, so a composed build sees no flag it did not ask for. |

The root adds the five OpenUSD-free libraries and both tools before OpenUSD is
resolved, so nothing they configure can see pxr; then the two adapters, whose
motion packages reuse the OpenUSD targets resolved there; then the bundles,
`mmdSchema` before `usdMmdFileFormat`.

The shared CMake lives in `cmake/`, one module per concern, and hides no
`add_library()`, `target_link_libraries()` or dependency declaration:

| Module | Provides |
| --- | --- |
| `UsdMmdProject.cmake` | `USDMMD_VERSION` from `VERSION`, before `project()`; `usdmmd_component()` — C++20, the single-config Release default, the component's tests option |
| `UsdMmdTargets.cmake` | `usdmmd_target_defaults()` (below), `usdmmd_use_utf8_code_page()` |
| `UsdMmdSanitizers.cmake` | `USDMMD_SANITIZERS`, `USDMMD_BUILD_FUZZERS` |
| `UsdMmdPackage.cmake` | `usdmmd_install_library()` — one package layout, one `SameMinorVersion` policy, one export for every plain library. A bundle installs its own way. |
| `UsdMmdTesting.cmake` | `usdmmd_test_executable()`, `usdmmd_add_boundary_test()` (§2.3), the test interpreter and the OpenUSD runtime environment |
| `UsdMmdOpenUsd.cmake` | `usdmmd_find_openusd()` and the pin |

- **Windows:** every target compiles with `/utf-8` and `NOMINMAX`; the plugin
  follows `usd-vrm-plugins`' DLL import/export discipline; executables embed a
  UTF-8 `activeCodePage` manifest
  ([TEXT_ENCODING_POLICY.md §4](../design/TEXT_ENCODING_POLICY.md#4-no-locale-anywhere)).
- **Staging:** the build writes the plugin library into the bundle's own
  `lib/`, its `plugInfo.json` and `buildInfo.json` into
  `plugin/resources/usdMmdFileFormat/`, and each tool into its `bin/` — inside
  the source tree, and ignored by git. `ost` 0.23.3 reads a bundle's
  registration and library, and a tool's declared directories, from the
  component directory itself, so staging into the build tree waits on
  OpenStrata ([ost report 01](../reports/ost/01-2026-09-24-v0.23.3-a-bundle-is-staged-in-its-source-tree.md)).

What each installed package promises a consumer — its `find_package` name,
target, header root and required packages — is
[PACKAGE_CONTRACT.md](PACKAGE_CONTRACT.md).

## 6. Tests

| Layer | Where | Proves | Exists |
| --- | --- | --- | --- |
| unit | `libs/*/tests/`, `plugins/*/tests/` | each transition — bytes → document, document → canonical, canonical → USD, VMD bytes → document → motion, motion and model → bound motion, bound motion and model → pose — in isolation; the schema's registration, §4.1 inventory, connectability and generated accessors, with no importer in the session | `mmdPmx_unit`, `mmdModel_unit`, `motionVmd_unit`, `mmdMotionBinding_unit`, `mmdControl_unit`, `mmdSchema_plugin`, `mmdSchema_material_api` |
| robustness | `libs/*/tests/` | the parsers: every byte of the sample models and motions overwritten, and every prefix read — no crash, no fatal diagnostic reported as recoverable, no document (or motion) that breaks its invariants. The canonical model: thousands of generated documents within the parser's invariants, each canonicalized twice — no crash, the same bits both times, every promise of `CanonicalDocument.h` kept. The evaluator: thousands of generated rigs and motions, each evaluated twice — the same bits both times | `mmdPmx_robustness`, `mmdModel_robustness`, `motionVmd_robustness`, `mmdControl_robustness` |
| boundary | `libs/*/tests/`, `tools/*/tests/` | §2.3's link-line and include gates | `mmdPmx_boundaries`, `mmdModel_boundaries`, `motionVmd_boundaries`, `mmdMotionBinding_boundaries`, `mmdControl_boundaries`, `mmd_inspect_boundaries`, `vmd_inspect_boundaries` |
| tool | `tools/*/tests/` | each tool against the generated fixtures, from an ASCII and a non-ASCII directory | `mmd_inspect_fixtures`, `vmd_inspect_fixtures` |
| fixtures | `tests/fixtures/`, `libs/motionVmd/tools/`, `plugins/mmdSchema/tools/` | the committed fixtures and texture files are exactly what the generator writes, and so are the CP932 table and the schema's usdGenSchema output | `workspace_fixtures`, `motionVmd_cp932_table`, `mmdSchema_generated` (where the runtime has usdGenSchema and the interpreter jinja2) |
| integration | `tests/integration/` | `Usd.Stage.Open("*.pmx")` through the registered plugin, against the [stage checklist](../design/STAGE_CONTRACT.md#14-validation-checklist) and the stage `fixtures.json` states for each fixture, and under a non-ASCII directory | `usdMmdFileFormat_stage_open`, `usdMmdFileFormat_unicode_paths`, `usdMmdFileFormat_notice_listeners` |
| pyramid | the bundle manifest's `tests:` | `ost plugin test` L0–L5, from the build tree and from the package | — (`ost`) |
| baseline | each bundle's `tests/fixtures/` | compact goldens do not change silently | the L5 goldens of `minimal.pmx`, `recoverable/unsafe-texture-paths.pmx` and `mmdSchema`'s `basic.usda` |
| installed consumer | `tests/installed_consumer/` | installed packages work from a clean prefix outside the repository | `workspace_installed_consumer` |
| shared-motion acceptance | `libs/mmdMotionAdapter/tests/` | VMD-derived IK motion survives `motionUsd`, poses a PMX-derived stage skeleton, and retargets through an MMD-free translation unit to a non-MMD skeleton | `mmdMotionAdapter_acceptance` |
| fuzz | `libs/mmdPmx/fuzz/`, `libs/motionVmd/fuzz/` | malformed input never crashes or over-reads, under ASan and UBSan | `mmdPmx_fuzz` and `motionVmd_fuzz` in [parser-sanitizers.yml](../../.github/workflows/parser-sanitizers.yml), which also runs every plain library's unit and robustness tests instrumented |

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
6. Every build mode of §5 works, and every component builds against installed
   siblings.
7. No component keeps a private copy of a facility another component owns.
   One exception, and its reason: `motionVmd` declares its own diagnostic
   record, `Result<T>` and diagnostic list, the same shape as `mmdPmx`'s,
   because a VMD parses without a model and §2.2 forbids it `mmdPmx`; binding
   carries its diagnostics into `mmdPmx`'s record field for field
   ([DIAGNOSTICS.md §1](../reference/DIAGNOSTICS.md#1-the-record)).
8. A capability is claimed only with a fixture behind it
   ([CAPABILITY_MATRIX.md](../reference/CAPABILITY_MATRIX.md)).
9. This repository consumes `usd-motion-plugins` and is never consumed by it
   (§2.4). Generic motion — poses, clips, sampling, retargeting, recording,
   `UsdSkelAnimation` authoring — is used from there, never re-implemented
   here; what is MMD's — VMD, CP932 names, Bézier curves, IK, append
   transforms, morphs — is never pushed there.
10. Generic physics — world construction, stepping, queries and backend
    ownership — is consumed from `usd-physics-plugins`, never re-implemented
    here or linked through a backend directly. PMX modes, filtering semantics
    and bone/body coupling remain MMD-side
    ([PHYSICS_INTEGRATION.md](../design/PHYSICS_INTEGRATION.md)).
