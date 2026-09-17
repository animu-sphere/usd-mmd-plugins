# usd-mmd-plugins — design policy

> Status: **accepted** as the project's design policy, 2026-09-15. Phases 0–6
> are implemented — the workspace skeleton, the PMX structural parser, the
> canonical stage, the material triad, morphs, control semantics and physics
> preservation; every other behavior
> described here is intended, and
> [reference/CAPABILITY_MATRIX.md](../reference/CAPABILITY_MATRIX.md) is the
> only document that says what is implemented.
>
> This is the canonical, long-form policy: why the project is shaped the way it
> is, where its boundaries are, and the order it is built in. It is distilled
> from the 2026-09-15 implementation policy. Five focused documents own the
> detail of one area each, and **on its own area the focused document wins**:
>
> | Area | Owning document |
> | --- | --- |
> | The authored USD stage | [STAGE_CONTRACT.md](STAGE_CONTRACT.md) |
> | PMX source → canonical semantics | [PMX_CONTRACT.md](PMX_CONTRACT.md) |
> | Materials and their realizations | [MATERIAL_POLICY.md](MATERIAL_POLICY.md) |
> | Text, names, identifiers and paths | [TEXT_ENCODING_POLICY.md](TEXT_ENCODING_POLICY.md) |
> | The MMD motion boundary | [MOTION_CONTRACT.md](MOTION_CONTRACT.md) |
> | Component identities and dependency edges | [architecture/WORKSPACE.md](../architecture/WORKSPACE.md) |
>
> Section numbers are stable so other documents can cite them ("design policy
> §7"). A later revision may add subsections; a numbered section never changes
> meaning.

---

## 1. Purpose

`usd-mmd-plugins` makes MikuMikuDance (MMD) assets usable as ordinary OpenUSD
assets, while keeping MMD-specific parsing and semantics isolated from
renderers, animation runtimes and avatar applications.

The whole policy reduces to one rule:

> **Parse MMD, normalize it once, author conventional OpenUSD, and keep runtime
> evaluation and rendering outside the file-format importer.**

The first implementation target is **PMX model import**. VMD motion is a
separate concern behind a motion boundary (§9) that is compatible with the
future `usd-motion-plugins` split.

The project is the MMD sibling of
[`usd-vrm-plugins`](https://github.com/animu-sphere/usd-vrm-plugins), shaped so
that both can later be composed by `usd-avatar-runtime` without special-case
integration. It is not an MMD application, an MMD editor, or a renderer.

## 2. Design principles

### 2.1 OpenUSD first

Standard schemas carry everything they can carry faithfully enough:
`UsdGeomXform`, `UsdGeomScope`, `UsdGeomMesh`, `UsdGeomSubset`, `UsdSkelRoot`,
`UsdSkelSkeleton`, `UsdSkelAnimation`, `UsdSkelBlendShape`,
`UsdSkelBindingAPI`, `UsdShadeMaterial`, `UsdShadeShader`, `UsdShadeNodeGraph`,
`UsdShadeMaterialBindingAPI`, and `UsdPhysics` where the semantics match (§8).

An MMD-specific schema or property is introduced only for a semantic that no
standard schema represents cleanly. Mesh, skeleton, material, animation and
blend-shape concepts are **never** duplicated in an MMD schema.

### 2.2 The static importer boundary

The PMX `SdfFileFormat` reads the source, validates it, normalizes it into a
canonical in-memory model, and authors a deterministic stage. It never
simulates physics, solves IK, evaluates bone constraints or morph logic,
performs toon shading, runs an update loop, plays VMD, or resolves application
behavior. That is the boundary `usd-vrm-plugins` draws, and for the same
reason: an importer that only authors data can be swapped out, or have its
runtime swapped out, without either side changing.

### 2.3 A renderer-independent stage

The stage must stay meaningful in a USD tool that has never heard of MMD. It
carries, in this order of priority:

1. generic geometry and rigging;
2. generic material realizations (`UsdPreviewSurface`, MaterialX `gltf_pbr`);
3. MMD-native semantics where nothing generic can hold them.

An MMD-aware renderer such as a future `hydra-toon` reads level 3 as well; a
generic consumer ignores it and still sees a correct, bound, skinned model
(§11).

### 2.4 Reader first

The scope is source → USD. USD → PMX/VMD writing is deferred until a reliable
canonical model and a test corpus exist, because authoring PMX forces
preservation decisions that should not be frozen early.

### 2.5 Determinism over cleverness

The same source bytes and the same importer version author the same stage, byte
for byte. Nothing depends on the process locale, the filesystem encoding, the
host code page, the time of day, or hash-map iteration order. Concretely:
stable prim and material naming, stable joint ordering, one explicit coordinate
conversion, explicit encoding diagnostics, a stamped stage-contract version,
and deterministic texture asset paths.

### 2.6 Format semantics and runtime semantics are different layers

A source concept that is part of the file contract is preserved. A source
*quirk* does not leak into consumers that have no reason to know it.

```text
PMX bytes ─→ PMX syntax parser ─→ canonical MMD document ─→ USD authorer
          ─→ OpenUSD scene contract ─→ renderer / motion runtime / avatar runtime / tools
```

Each arrow is a function with its own tests (§4).

## 3. Relationship to usd-vrm-plugins

Contributors who know `usd-vrm-plugins` should find this repository familiar.
What carries over is **architecture**, not internals:

- a repository-level CMake workspace, with `plugins/` for OpenUSD plugin
  bundles and `libs/` for plain C++ libraries;
- parser and container code separate from USD authoring;
- components that build, test and package independently, each with its
  OpenStrata manifest beside it;
- plain CMake support alongside `ost`;
- deterministic fixtures, stage-open tests and an installed-consumer lane;
- the same documentation taxonomy (architecture · design · guides · reference ·
  roadmap · releases · reports · contributing);
- the `/Asset`-rooted stage, with `geo` · `mtl` · `skel` · `rig` scopes
  ([STAGE_CONTRACT.md §4](STAGE_CONTRACT.md#4-prim-hierarchy));
- material-local `preview` / `mtlx` realization graphs
  ([MATERIAL_POLICY.md §3](MATERIAL_POLICY.md#3-hierarchy));
- no private copy of a generic facility inside an importer or adapter.

What deliberately does **not** carry over:

- **No package resolver.** VRM needs an `ArPackageResolver` because a `.vrm`
  embeds its textures in a GLB container. A PMX references external files, so
  standard `ArResolver` behavior is sufficient
  ([TEXT_ENCODING_POLICY.md §7](TEXT_ENCODING_POLICY.md#7-texture-paths)).
- **No schema bundle by default.** `vrmSchema` exists because VRM has typed
  semantics consumers read. `mmdSchema` must pass its own admission test (§6).
- **No humanoid assumption.** MMD bone names are a community convention, not a
  humanoid specification; humanoid mapping is a runtime or retarget concern
  (§9.3), not an importer one.

## 4. Pipeline and testable transitions

```text
bytes ──Read──→ pmx::Document ──Canonicalize──→ mmd::CanonicalDocument ──Author──→ USD layer
```

The three transitions are separate functions in separate components, so each
is testable without the next:

```cpp
// mmdPmx
namespace mmd::pmx {
    struct Document;
    Result<Document> Read(std::span<const std::byte> bytes);
    Result<Document> ReadFile(const std::filesystem::path& path);
}

// mmdModel
namespace mmd {
    struct CanonicalDocument;
    Result<CanonicalDocument> Canonicalize(const pmx::Document&);
}

// usdMmdFileFormat (internal)
class UsdMmdAuthorer {
public:
    bool WriteToString(const mmd::CanonicalDocument&,
                       std::string* outUsda,
                       std::vector<Diagnostic>* diagnostics) const;
};
```

`Result<T>` carries either a value and its recoverable diagnostics, or the
fatal diagnostic that prevented one. Both it and the diagnostic record exist
since the Phase 0 scaffold and are described in
[reference/DIAGNOSTICS.md §1](../reference/DIAGNOSTICS.md#1-the-record).
`ReadFile` exists for callers that hold a path, `mmd_inspect` first; the
importer never calls it — it reads through `Ar` and hands the parser bytes
([TEXT_ENCODING_POLICY.md §4](TEXT_ENCODING_POLICY.md#4-no-locale-anywhere)).

Signatures above are the intended public boundary, not a frozen ABI; §15 lists
what *is* frozen.

## 5. Component responsibilities

Identities, directories, kinds and permitted edges are fixed in
[WORKSPACE.md](../architecture/WORKSPACE.md). This section says what each one
is *for*.

### 5.1 `mmdPmx` — the PMX syntax parser

A plain C++ library with **no OpenUSD dependency**. It owns the header, version
validation, index widths, text decoding, and every PMX table: vertices, faces,
textures, materials, bones, morphs, display frames, rigid bodies, joints and
(2.1) soft bodies, plus structural validation and syntax-level diagnostics.

It exposes **source facts, not USD policy**: a `Bone` has a `name` and an
`englishName`, never a `pxr::SdfPath`. Values stay in the source's own
coordinate basis and units; conversion is `mmdModel`'s job (§5.2).

### 5.2 `mmdModel` — canonical MMD semantics

The semantic hand-off between parsing and authoring. It owns canonical names
and stable identifiers, mesh partitions, the canonical joint order, normalized
deform data, morph descriptions, material semantics, bone-control
descriptions, physics descriptions, provenance — and **the single
source-to-USD coordinate conversion**
([STAGE_CONTRACT.md §6](STAGE_CONTRACT.md#6-coordinate-conversion)). After
canonicalization every value is in the USD basis and in meters; nothing
downstream flips an axis.

It makes PMX version differences disappear where that is reasonable. It depends
on `mmdPmx` (canonicalization takes a `pmx::Document`) and, like `mmdPmx`, on
**no OpenUSD** — a non-USD tool must be able to consume canonical MMD.

```cpp
namespace mmd {
struct CanonicalDocument {
    Metadata metadata;                  // provenance of the model
    std::vector<Texture> textures;      // verbatim and normalized paths
    Mesh mesh;                          // contract v1 has one
    std::vector<Material> materials;
    Skeleton skeleton;                  // canonical joint order
    // Phases 4-6: morphs, control semantics, rigid bodies and joints.
};
}
```

That is the document as of Phase 2
([libs/mmdModel/include/mmdModel/CanonicalDocument.h](../../libs/mmdModel/include/mmdModel/CanonicalDocument.h));
each later Phase adds what it authors.

### 5.3 `mmdMaterial` — deferred

Material semantics start inside `mmdModel`. They move to their own library only
when the boundary is demonstrated — when material translation is large enough,
or a second consumer (a renderer's test harness, say) needs it without the rest
of the canonical model. It must never become a renderer.

### 5.4 `usdMmdFileFormat` — the importer

The OpenUSD `SdfFileFormat` bundle. It owns `.pmx` registration and
identification, the read path, stage metadata, prim layout, material
realization, skeleton binding, blend-shape authoring, the application of any
approved MMD schema, and the diagnostics that cross the source → USD boundary.
It does not own VMD playback, physics, IK, toon rendering, or anything with a
clock.

### 5.5 Tools

`mmd_inspect` reports what a PMX contains — header, counts, names, flags,
diagnostics — without USD, so a parser question can be answered without an
importer in the way. `mmd_convert` (PMX → `.usda`/`.usdc` on disk) follows only
when `usdcat` over the file format proves insufficient.

## 6. The schema admission test

`mmdSchema` is not created because `vrmSchema` exists. A candidate API schema
(`MmdModelAPI`, `MmdMaterialAPI`, `MmdBoneAPI`, `MmdPhysicsAPI`, …) is admitted
only if it passes:

> **Would a consumer benefit from reading this value from USD even if it never
> sees the original PMX?**

If not, the value stays as provenance, custom data, or is not authored.

The v1 stage therefore has **no custom typed prim schemas and no applied API
schemas**. Semantics that a consumer evaluates are authored as namespaced
custom attributes (`mmd:material:*`, `mmd:morph:*`, …) whose names a later
applied API schema can declare verbatim, so admitting a schema does not change
the authored stage
([STAGE_CONTRACT.md §3](STAGE_CONTRACT.md#3-authoring-conventions)). An API is
proposed when a real consumer asks for one, and the proposal cites that
consumer. `mmdSchema` must not become a dump of the PMX binary structure.

## 7. Deformation, morph and bone-control policy

The detail is in [PMX_CONTRACT.md](PMX_CONTRACT.md) and
[STAGE_CONTRACT.md](STAGE_CONTRACT.md); the principles are fixed here.

### 7.1 Conventional USD first, source semantics preserved beside it

Where standard USD represents a PMX concept exactly, it is authored as standard
USD. Where it does not, the importer authors the closest standard
representation, **preserves the source parameters beside it**, and says so with
a diagnostic. It never claims exactness it does not have.

- BDEF1/2/4 → `UsdSkel` joint indices and weights. Exact.
- SDEF → the same linear-blend weights, plus the C/R0/R1 parameters preserved
  per vertex, plus `MMD_SKEL_SDEF_APPROXIMATED`. A generic consumer gets LBS; an
  SDEF-aware runtime or renderer gets what it needs.
- QDEF → four linear-blend influences, the deform type preserved per vertex,
  and no support claim until a consumer that performs dual-quaternion skinning
  has been verified.

### 7.2 Morphs are not all blend shapes

Only vertex morphs become `UsdSkelBlendShape`. Group, bone, UV, additional-UV,
material, flip and impulse morphs are preserved as **declarative** semantics —
never baked into the rest skeleton, never exploded into precomputed geometry
or material variants, never executed. Evaluating them belongs to a runtime.

### 7.3 The deformation skeleton is not the control rig

`UsdSkelSkeleton` is the **deformation** skeleton. PMX control semantics —
append (inherit) rotation and translation, fixed axis, local axis, external
parent, IK chains, transform order — are real and are preserved, but separately
(`/Asset/rig`), and without duplicating bones into a second hierarchy. IK is
imported declaratively and never solved at import. Where OpenUSD gains a
standard constraint representation that matches, it is preferred.

## 8. Physics policy

PMX rigid bodies and joints are preserved; nothing is simulated.

1. Standard `UsdPhysics` schemas are used where the semantics match, evaluated
   against the OpenUSD release in use when Phase 6 begins — 26.08; which ones
   match is fixed in
   [STAGE_CONTRACT.md §13](STAGE_CONTRACT.md#13-physics).
2. PMX values with no standard counterpart are preserved as MMD semantics.
3. No physics engine (Bullet or otherwise) is ever a parser or importer
   dependency.
4. Opening a PMX stage never steps a simulation.

Execution belongs to `usd-stage-runner` or another runtime.

## 9. Motion boundary

VMD belongs to the broader motion architecture, not to the PMX importer. The
boundary is fixed in [MOTION_CONTRACT.md](MOTION_CONTRACT.md); its principles:

### 9.1 Extraction-ready

`motionVmd` may live here first so work can progress, but it is written as a
plain library that can leave: it depends on a shared motion contract, never on
`usdMmdFileFormat` and never on `mmdModel`.

### 9.2 A VMD opens without a model

A future `usdVmdFileFormat` may open a `.vmd` directly. Its output is an
avatar-independent motion representation or `UsdSkelAnimation`, per the shared
motion contract. Parsing a VMD never requires a PMX.

### 9.3 Binding motion to a model is a separate operation

```text
VMD ─→ canonical motion ─→ MMD semantic mapping ─→ retarget ─→ target UsdSkelSkeleton
```

MMD bone naming is a convention, not a skeleton specification, so binding a
clip to a skeleton is its own step with its own diagnostics, never an implicit
side effect of loading either file.

## 10. Parser strategy

The parser is small, owned by the project, and defensive:

- every read is bounded; every size computation is overflow-checked;
- index widths and signedness are validated against the header;
- little-endian is explicit, never assumed from the host;
- a declared count is accepted only if the remaining bytes can hold that many
  records of the minimum record size — which bounds allocation without an
  arbitrary cap ([PMX_CONTRACT.md §2](PMX_CONTRACT.md#2-reading-rules));
- malformed text is detected, never passed through;
- no unchecked pointer walking;
- the API takes a byte span, so it is directly fuzzable;
- no OpenUSD.

A third-party PMX parser may be adopted only after checking license
compatibility, maintenance status, malformed-input safety, encoding behavior,
PMX 2.0/2.1 coverage, dependency weight, and whether it preserves source facts
without imposing application or renderer semantics
([architecture/DEPENDENCIES.md §4](../architecture/DEPENDENCIES.md#4-third-party-code)).
PMX is a bounded binary format, so the default is a purpose-built parser.

## 11. Interoperability levels

A successfully imported PMX serves several consumers at once, each ignoring
what it does not understand:

| Level | Consumer | Reads |
| --- | --- | --- |
| 1 | Any USD tool | mesh, materials, skeleton, skinning, blend shapes, textures |
| 2 | A MaterialX-aware renderer | the `gltf_pbr` realization |
| 3 | An MMD-aware Hydra renderer (`hydra-toon`) | toon ramp, sphere map, outline, MMD alpha and draw behavior |
| 4 | An avatar runtime (`usd-avatar-runtime`) | VMD, IK, bone control, morph composition, physics, live motion |

This layering is preferred to making every USD consumer understand MMD.

## 12. Diagnostics

Every diagnostic has a stable code (`MMD_<FAMILY>_<EVENT>`), a severity, a
message, a source location (byte offset or table index) where one exists, and
a recoverable flag. Tests assert codes, never prose. The families, the record,
and the catalog are in
[reference/DIAGNOSTICS.md](../reference/DIAGNOSTICS.md).

## 13. Testing policy

A format plugin is only as trustworthy as its fixtures.

- **Parser tests** cover the minimum valid PMX, PMX 2.0 and 2.1, UTF-8 and
  UTF-16LE text, Japanese bone and texture names, every index width, every
  deform type, malformed counts, invalid indices, truncated sections, and
  invalid strings.
- **Stage tests** open fixtures through the registered plugin
  (`Usd.Stage.Open("fixture.pmx")`) and assert the contract checklist in
  [STAGE_CONTRACT.md §14](STAGE_CONTRACT.md#14-validation-checklist).
- **Golden USDA** under `tests/baseline/` covers compact, stable contracts only.
  Enormous production models are never baselined.
- **Semantic tests** assert meaning over text: joint count and paths, material
  binding, weight sums, morph offsets, name preservation, texture resolution.
- **Installed-consumer tests** build against a clean installed prefix outside
  the repository, which catches dependencies on the source tree, relative
  include paths, the build tree, or undeclared siblings.
- **Fuzzing** targets the parser's one entry point, `Read`, which every
  section reader sits behind, with ASan/UBSan on a lane of its own
  ([parser-sanitizers.yml](../../.github/workflows/parser-sanitizers.yml)).
  Every fuzzed document must keep the invariants `pmx::Document` promises. A
  deterministic cousin — every byte of the sample models overwritten and read
  back — runs in every build.

**Fixtures are synthetic.** They are generated by code committed in this
repository, so every byte has a known author and license. MMD models
distributed by their creators carry individual terms of use and are never
committed or redistributed; they may be used locally for evidence, and a report
may describe what they showed.

## 14. Phases

This repository has **one** phase sequence, written `Phase 0`–`Phase 8`, and
this section is its source of truth. A phase is not a release; which release
carries a phase is decided in the
[roadmap](../roadmap/README.md#status-at-a-glance), never here. Should a second
sequence ever appear, both get a qualifier, as they do in `usd-vrm-plugins`.

| Phase | Goal | Acceptance |
| --- | --- | --- |
| **0 — workspace skeleton** | The repository builds before feature work: root CMake, `VERSION`, OpenStrata configuration, CI, `libs/mmdPmx` and `plugins/usdMmdFileFormat` scaffolds, `.pmx` registration, installed-consumer test. | `Usd.Stage.Open("minimal.pmx")` returns a stage whose default prim is `/Asset`. |
| **1 — PMX structural parser** | Header, text, vertices, faces, textures, materials, bones, morphs, display frames, rigid bodies, joints, and 2.1 soft bodies for complete traversal. | Deterministic parser fixtures; malformed-input tests; no OpenUSD in the parser's link line. |
| **2 — canonical stage** | `/Asset`, `geo`, `mtl`, `skel`; Y-up, meters; mesh, normals, UVs, material subsets, skeleton, BDEF skinning. | Recognizable character geometry in `usdview`; skeleton and skin binding inspectable; Japanese names preserved. |
| **3 — material triad** | `UsdPreviewSurface`, MaterialX `gltf_pbr`, native MMD material semantics. | Source color, texture and alpha visible in generic renderers; toon, sphere and edge semantics recoverable. |
| **4 — morphs** | Vertex morph → `UsdSkelBlendShape`; the preservation encoding for every other morph type; stable naming. | Blend shapes drive in `usdview`; every PMX morph is recoverable from `/Asset/morph`. |
| **5 — control semantics** | Narrow MMD semantics for IK, append transforms, axes, morph composition and material morphs — schema only where §6 admits one. | A consumer can reconstruct every IK chain and append relation from the stage alone. |
| **6 — physics preservation** | Standard `UsdPhysics` where it matches; everything else preserved. | No simulation; every rigid body and joint recoverable. |
| **7 — VMD** | Extraction-ready `libs/motionVmd`, integrated with the shared motion architecture; `usdVmdFileFormat` only once direct stage-open has a contract. | Per [MOTION_CONTRACT.md](MOTION_CONTRACT.md). |
| **8 — avatar runtime composition** | Composition through OpenStrata with `usd-vrm-plugins`, `usd-motion-plugins`, `motion-connectors`, `hydra-toon` and `usd-stage-runner`, under `usd-avatar-runtime`. | The runtime, not this repository, is the avatar execution environment. |

### 14.1 First substantial release — definition of done

The first release that claims PMX import is done when:

- a PMX 2.0 and a PMX 2.1 fixture open directly with `Usd.Stage.Open`;
- the default prim is `/Asset`, and the stage is Y-up and in meters;
- visible geometry is `UsdGeomMesh`, with correct material assignment;
- `UsdPreviewSurface` works as a generic fallback, and a MaterialX `gltf_pbr`
  graph exists;
- native MMD material semantics are preserved;
- the skeleton is a valid `UsdSkelSkeleton` and BDEF skinning is correct;
- vertex morphs are available as blend shapes;
- Japanese model, material, bone and morph names survive into USD;
- Japanese texture filenames resolve;
- approximated SDEF and unverified QDEF are explicit, in diagnostics and in the
  capability matrix;
- no IK or physics runs while a file opens;
- the parser library has no OpenUSD dependency;
- the plugin builds with `ost` and with plain CMake;
- the installed-consumer lane passes;
- every capability claim is backed by a fixture.

That is Phases 0–4 plus the preservation parts of 5 and 6 that the checklist
names; the [roadmap](../roadmap/README.md) decides the version.

## 15. Decisions frozen early

These are expensive to change once consumers exist, so they are fixed now and
fixture-tested from the first meaningful release. Changing one requires a
stage-contract version bump
([STAGE_CONTRACT.md §2](STAGE_CONTRACT.md#2-contract-version)) or, for the
structural ones, a change to WORKSPACE.md first.

| # | Decision | Fixed in |
| --- | --- | --- |
| 1 | `/Asset` is the default prim | [STAGE_CONTRACT.md §4](STAGE_CONTRACT.md#4-prim-hierarchy) |
| 2 | The `geo` · `mtl` · `skel` · `rig` · `morph` · `physics` scope vocabulary | [STAGE_CONTRACT.md §4](STAGE_CONTRACT.md#4-prim-hierarchy) |
| 3 | Y-up, meters, and one conversion layer | [STAGE_CONTRACT.md §6](STAGE_CONTRACT.md#6-coordinate-conversion) |
| 4 | Source name and USD identifier are separate | [TEXT_ENCODING_POLICY.md §5](TEXT_ENCODING_POLICY.md#5-identity-versus-display) |
| 5 | Canonical joint ordering | [STAGE_CONTRACT.md §9](STAGE_CONTRACT.md#9-skeleton-and-skinning) |
| 6 | Material realization names `preview` and `mtlx` | [MATERIAL_POLICY.md §3](MATERIAL_POLICY.md#3-hierarchy) |
| 7 | The stage-contract version mechanism | [STAGE_CONTRACT.md §2](STAGE_CONTRACT.md#2-contract-version) |
| 8 | parser → canonical → authorer dependency direction | [WORKSPACE.md §2](../architecture/WORKSPACE.md#2-dependency-directions) |
| 9 | The static importer / runtime evaluator boundary | §2.2 |
| 10 | VMD is never hard-wired into PMX loading | §9 |

## 16. Decisions deliberately left flexible

Not frozen, and not to be frozen until at least two plausible consumers or one
real implementation demonstrate the need: custom MMD API schema names; the
physics backend; the runtime IK implementation; an SDEF GPU implementation; the
toon renderer's architecture; the final VMD retarget API; Python or JavaScript
bindings; USD export policy; PMD support.

## 17. Non-goals for the first releases

Perfect MMD toon parity; live VMD playback; Bullet physics; IK solving; MMD
editor features; PMX or VMD writing; renderer-specific shader compilation; a
general format-conversion framework; WebXR; live tracking; VRM ↔ MMD semantic
retargeting inside the PMX importer. Some of these exist elsewhere in the
ecosystem (§18); none belongs here.

## 18. Place in the ecosystem

```text
                          usd-avatar-runtime
                                 │
          ┌──────────────────────┼──────────────────────┐
   usd-motion-plugins      usd-stage-runner        hydra-toon
          └─────────────── USD scene contract ──────────┘
                                 ▲
                         usd-mmd-plugins
              ┌──────────────────┴──────────────────┐
      usdMmdFileFormat                         usdVmdFileFormat (later)
              │                                     │
          mmdModel ── mmdSchema (only if §6)    motionVmd ── shared motion contract
              │
           mmdPmx
              │
             PMX
```

Every arrow points toward a more general contract. The PMX parser knows nothing
about Hydra; USD authoring knows nothing about an update loop; the renderer
knows nothing about PMX bytes; the avatar runtime composes the pieces rather
than forcing them into one library.

| Repository | Owns |
| --- | --- |
| `usd-vrm-plugins` | VRM file semantics |
| `usd-mmd-plugins` | PMX/MMD file semantics |
| `usd-motion-plugins` | motion representation, retarget, recording, USD bridge |
| `motion-connectors` | live external inputs |
| `hydra-toon` | toon rendering, consuming the USD contract — never PMX |
| `usd-stage-runner` | the update and evaluation loop |
| `usd-avatar-runtime` | OpenStrata composition and the cross-format avatar contract |

Cross-format avatar policy — canonical humanoid semantics, a runtime expression
API, shared motion application, controller behavior, renderer selection — lives
in `usd-avatar-runtime`. `usd-mmd-plugins` stays a format adapter.

## 19. Where this document departs from the implementation policy

The 2026-09-15 implementation policy is the origin of this document. Where the
focused documents had to be more precise than it, they depart from it in the
following places, each for a stated reason. Each departure is `proposed` until
the Phase that first authors it lands with a fixture, and binding from then.

| Implementation policy | Here | Why | Status |
| --- | --- | --- | --- |
| §6.1, §6.8 — `/Asset` is a `UsdGeomXform`; `/Asset/rig/SkelRoot/Skeleton` | `/Asset` is the `UsdSkelRoot` when the model has bones; the skeleton is `/Asset/skel/Skeleton` | `UsdSkel` only skins geometry beneath a `SkelRoot`, so meshes under `/Asset/geo` would not deform under `/Asset/rig/SkelRoot`. The `skel`/`rig` split is `usd-vrm-plugins`' layout and matches §7.3's own deformation/control split ([STAGE_CONTRACT.md §4.1](STAGE_CONTRACT.md#41-why-asset-is-the-skelroot)). | binding (Phase 2) |
| §10 — a `native` child under each material | Native semantics are attributes on the `UsdShadeMaterial` itself | A child graph reads as a third realization; the material prim is where identity and semantics already live in the VRM material policy ([MATERIAL_POLICY.md §3](MATERIAL_POLICY.md#3-hierarchy)). | binding (Phases 2 and 3) |
| §27 — `customLayerData.mmdSchemaContractVersion` | `/Asset.customData.mmd:stageContractVersion` | Layer metadata is not composed, so it is lost once the asset is referenced; `/Asset` customData travels with the reference, and matches `vrm:schemaContractVersion` ([STAGE_CONTRACT.md §2](STAGE_CONTRACT.md#2-contract-version)). | binding (Phase 0) |
| §19 — `mmdModel` may or may not depend on `mmdPmx`; OpenUSD unspecified | `mmdModel → mmdPmx`; no OpenUSD in either | §26's `Canonicalize(const pmx::Document&)` settles the first; the second keeps canonical MMD usable by non-USD tools ([WORKSPACE.md §2](../architecture/WORKSPACE.md#2-dependency-directions)). | binding (Phase 2) |
| §15.2 — stable-ID precedence includes transliteration and recognized roles | Contract v1 uses the English name or an index fallback only | Both a transliteration table and a role table would become part of the stage ABI; they stay open until a consumer needs them ([TEXT_ENCODING_POLICY.md §6](TEXT_ENCODING_POLICY.md#6-stable-identifiers)). | binding (Phase 2) |
