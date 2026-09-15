# PMX contract

> Status: §1–§13 and §15 are **binding** for the syntax layer: the Phase 1
> parser (`mmdPmx`) implements them, with fixtures. §14 steps 1–5 and 8, the
> texture references of step 6, and the "Canonical" column of §5 (vertices
> and deform), are **binding** since
> Phase 2, where `mmdModel` implements them for what the stage authors: the
> model's metadata, textures, vertices, faces, materials' face ranges and
> texture slots, and bones' names, positions and parents. The rest of the
> "Canonical" columns and §14 step 7 (morphs, control, physics) stay
> **proposed** until the Phase that authors each. This document
> fixes how PMX 2.0 and 2.1 bytes are read and what each source concept
> becomes in the canonical model. It records *decisions*; the byte layout
> below is a summary for orientation, and the parser's fixtures —
> [tests/fixtures/generate_fixtures.py](../../tests/fixtures/generate_fixtures.py)
> and the unit tests' encoder in
> [libs/mmdPmx/tests/](../../libs/mmdPmx/tests/) — are the authority on
> layout.
>
> What the canonical values become in USD is
> [STAGE_CONTRACT.md](STAGE_CONTRACT.md)'s; how text is decoded and names are
> formed is [TEXT_ENCODING_POLICY.md](TEXT_ENCODING_POLICY.md)'s. Section
> numbers are stable.

---

## 1. Scope

- **In:** PMX 2.0 and PMX 2.1, as described by the PMX specification text
  distributed with PmxEditor.
- **Out:** PMD. If PMD is ever supported it is a separate library
  (`libs/mmdPmd`) with its own Shift-JIS/CP932 policy, and `mmdPmx` gains no
  PMD branch
  ([TEXT_ENCODING_POLICY.md §9](TEXT_ENCODING_POLICY.md#9-pmd-and-vmd)).
- **Out:** writing PMX.

Two layers, never merged:

| Layer | Library | Holds | Basis and units |
| --- | --- | --- | --- |
| Syntax | `mmdPmx` | `pmx::Document`: every table as the file states it, decoded text, validated indices | source (left-handed, MMD units) |
| Semantics | `mmdModel` | `mmd::CanonicalDocument`: stable identities, canonical order, normalized deform, morph, material, control and physics semantics, provenance | USD (right-handed, meters) |

## 2. Reading rules

These apply to every read in `mmdPmx`:

- **Little-endian**, explicitly. `float` is IEEE-754 binary32.
- **Bounded.** Every read checks the remaining length first; a read past the end
  is `MMD_PMX_TRUNCATED_BUFFER` (fatal) with the byte offset.
- **Overflow-checked** size arithmetic: `count × recordSize` is computed in a
  type that cannot wrap, and a wrap is `MMD_PMX_COUNT_EXCEEDS_BUFFER`.
- **Counts are bounded by bytes, not by constants.** A table count `n` is
  accepted only if `n × minimumRecordSize ≤ remainingBytes`, where
  `minimumRecordSize` is the smallest encoding one record of that table can
  have. That bounds every allocation by the input size — a 1 KB file can never
  request a gigabyte — without an arbitrary cap that a legitimate large model
  could hit. A negative count is `MMD_PMX_COUNT_EXCEEDS_BUFFER`.
- **Text** is an `int32` byte length followed by that many bytes, decoded per
  the header's encoding
  ([TEXT_ENCODING_POLICY.md §3](TEXT_ENCODING_POLICY.md#3-decoding-pmx-text)).
  The length is a count like any other: negative, or more than the bytes
  left, is `MMD_PMX_COUNT_EXCEEDS_BUFFER`.
- **A byte that selects the layout of what follows** — a material's toon
  reference (§8), an IK link's limit flag (§9), a display-frame element's kind
  (§11) — must hold a value PMX defines. Any other value is
  `MMD_PMX_INVALID_LAYOUT_FLAG` (fatal): the record's length is unknown. The
  deform type and the morph type select layouts too, and have codes of their
  own (§5, §10).
- **Trailing bytes** after the last table are `MMD_PMX_TRAILING_BYTES`
  (warning, recoverable) and are ignored.
- **No pointer walking.** The reader is a cursor over a `std::span<const
  std::byte>`; no raw pointer arithmetic escapes it.

## 3. Header and globals

| Field | Encoding | Rule |
| --- | --- | --- |
| signature | 4 bytes | `"PMX "` (`50 4D 58 20`), else `MMD_PMX_BAD_SIGNATURE` (fatal) |
| version | `float` | exactly `2.0` or `2.1`, else `MMD_PMX_UNSUPPORTED_VERSION` (fatal) |
| globals count | `uint8` | ≥ 8, else `MMD_PMX_INVALID_GLOBALS` (fatal); bytes beyond 8 are preserved and `MMD_PMX_UNKNOWN_GLOBALS` (warning) is raised |
| globals[0] | `uint8` | text encoding: `0` UTF-16LE, `1` UTF-8; else `MMD_TEXT_INVALID_ENCODING_FLAG` (fatal) |
| globals[1] | `uint8` | additional vec4 count, `0`–`4`; else `MMD_PMX_INVALID_GLOBALS` (fatal) |
| globals[2]–[7] | `uint8` each | index sizes for vertex, texture, material, bone, morph, rigid body: each `1`, `2` or `4`; else `MMD_PMX_INVALID_INDEX_SIZE` (fatal) |
| model name, English name, comment, English comment | text × 4 | decoded; preserved verbatim as provenance |

The version is compared exactly: the two legal values are exactly
representable in binary32.

## 4. Indices

| Index kind | Width 1 | Width 2 | Width 4 | "none" |
| --- | --- | --- | --- | --- |
| vertex | `uint8` | `uint16` | `int32` | — (never optional) |
| texture, material, bone, morph, rigid body | `int8` | `int16` | `int32` | `−1` |

An index is validated against its table's size once that table has been read.
Forward references (a bone whose parent is a later bone, a morph naming a later
morph) are legal and are validated after the table is complete. A negative
index other than `−1`, or an index ≥ the table size, is out of range. In a face
that is `MMD_PMX_FACE_INDEX_OUT_OF_RANGE` (fatal, §6); everywhere else it is
`MMD_PMX_INDEX_OUT_OF_RANGE` (recoverable). A vertex index read at width 4
that is negative is out of range.

**The syntax layer repairs, the canonical layer drops.** An out-of-range index
becomes `−1` in the `pmx::Document`, so the document holds one record per
record in the file and every index in it names an element or is `−1`. What a
`−1` then means for each relation — an influence dropped, a bone made a root, a
joint dropped — is canonicalization's to apply, as each table below says. `−1`
itself is a legal "none" for every non-vertex index kind; the syntax layer
reports it only where §5 says a none is an error.

## 5. Vertices and deform

Per vertex: position (vec3), normal (vec3), UV (vec2), additional vec4 × *n*
(§3), deform type (`uint8`), deform data, edge scale (`float`).

| Type | Value | Data | Versions | Canonical |
| --- | --- | --- | --- | --- |
| BDEF1 | 0 | bone | 2.0, 2.1 | one influence |
| BDEF2 | 1 | bone ×2, weight₁ | 2.0, 2.1 | two influences, weight₂ = 1 − weight₁ |
| BDEF4 | 2 | bone ×4, weight ×4 | 2.0, 2.1 | four influences, normalized ([STAGE_CONTRACT.md §9.5](STAGE_CONTRACT.md#95-weight-normalization)) |
| SDEF | 3 | bone ×2, weight₁, C, R0, R1 (vec3 each) | 2.0, 2.1 | as BDEF2, plus C/R0/R1 converted as points and preserved |
| QDEF | 4 | as BDEF4 | 2.1 only | as BDEF4, deform type preserved |

- A deform type outside the table, or QDEF in a 2.0 file, is
  `MMD_PMX_INVALID_DEFORM_TYPE` (fatal: the record length is unknown, so the
  rest of the file cannot be located).
- A bone index of `−1` inside a deform with weight 0 is accepted and dropped. A
  `−1` with a non-zero weight — BDEF1's implicit weight is 1, BDEF2's and
  SDEF's second is 1 − weight₁ — and an out-of-range bone with any weight are
  `MMD_PMX_INDEX_OUT_OF_RANGE` (recoverable): the influence is dropped and the
  vertex's remaining weights are renormalized in canonicalization.
- SDEF is **approximated** as linear blend skinning in generic `UsdSkel`; its
  parameters are preserved for an SDEF-aware consumer
  ([DESIGN_POLICY.md §7.1](DESIGN_POLICY.md#71-conventional-usd-first-source-semantics-preserved-beside-it)).
- QDEF is **unverified**: no consumer that performs dual-quaternion skinning
  has been verified against it.

## 6. Faces

An `int32` count of vertex indices, then that many vertex indices. The count
must be a multiple of 3 (`MMD_PMX_FACE_COUNT_NOT_TRIANGLES`, fatal). Every face
is a triangle. A face referencing an out-of-range vertex is
`MMD_PMX_FACE_INDEX_OUT_OF_RANGE` (fatal: dropping the face would shift every
material face range after it). Winding is converted in canonicalization, not here.

## 7. Textures

An `int32` count, then that many text paths. Each path is kept exactly as
decoded in the syntax layer; normalization, safety checks and asset-path
formation happen in canonicalization
([TEXT_ENCODING_POLICY.md §7](TEXT_ENCODING_POLICY.md#7-texture-paths)).

## 8. Materials

| Field | Encoding | Canonical |
| --- | --- | --- |
| name, English name | text × 2 | display names + stable identifier |
| diffuse | vec4 (RGBA) | base color and opacity |
| specular | vec3 | specular color |
| specular power | `float` | specular exponent |
| ambient | vec3 | ambient color |
| drawing flags | `uint8` | bits below |
| edge color | vec4 | outline color |
| edge size | `float` | outline size |
| texture | texture index | base texture, or none |
| sphere texture | texture index | sphere (environment) texture, or none |
| sphere mode | `uint8` | `0` disabled, `1` multiply, `2` add, `3` sub-texture |
| toon reference | `uint8` | `0` texture index follows, `1` shared toon slot (`uint8`, `0`–`9`) follows; anything else is `MMD_PMX_INVALID_LAYOUT_FLAG` (§2) |
| toon value | texture index or `uint8` | one canonical toon ramp (below) |
| memo | text | preserved as provenance |
| face count | `int32` | number of vertex indices this material draws |

| Flag bit | Meaning | Versions |
| --- | --- | --- |
| `0x01` | no culling (double-sided) | 2.0, 2.1 |
| `0x02` | ground shadow | 2.0, 2.1 |
| `0x04` | casts self-shadow (draws into the shadow map) | 2.0, 2.1 |
| `0x08` | receives self-shadow | 2.0, 2.1 |
| `0x10` | draws edge (outline) | 2.0, 2.1 |
| `0x20` | vertex color | 2.1 |
| `0x40` | point drawing | 2.1 |
| `0x80` | line drawing | 2.1 |

Decisions:

- **Face ranges.** Materials consume the face index table in order; material
  *k* draws the next `faceCount` indices. `faceCount` must be a multiple of 3
  and the counts must sum to the face index count. A count that is not a
  multiple of 3, or a sum that exceeds the table, is
  `MMD_PMX_MATERIAL_FACES_EXCEED_TABLE` (fatal). A sum that falls short is
  `MMD_PMX_MATERIAL_FACES_SHORT` (recoverable): the tail is left unbound and the
  subsets become `nonOverlapping`.
- **Toon normalization.** A shared slot and an individual toon texture become
  **one** canonical `ToonRamp` semantic with two variants — `individual`
  (a texture reference) and `shared` (slot 0–9, which MMD resolves to
  `toon01.bmp`–`toon10.bmp` in its own data folder). Consumers read one
  semantic; the variant is data
  ([MATERIAL_POLICY.md §7](MATERIAL_POLICY.md#7-toon-ramps)).
- **Sphere mode** outside `0`–`3` is `MMD_MATERIAL_UNSUPPORTED_SPHERE_MODE`
  (recoverable, sphere disabled).
- **Texture index** out of range is `MMD_PMX_INDEX_OUT_OF_RANGE`
  (recoverable, slot empty).
- 2.1-only flag bits in a 2.0 file are preserved and raise no error; they carry
  no meaning there.

## 9. Bones

| Field | Present when | Canonical |
| --- | --- | --- |
| name, English name | always | display names + stable identifier |
| position | always | bind translation (converted) |
| parent | always (bone index) | hierarchy ([STAGE_CONTRACT.md §9.1](STAGE_CONTRACT.md#91-canonical-joint-order)) |
| transform layer | always (`int32`) | control semantics |
| flags | always (`uint16`) | bits below |
| tail | always: bone index if `0x0001`, else vec3 offset | display semantics |
| append parent + ratio | `0x0100` or `0x0200` | control semantics |
| fixed axis | `0x0400` | control semantics |
| local X and Z axes | `0x0800` | control semantics |
| external parent key | `0x2000` | control semantics |
| IK target, loop count, limit angle, links (bone, has-limit, min, max) | `0x0020` | declarative IK chain; a has-limit byte other than `0`/`1` is `MMD_PMX_INVALID_LAYOUT_FLAG` (§2) |

| Flag bit | Meaning |
| --- | --- |
| `0x0001` | tail is a bone index |
| `0x0002` | rotatable |
| `0x0004` | translatable |
| `0x0008` | visible |
| `0x0010` | operable |
| `0x0020` | IK |
| `0x0080` | local append: append the parent's local transform |
| `0x0100` | append (inherit) rotation |
| `0x0200` | append (inherit) translation |
| `0x0400` | fixed axis |
| `0x0800` | local axes |
| `0x1000` | deform after physics |
| `0x2000` | external parent deform |

Decisions:

- The **deformation skeleton** is built from positions and parents only.
  Everything else in this table is control or display semantics, carried
  declaratively into the canonical model and authored under `/Asset/rig` from
  Phase 5. Nothing is evaluated.
- **Transform layer** and **deform after physics** determine MMD's evaluation
  order. They are preserved exactly; the canonical joint order is *not* an
  evaluation order and must not be read as one.
- An out-of-range **parent** is recoverable (the bone becomes a root). An
  out-of-range IK target, IK link, append parent or tail bone is recoverable:
  that relation is dropped with `MMD_PMX_INDEX_OUT_OF_RANGE`.
- Flag bits not listed are preserved and ignored.

## 10. Morphs

Per morph: name, English name, panel (`uint8`), type (`uint8`), `int32` offset
count, offsets.

| Type | Value | Offset data | Versions | Canonical → USD |
| --- | --- | --- | --- | --- |
| group | 0 | morph, weight | 2.0, 2.1 | member relationships + weights, preserved |
| vertex | 1 | vertex, displacement vec3 | 2.0, 2.1 | `UsdSkelBlendShape` |
| bone | 2 | bone, translation vec3, rotation quaternion | 2.0, 2.1 | per-joint deltas, preserved |
| UV | 3 | vertex, vec4 | 2.0, 2.1 | per-vertex offsets, preserved |
| additional UV 1–4 | 4–7 | vertex, vec4 | 2.0, 2.1 | per-vertex offsets, preserved |
| material | 8 | material (`−1` = all), operation (`0` multiply, `1` add), diffuse, specular, specular power, ambient, edge color, edge size, texture tint, sphere tint, toon tint | 2.0, 2.1 | declarative material modulation, preserved |
| flip | 9 | morph, weight | 2.1 | member relationships + weights, preserved |
| impulse | 10 | rigid body, local flag, velocity vec3, torque vec3 | 2.1 | physics impulse, preserved, never executed |

| Panel | Value |
| --- | --- |
| hidden (system) | 0 |
| eyebrow | 1 |
| eye | 2 |
| mouth | 3 |
| other | 4 |

Decisions:

- An unknown morph type, or a 2.1-only type in a 2.0 file, is
  `MMD_PMX_INVALID_MORPH_TYPE` (fatal: the offset record length is unknown).
- An unknown panel value is preserved as `other` with
  `MMD_MORPH_UNKNOWN_PANEL` (recoverable).
- Vertex-morph displacements and bone-morph translations are displacements
  (§6.3 of the stage contract); bone-morph rotations are quaternions; UV
  offsets are **not** flipped — they are raw deltas in the channel they modify.
  (A UV morph on the primary UV is recorded against source `v`; a consumer
  applying it to `primvars:st` negates the `v` delta. Recorded explicitly so it
  is not rediscovered.)
- A group or flip morph that references itself, directly or through other
  group morphs, is `MMD_MORPH_GROUP_CYCLE` (recoverable): the cyclic member is
  dropped. Nothing is expanded, so this is a validation, not an evaluation.
- Out-of-range offset targets are recoverable: the offset is dropped with
  `MMD_PMX_INDEX_OUT_OF_RANGE`.

## 11. Display frames

Per frame: name, English name, special flag (`uint8`), `int32` element count,
elements (`uint8` kind — `0` bone, `1` morph — and the index). Any other kind
is `MMD_PMX_INVALID_LAYOUT_FLAG` (§2); an out-of-range index is
`MMD_PMX_INDEX_OUT_OF_RANGE`, and the element is dropped in canonicalization.

Display frames organize bones and morphs for an editor's UI; they carry no
geometry, deformation or rendering meaning. They are parsed and kept in the
canonical model and are **not authored** in contract v1. They are the most
likely source of a morph *category* if a consumer needs one.

## 12. Soft bodies (2.1)

PMX 2.1 appends a soft-body table after the joints. Contract v1 parses it
completely, so that a 2.1 file is traversed to its end and trailing-byte
detection (§2) stays meaningful, and keeps it in the syntax layer only. Soft
bodies are **unsupported** for authoring; a model with any raises
`MMD_PHYSICS_SOFT_BODY_UNSUPPORTED` (warning), once per import, from the
importer.

Per soft body: name, English name, shape (`uint8`: `0` triangle mesh, `1`
rope), material index, group (`uint8`), non-collision mask (`uint16`), flags
(`uint8`: `0x01` B-link, `0x02` cluster creation, `0x04` link crossing), B-link
distance and cluster count (`int32`), total mass and collision margin
(`float`), aero model (`int32`), 12 configuration and 6 cluster coefficients
(`float`), 4 iteration counts (`int32`), 3 material coefficients (`float`), an
`int32` count of anchors (rigid-body index, vertex index, near mode `uint8`),
and an `int32` count of pinned vertex indices.

## 13. Rigid bodies and joints

Rigid body: name, English name, bone (index or `−1`), group (`uint8`, 0–15),
non-collision mask (`uint16`), shape (`0` sphere, `1` box, `2` capsule), size
(vec3), position (vec3), rotation (vec3, radians), mass, linear damping,
angular damping, restitution, friction (`float` each), physics mode (`0`
follows bone, `1` simulated, `2` simulated with bone position alignment).

Joint: name, English name, type (`uint8`), rigid body A, rigid body B,
position, rotation (vec3, radians), translation min/max, rotation min/max,
translation spring, rotation spring (vec3 each). Type `0` is the spring 6-DOF
constraint in both versions; 2.1 adds `1` 6-DOF, `2` point-to-point, `3`
cone-twist, `4` slider, `5` hinge.

Decisions:

- Everything is **preserved** (Phase 6), converted per the stage contract's
  §6.3; nothing is simulated.
- **Euler rotations** are composed to a matrix before conversion. The PMX
  Euler composition order (PMX-O1) is fixed by a fixture built against a
  reference implementation before Phase 6; until then rotations are carried in
  the canonical model as the source triple, unconverted, and not authored.
- Sizes are lengths (`× s`). Spring constants are preserved in source units:
  their unit depends on the length scale, and converting them is a physics
  runtime's decision, not the importer's.
- An out-of-range bone or rigid-body reference is recoverable: the rigid body
  is unattached, or the joint is dropped, with `MMD_PMX_INDEX_OUT_OF_RANGE`.

## 14. Canonicalization

What `Canonicalize(const pmx::Document&)` does, in order:

1. **Identity.** Assign every material, bone, morph, rigid body and joint a
   stable identifier ([TEXT_ENCODING_POLICY.md §6](TEXT_ENCODING_POLICY.md#6-stable-identifiers)).
2. **Basis.** Convert every spatial value to the USD basis and meters
   ([STAGE_CONTRACT.md §6](STAGE_CONTRACT.md#6-coordinate-conversion)), once.
3. **Skeleton.** Compute the canonical joint order and remap every bone index
   in the document to it.
4. **Deform.** Expand BDEF1/2/SDEF to explicit influence lists, normalize
   weights, keep deform type and SDEF parameters.
5. **Mesh.** Reverse winding, flip `st`, derive material face ranges.
6. **Materials.** Normalize toon ramps, sphere modes and texture references.
7. **Morphs, control, physics.** Carry declaratively, with indices remapped.
8. **Provenance.** Keep source indices and decoded names for every element.

It never reads a file, never touches OpenUSD, and never evaluates anything.
The same `pmx::Document` always produces the same `CanonicalDocument`, bit for
bit, and nothing in a document the parser accepted is fatal to it: every
repair is a recoverable diagnostic.

Each step covers only the tables the stage authors so far: step 1 names
materials and bones; step 6 normalizes texture references, and toon ramps and
sphere modes with Phase 3; step 7 is empty until Phase 4. An element kind joins
identity and provenance with the Phase that authors it, so no diagnostic is
raised about something the stage does not contain.

## 15. Fatal versus recoverable

A diagnostic is **fatal** when the parser can no longer locate the rest of the
file (bad signature, version, globals or index sizes; truncation; a count or
text length the bytes cannot hold; an unknown deform or morph type or layout
flag) or when continuing would author a stage whose basic
structure is wrong (face ranges overrunning the table, a face referencing a
missing vertex). A fatal diagnostic makes `SdfFileFormat::Read` fail, and the
stage does not open.

Everything else is **recoverable**: the element or relation is dropped or
repaired as each section states, the diagnostic is recorded
([STAGE_CONTRACT.md §5](STAGE_CONTRACT.md#5-model-metadata-and-provenance)),
and the import continues. Codes and severities are catalogued in
[reference/DIAGNOSTICS.md](../reference/DIAGNOSTICS.md).

## 16. Open questions

| Id | Question | Resolve by |
| --- | --- | --- |
| PMX-O1 | Euler composition order of rigid-body and joint rotations | fixture against a reference implementation, before Phase 6 |
| PMX-O2 | Whether QDEF can be claimed against any verified consumer | a dual-quaternion consumer to verify against |
| PMX-O3 | Whether a globals count above 8 occurs in the wild, and what the extra bytes mean | evidence from real files |
| PMX-O4 | Whether display frames should author a morph category | a consumer that needs one |
