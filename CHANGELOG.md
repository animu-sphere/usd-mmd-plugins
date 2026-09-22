# Changelog

All notable changes to `usd-mmd-plugins` are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). The release
version is the single value in the repository-root `VERSION` file; the git tag
(`vX.Y.Z`) and this changelog mirror it, and each released version has a
record in [docs/releases/](docs/releases/README.md).

The **stage-contract version** is tracked separately from the package version:
it changes only when the downstream interpretation of the authored stage
changes incompatibly
([docs/design/STAGE_CONTRACT.md §2](docs/design/STAGE_CONTRACT.md#2-contract-version)).
Stage-contract version: **1**, authored since the Phase 0 importer.

## [Unreleased]

### Added

- **Phase 9 shared-motion adapters.** `mmdSkeletonAdapter` implements role-table
  version 1 and builds the PMX stage's `SkeletonDescriptor`, source rest and
  target `RetargetMap`; `mmdMotionAdapter` samples `mmdControl` over an explicit
  time range into evaluated `MotionClip` poses, root motion, namespaced MMD
  morph channels and a reserved visibility channel. Non-finite evaluated
  values are rejected at the shared boundary rather than replaced. Their
  manifests consume digest-pinned `motionCore` and
  `motionRetarget` v0.5.0 artifacts, and unit, boundary and installed-consumer
  tests cover both edges. Source and release CI pin `ost` 0.23.2 so those
  external artifacts are parsed, pulled and composed into root builds. CI
  also forwards the host-resolved Python development paths into the clean
  installed-consumer configure, provisions Python for Windows runtime
  validation, and recognizes the complete macOS OpenUSD foundation closure.
  `mmdSkeletonAdapter` now also states `motionRetarget`'s pinned `motionCore`
  artifact closure so its standalone build is complete. `mmdMotionBinding`
  now preserves the VMD model name as provenance. A deterministic skeletal
  acceptance now carries VMD-derived, IK-evaluated legs through `motionUsd`,
  retargets them with `motionRetarget` onto both a PMX-derived stage skeleton
  and a non-MMD skeleton, and binds the PMX target's `UsdSkelAnimation`;
  `motionUsd` v0.5.0 is digest-pinned for that test only.
  (`MOTION_CONTRACT.md` §10, §12; report 2026-09-22.)

- **Physics runtime integration direction.** A proposed focused contract now
  fixes the future boundary: the existing `/Asset/physics` stage remains the
  hand-off, MMD bone/body coupling stays in this repository, generic
  simulation and backend ownership live in `usd-physics-plugins`,
  `usd-stage-runner` owns frame order, and `usd-avatar-runtime` composes the
  pieces. The dependency is optional and no parser, canonical-model or
  importer package takes it. Documentation only; no simulation is claimed.
  (`PHYSICS_INTEGRATION.md`; `DESIGN_POLICY.md` §8, §14;
  `DEPENDENCIES.md` §7; `roadmap/current.md`.)

- **`mmdControl` (`libs/mmdControl/`), MMD control evaluation.** A plain
  library, with no OpenUSD and nothing of `usd-motion-plugins`, that
  evaluates a motion bound to a model at an explicit time into the local
  transform of every deformation joint: bone tracks sampled on their Bézier
  curves, bone morphs applied directly and through nested group morphs,
  appends — negative ratios and chains included — and IK by cyclic
  coordinate descent with angle limits, plane and Euler-limited links and the
  IK-enable track, in MMD's evaluation order. Every other bound morph is
  returned as a channel with its sampled weight. `Prepare` raises
  `MMD_MOTION_EXTERNAL_PARENT_IGNORED`, `MMD_MOTION_LOCAL_APPEND_APPROXIMATED`
  and `MMD_MOTION_IK_LOOP_CLAMPED`; `Evaluate` is stateless, and the same
  inputs give the same bits. It installs as a CMake package, and the
  installed-consumer lane evaluates a VMD fixture through it. MOT-O7 is
  resolved, and MOT-O9 — whether MMD's own IK leaves less distance at a
  model's loop count — is opened (`MOTION_CONTRACT.md` §9, §11; report
  2026-09-19).

### Changed

- **MMD IK is compared against an independent implementation (MOT-O9).**
  Over 12 local characters and two distributed motions, with the inputs
  matched to 0.02 mm, three.js r168's `CCDIKSolver` at the same stored loop
  count leaves no less distance than `MOTION_CONTRACT.md` §11.7 overall — a
  median 10.5 mm against 0.55 mm on an IK-authored motion, at most 29 mm in
  both — so §11.7 is kept and MOT-O9 is resolved. Where the reference leaves
  less, on a motion that keys its legs alongside their goals, the difference
  is that it starts a knee from its keyed rotation where §11.7 starts it
  from zero: MOT-O11, opened, for MMD's output to decide. Documentation
  only; report 2026-09-19. (`MOTION_CONTRACT.md` §9, §11.7;
  `CAPABILITY_MATRIX.md`.)

- **The humanoid role table and root motion are decided (MOT-O5, MOT-O6).**
  `MOTION_CONTRACT.md` §12, new: table version 1 maps MMD's conventional bone
  names — exact source names, deforming `D` bones first, never English names
  or folded spellings — to the shared core's `HumanJoint`s, with a required
  set of fifteen joints. A clip's rotations are derived from evaluated world
  rotations, relative to each joint's nearest mapped ancestor, and its root
  motion is the world transform of the joint `hips` maps to — no MMD bone is
  chosen as the root. As a target, `hips` binds the nearest common ancestor
  of the spine and legs. §10 follows what `usd-motion-plugins` has merged:
  `SkeletonDescriptor`, `RetargetMap` and `SourceRestPose` are
  `motionRetarget`'s, a descriptor carries the stage's joint tokens, and the
  packages are named `motionCore` and `motionRetarget`. MOT-O10, the rest a
  clip from MMD states, is opened: every local character's arms rest about
  40° below horizontal. Documentation only; measured in a dated report
  (2026-09-19) against 13 local characters and two distributed motions.
  (`MOTION_CONTRACT.md` §9, §10, §12; `DESIGN_POLICY.md` §5.7, §14, §20.1;
  `WORKSPACE.md` §1.2, §2, §2.4; `DEPENDENCIES.md` §6.)

- **The shared-motion edge is split by responsibility.** The
  `mmdMotionAdapter` emits only fully evaluated `MotionPose`/`MotionClip`
  data. A separate `mmdSkeletonAdapter` exposes
  `SkeletonDescriptor`, `RetargetMap` and `SourceRestPose` and owns the
  versioned MMD humanoid mapping. Both remain narrow consumers of
  `usd-motion-plugins`; neither implements generic retargeting. This boundary
  was decided in documentation first and implemented by the adapter change above.
  (`MOTION_CONTRACT.md` §10, §12;
  `DESIGN_POLICY.md` §5.7; `WORKSPACE.md` §1.2, §2.4.)

- **The design documents follow the `usd-motion-plugins` design policy.**
  VMD stays in this repository and `motionVmd` is no longer described as
  extraction-ready; MMD IK and append evaluation moves from the avatar
  runtime to a planned plain library here, `mmdControl`, superseding MOT-O3;
  planned `mmdMotionAdapter` and `mmdSkeletonAdapter` components are the only
  dependencies on `usd-motion-plugins`, separately building evaluated motion
  and the PMX skeleton/role description. Phase 9, shared motion core
  adoption, is added and runs before Phase 8; MOT-O5 to MOT-O8 are opened.
  Documentation
  only: no component, manifest or authored stage changes
  (`DESIGN_POLICY.md` §5.6, §5.7, §9, §14, §20; `MOTION_CONTRACT.md` §8.2,
  §10; `WORKSPACE.md` §1.2, §2.4, §7; `DEPENDENCIES.md` §6).

## [0.1.0] - 2026-09-17

The first release: `.pmx` opens as the canonical stage, every PMX 2.0 and 2.1
table is read, morphs, rig and physics are preserved without being evaluated,
and VMD motion is read and bound to a model — Phases 0–7.

### Added

- **Release machinery.** `release.yml`, on a `vX.Y.Z` tag equal to `VERSION`
  and a dated changelog section, builds the workspace on Windows, macOS and
  Linux against the pull request lanes' pinned runtimes, runs the bundle's
  verification pyramid against the build tree and the package, packages the
  bundle, both tools and the aggregate product twice to the same digests,
  installs the product into a fresh prefix and uses it with nothing from the
  build tree (`product_smoke.py`), and drafts a GitHub release with every
  archive, its manifest, a source archive, `SHA256SUMS`, and notes rendered
  from this changelog (`make_release_notes.py`,
  `docs/contributing/RELEASE_NOTES_TEMPLATE.md`). `workflow_dispatch` is a
  dry run. `check_docs.py` also fails when a manifest's required sibling
  range or the installed-consumer lane's `find_package` version excludes
  `VERSION`.

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
- **Phase 6 physics preservation.** A model with rigid bodies now authors
  `/Asset/physics`. Each rigid body is an `Xform` at its rest frame with
  `PhysicsRigidBodyAPI` (kinematic exactly when it follows its bone) and
  `PhysicsMassAPI`, and a guide-purpose `collider` child — a sphere, a cube
  scaled to the half extents, or a Y capsule — with `PhysicsCollisionAPI`.
  Each joint between two bodies is a `PhysicsJoint` with its bodies, its frame
  in each body's, and a `PhysicsLimitAPI` per limited axis; a free axis
  (lower above upper) authors none. Every PMX value is also a uniform
  `mmd:physics:*` attribute: bone, shape, size, collision group and mask,
  mass, damping, restitution, friction and mode on a body; type, bodies,
  position, orientation, limits and springs on a joint. PMX-O1 is resolved:
  Euler angles compose as `Ry · Rx · Rz`, from distributed models' capsules.
  The physics half of STAGE-O6 is decided. A PMX 2.1 joint type `UsdPhysics`
  has no counterpart for is a typeless prim with `MMD_PHYSICS_JOINT_UNMAPPED`;
  an undefined shape, mode or joint type falls back with
  `MMD_PHYSICS_UNKNOWN_SHAPE`, `MMD_PHYSICS_UNKNOWN_MODE` or
  `MMD_PHYSICS_UNKNOWN_JOINT_TYPE`; a joint whose body is missing is dropped.
  Nothing is simulated and no `PhysicsScene` is authored. `usdPhysics` joins
  the linked modules, `recoverable/physics-repairs.pmx` joins the fixtures
  (39), and the stage checks recover every rigid body and joint from the stage
  alone.
- **Phase 7 VMD.** `motionVmd` (`libs/motionVmd/`), a plain static library
  with no dependency at all, reads VMD: `motionVmd::Read` holds the header
  and every section — bone, morph, camera, light, self-shadow and IK /
  visibility keyframes — in file order, in the source basis, for both
  signatures and for a file that ends after any section; `BuildMotion` groups
  the records into tracks per name, sorted by frame, keeps the last record of
  a duplicate frame (`MMD_MOTION_DUPLICATE_KEYFRAME`), and decodes the Bézier
  curves, reading Z's and rotation's `x1` from the row a physics toggle does
  not overwrite. Names are CP932, decoded through a table the project
  generates from Microsoft's code page (`generate_cp932_table.py`, checked by
  a test), with their bytes kept; a cut character is dropped
  (`MMD_TEXT_TRUNCATED_CP932`) and an unmapped one refused
  (`MMD_TEXT_INVALID_CP932`). Malformed files fail with
  `MMD_MOTION_BAD_SIGNATURE`, `MMD_MOTION_TRUNCATED_BUFFER` or
  `MMD_MOTION_COUNT_EXCEEDS_BUFFER`, bytes after the last section are
  `MMD_MOTION_TRAILING_BYTES`, and `ReadFile` raises
  `MMD_MOTION_FILE_UNREADABLE`. `vmd_inspect` reports on a VMD, as text or
  JSON. `mmdMotionBinding` (`libs/mmdMotionBinding/`) binds a motion to a
  canonical model: names compared as CP932 bytes cut to the VMD field, as MMD
  does, with `MMD_MOTION_UNMATCHED_BONE`, `MMD_MOTION_UNMATCHED_MORPH`,
  `MMD_MOTION_AMBIGUOUS_NAME` and `MMD_MOTION_UNENCODABLE_NAME`, and bone keys
  converted with `mmdModel`'s basis functions. Nothing is baked: MOT-O1 is
  resolved (the conversion lives in `mmdModel` and is applied by binding) and
  MOT-O3 (the Phase 8 runtime evaluates IK and append transforms). Tests: the
  reader's unit, robustness and boundary tests and fuzz target, the table
  check, binding's unit and boundary tests, `vmd_inspect` over nine generated
  VMD fixtures from two directories, and the installed-consumer lane reading
  and binding through the installed packages. `check_library_boundaries.py`
  gains `--forbid-include`. A report records the reader and binding against
  three distributed motions and 25 models.
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

- `VERSION` is 0.1.0, and so is every manifest and CMake fallback; each
  component requires its siblings at `>=0.1,<0.2`, and the installed-consumer
  lane asks for 0.1.
- The capability matrix states the Phase 3 material rows as implemented —
  the portable realizations approximated, the MMD semantics preserved; they
  had kept the documentation baseline's `—`.
- Every GCC and Clang target compiles with `-ffp-contract=off`, so no
  floating-point expression is fused into an FMA and the same bytes author
  the same stage on every platform.

[Unreleased]: https://github.com/animu-sphere/usd-mmd-plugins/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/animu-sphere/usd-mmd-plugins/releases/tag/v0.1.0
