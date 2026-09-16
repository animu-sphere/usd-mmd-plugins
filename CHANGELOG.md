# Changelog

All notable changes to `usd-mmd-plugins` are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). Once it exists, the
release version is the single value in the repository-root `VERSION` file; the
git tag (`vX.Y.Z`) and this changelog mirror it.

The **stage-contract version** is tracked separately from the package version:
it changes only when the downstream interpretation of the authored stage
changes incompatibly
([docs/design/STAGE_CONTRACT.md §2](docs/design/STAGE_CONTRACT.md#2-contract-version)).
Stage-contract version: **1**, authored since the Phase 0 importer.

## [Unreleased]

### Added

- **Phase 2 canonical stage.** `Usd.Stage.Open("model.pmx")` now authors the
  model: `/Asset` as the `UsdSkelRoot` of a model with bones (an `Xform`
  without), `/Asset/geo/Mesh` with points, reversed-winding triangles,
  normals, `primvars:st`, the additional vec4 channels and edge scale as
  `primvars:mmd:*`, and `doubleSided` when any drawn material is no-cull;
  one `UsdShadeMaterial` per PMX material under `/Asset/mtl`, bound through
  a `materialBind` subset, with its provenance, `mmd:material:doubleSided`
  and its texture slots; and `/Asset/skel/Skeleton` in canonical joint order,
  with bind and rest transforms, per-joint source names, and BDEF1/2/4
  skinning — SDEF as linear blending with C/R0/R1 preserved, QDEF
  unverified. Model names and comments join `/Asset`'s `customData`.
- **Phase 3 material triad.** Canonical materials now retain diffuse, specular,
  ambient, edge, draw flags, sphere modes and toon ramp variants. Every
  material authors VRM-like unlit `preview` and MaterialX `mtlx` graphs;
  MMD-specific semantics remain on the material prim for an MMD-aware
  renderer. Unsafe texture paths remain provenance-only, and alpha mode uses
  the source texture-slot contract without decoding image pixels.
- **Phase 4 morphs.** Every PMX morph is now a prim under `/Asset/morph`, in
  morph-table order and named by its stable identifier, carrying
  `mmd:morph:type`, `mmd:morph:panel` and its provenance. A vertex morph is a
  `UsdSkelBlendShape` with sparse `pointIndices`, listed by the mesh in
  `skel:blendShapes` / `skel:blendShapeTargets`, so a consumer that authors
  weights deforms the mesh with it; a model with no bones is no SkelRoot,
  and one with no mesh has nothing to name a blend shape, so there it is
  preserved on a typeless prim with `MMD_MORPH_NO_SKELETON`.
  Group, flip, bone, UV, material and impulse morphs are typeless prims whose
  uniform `mmd:morph:*` arrays and member relationship carry the source
  semantics — a group is never expanded, a bone morph never moves the rest
  skeleton, a material morph never edits a material, an impulse is never
  applied. A member that reaches its own morph is dropped with
  `MMD_MORPH_GROUP_CYCLE`, and a panel outside 0–4 is preserved as `other`
  with `MMD_MORPH_UNKNOWN_PANEL`. STAGE-O4 is resolved as proposed; the
  torque of an impulse morph adds the axial-vector row to the conversion
  table.
- **Phase 5 control semantics.** A model with bones now authors `/Asset/rig`.
  `/Asset/rig/Bones` carries every joint's control semantics as uniform
  `mmd:rig:*` arrays parallel to the Skeleton's joints — transform layer,
  deform-after-physics, the rotatable / translatable / visible / operable
  flags, the tail, the append source, ratio and kind, the fixed axis, the
  local axes and the external-parent key — and `/Asset/rig/ik/<bone>` one
  typeless prim per IK chain, with its effector, loop count, limit angle and
  links with their rotation limits. Every joint index is canonical; a fixed
  axis converts as an axial vector, local axes as the frame they make up, and
  link limits per axis. A relation whose bone the parser rejected is dropped,
  and an IK bone with no effector authors no chain. Nothing is solved or
  applied. The rig half of STAGE-O6 is decided: typeless prims and no API
  schema. `recoverable/rig-broken-relations.pmx` joins the fixtures (38), and
  the stage checks reconstruct every IK chain and append relation from the
  stage alone and compare them with the source.
- **`mmdModel`** (`libs/mmdModel/`), a plain static library with no OpenUSD:
  `mmd::Canonicalize(const pmx::Document&)` applies the one source-to-USD
  conversion (right-handed, facing +Z, 0.08 m per MMD unit), assigns stable
  ASCII identifiers with case-insensitive collision handling, orders joints
  parents-first (repairing self-parents and cycles), normalizes weights,
  derives material face ranges, and normalizes texture paths, refusing ones
  that are absolute, drive- or scheme-qualified, leave the model's
  directory, or hold a control character. It emits eight catalogued codes (`MMD_TEXT_TRAILING_NUL`,
  `MMD_PATH_UNSAFE_TEXTURE_PATH`, `MMD_SKEL_INVALID_PARENT`,
  `MMD_SKEL_PARENT_CYCLE`, `MMD_SKEL_JOINTS_REORDERED`,
  `MMD_SKEL_WEIGHTS_NORMALIZED`, `MMD_SKEL_ZERO_WEIGHTS`,
  `MMD_USD_IDENTIFIER_COLLISION`); the importer adds
  `MMD_SKEL_SDEF_APPROXIMATED` and `MMD_SKEL_QDEF_APPROXIMATED`. Installed as
  its own CMake package, shipped inside the plugin.
- `mmdPmx/DiagnosticList.h` is public, so the parser and the canonical model
  bound their diagnostics with one implementation.
- Fixtures: six new ones — joints reordered, identifiers (padding, fallback,
  collisions), a parent cycle, weights to normalize, unsafe texture paths, a
  model without bones — 33 in all, with the one-pixel texture files the
  samples name; `fixtures.json` now states, for each fixture that opens, what
  its stage must hold, computed by the generator from the design documents'
  rules. A second L5 golden, of the whole canonical stage.
- Tests: `mmdModel` unit tests, a robustness suite of 20,000 generated
  documents canonicalized twice, and its boundary check; the stage checks
  assert every fixture's identifiers, joint paths, bind translations,
  subsets, texture paths and their resolution, a vertex through the
  conversion, UsdSkel's binding, and every validator OpenUSD registers; the
  Unicode-path test copies the texture files too; the installed-consumer
  lane builds against `mmdModel` and canonicalizes every fixture.
- CI: `parser-sanitizers.yml` also builds `mmdModel` against an instrumented
  `mmdPmx` and runs its tests under ASan and UBSan.
- Documentation: the opening guide; the first report, on distributed models
  imported locally; STAGE-O1, -O2, -O3, -O5 and TEXT-O1, -O2 resolved as
  proposed; the stage, text and PMX contracts' Phase 2 sections marked
  binding; architecture, reference and roadmap pages updated to Phase 2.

- **Phase 1 PMX structural parser.** `mmd::pmx::Read` reads every table of
  PMX 2.0 and 2.1 into a `pmx::Document` of source facts — vertices with every
  deform type (QDEF in 2.1), faces, textures, materials with both toon
  references, bones with every conditional field and IK chains, all eleven
  morph types, display frames, rigid bodies, joints, and 2.1 soft bodies —
  at every index width. Text is decoded from UTF-8 or UTF-16LE into validated
  UTF-8; malformed text is read as empty, never repaired. Every read is
  bounded, every count bounded by the bytes left, and every index validated:
  an out-of-range one becomes "none" with `MMD_PMX_INDEX_OUT_OF_RANGE`. The
  parser emits every PMX-syntax and text-decoding code of the catalog, plus two
  new ones: `MMD_PMX_INVALID_LAYOUT_FLAG` (a toon reference, IK-link limit flag
  or display-frame element kind PMX does not define) and
  `MMD_PMX_FILE_UNREADABLE`. One code is recorded at most 16 times per table,
  then summarized.
- `mmd::pmx::ReadFile`, which takes a `std::filesystem::path`.
- **`mmd_inspect`** (`tools/mmdInspect/`): header, model names, table sizes,
  every element and every diagnostic of a PMX, as text or JSON, with no
  OpenUSD; exit status by the most severe diagnostic. Installed to `bin/` and
  shipped in the product.
- The importer parses the whole file: a malformed table now fails the open
  with its fatal code, every recoverable parser diagnostic is recorded on
  `/Asset`, and a model with soft bodies records
  `MMD_PHYSICS_SOFT_BODY_UNSUPPORTED`. The authored stage is unchanged.
- Fixtures: the generator writes models that use every table and record
  variant (PMX 2.0 UTF-16LE, 2.1 UTF-8, 2.1 with 4-byte indices), five that
  open with a recorded diagnostic, and thirteen more malformed ones — 27 in
  all; `fixtures.json` now states table counts, model names and the parser's
  own diagnostics.
- Tests: the parser table by table against an independent C++ encoder; a
  robustness suite that overwrites every byte of the sample models; a
  libFuzzer target; `mmd_inspect` against every fixture from a non-ASCII
  directory; the installed `mmd_inspect` in the installed-consumer lane.
- CI: `parser-sanitizers.yml`, hand-written, builds `mmdPmx` alone with Clang
  18 under ASan and UBSan, runs its tests, and fuzzes it seeded with the
  fixtures. `check_docs.py` now fails when the diagnostic catalog and the
  declared codes disagree.
- Documentation: the inspecting guide; the PMX contract's syntax-layer
  sections and the text policy's decoding sections marked binding, with the
  decisions the parser made recorded there; architecture, reference and
  roadmap pages updated to Phase 1.

- **Phase 0 workspace skeleton.** A root CMake workspace (`VERSION` 0.0.0,
  `CMakePresets.json`, `openstrata.toml`) that builds with `ost` and with
  plain CMake, pinned to OpenUSD 26.08 at configure time.
- **`mmdPmx`**, a plain static library with no OpenUSD dependency: the
  diagnostic record, `Result<T>`, and `mmd::pmx::Read`, which validates the
  PMX signature, version and globals and returns the header. Seven diagnostic
  codes are emitted. A boundary check fails the build's tests if its sources,
  its link line or anything linking it reaches OpenUSD.
- **`usdMmdFileFormat`**, the `.pmx` `SdfFileFormat`, reading through `Ar`:
  `Usd.Stage.Open` on a PMX authors `/Asset` (an `Xform`, `kind =
  component`) as the default prim, Y-up, meters, with
  `mmd:stageContractVersion`, `mmd:sourceFormat`, `mmd:sourceVersion` and any
  recoverable diagnostics in its `customData`. A fatal diagnostic fails the
  open with an error naming its code.
- A committed fixture generator and six generated PMX fixtures (PMX 2.0
  UTF-16LE, PMX 2.1 UTF-8, unknown globals, three malformed headers); stage-open
  and Unicode-path integration tests; the installed-consumer lane; a docs and
  version-mirror check.
- CI: `ost-source-ci.yml` generated from `openstrata.ci.yaml` (a graph cell,
  workspace and standalone-bundle cells on Windows, macOS and Linux), and a
  hand-written `docs-check.yml`.
- Documentation: the building guide and the package contract; the
  architecture, reference and roadmap pages updated to what exists.

- **Documentation baseline.** The design policy and five focused design
  contracts — stage, PMX, material, text encoding, and motion — distilled from
  the 2026-09-15 implementation policy; the workspace contract and external
  dependency policy; a capability matrix, diagnostic catalog and source-mapping
  table that state that nothing is implemented yet; the roadmap with the Phase
  0–8 sequence, the open-decision register, and the Phase 0 plan; and the
  documentation guidelines. No code.
- Apache-2.0 `LICENSE`.

### Changed

- Every GCC and Clang target compiles with `-ffp-contract=off`, so no
  floating-point expression is fused into an FMA and the same bytes author
  the same stage on every platform.
