# Inspecting a PMX

`mmd_inspect` reports what a PMX file contains — header, model names, table
sizes, every element if asked, and every diagnostic — using the parser alone,
with no OpenUSD in the process. It answers a parser question without an
importer in the way ([DESIGN_POLICY.md §5.5](../design/DESIGN_POLICY.md#55-tools)).
Every command on this page has been run, on Windows 11 in PowerShell, on
2026-09-15.

The build puts it in `tools/mmdInspect/bin/`, and an install in the prefix's
`bin/` ([building.md](building.md)).

## The report

```powershell
tools\mmdInspect\bin\mmd_inspect.exe plugins\usdMmdFileFormat\tests\fixtures\sample-2.1-utf8.pmx
```

```text
file:        plugins\usdMmdFileFormat\tests\fixtures\sample-2.1-utf8.pmx
format:      PMX 2.1, UTF-8, 2 additional vec4
index bytes: vertex 2, texture 2, material 2, bone 2, morph 2, rigid body 2
model:       サンプル2.1 (Sample 2.1)
tables:
  vertices               6  (BDEF1 1, BDEF2 2, BDEF4 1, QDEF 1, SDEF 1)
  faces                  3  (9 indices)
  textures               4
  materials              2
  bones                  4  (IK 1)
  morphs                 8  (additionalUv1 1, bone 1, flip 1, group 1, impulse 1, material 1, uv 1, vertex 1)
  display frames         3
  rigid bodies           2
  joints                 1
  soft bodies            1
diagnostics: none
```

`--elements` adds one line per texture, material, bone, morph, display frame,
rigid body, joint and soft body — names, the indices that relate them, flags.
Values are printed as the file states them: `mmd_inspect` converts nothing.

A file with problems lists each diagnostic with its severity, code and
location ([DIAGNOSTICS.md](../reference/DIAGNOSTICS.md)):

```text
diagnostics:
  error  MMD_PMX_INDEX_OUT_OF_RANGE: the index is 9, but the bones table holds 4; it is read as none (bones[0].parent)
```

and a file that cannot be read says so, with its fatal diagnostic:

```text
status:      not read
diagnostics:
  fatal  MMD_PMX_TRUNCATED_BUFFER: the file ends before this field does (the file is 115 bytes) (vertices[0].deform.weights[3] at byte 114)
```

## JSON, for scripts

```powershell
tools\mmdInspect\bin\mmd_inspect.exe --json plugins\usdMmdFileFormat\tests\fixtures\minimal.pmx
```

prints one object: `ok`; `header`, `model` and `counts` (where `faces`
counts face indices, three per triangle, as the file does); the deform-type
histogram; every element of every table; `diagnostics` (each with `code`,
`severity`, `message` and `location`); and `fatal`, `null` when the file was
read. An index that names nothing is `null`. Strings are UTF-8. The keys are
stable — the tool's test,
[tools/mmdInspect/tests/test_mmd_inspect.py](../../tools/mmdInspect/tests/test_mmd_inspect.py),
reads them — and messages are not: compare codes.

## Exit status

| Status | Meaning |
| --- | --- |
| `0` | read; nothing worse than a warning |
| `1` | read, with at least one `error` diagnostic |
| `2` | not read: a fatal diagnostic, including a file that cannot be opened (`MMD_PMX_FILE_UNREADABLE`) |
| `3` | a usage error |

## Paths and consoles

The path may name any directory, Japanese included: on Windows the executable
runs with a UTF-8 code page, so its arguments arrive in UTF-8, and the file is
opened through a `std::filesystem::path` built from them
([TEXT_ENCODING_POLICY.md §4](../design/TEXT_ENCODING_POLICY.md#4-no-locale-anywhere)).
The report is UTF-8 too; when it is written to a console, the console's output
code page is switched to UTF-8 while the tool runs and restored afterwards, so
Japanese names display correctly. Redirected to a file or a pipe, it is plain
UTF-8 bytes.
