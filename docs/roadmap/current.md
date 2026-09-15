# Phase 1 — PMX structural parser

Status: 🚧 in progress — implemented and verified on Windows, and the
parser's sanitizer build on Linux; waiting for its first CI run.

`mmdPmx` reads every table of a PMX 2.0 or 2.1 file into source facts, bounded
and validated, with a diagnostic for everything it repairs or refuses;
`mmd_inspect` reports on it without USD. Nothing is authored from the tables
yet — that is Phase 2, the canonical stage.

## Outcome

```text
mmd_inspect model.pmx
    → mmdPmx::ReadFile → Read: header, text, vertices, faces, textures,
      materials, bones, morphs, display frames, rigid bodies, joints,
      soft bodies (2.1) — or the fatal diagnostic that stopped it
    → counts, names, flags, diagnostics; exit status by severity

Usd.Stage.Open("model.pmx")
    → the same parse, through Ar; a malformed table fails the open
    → the Phase 0 stage, with every recoverable diagnostic on /Asset
```

What exists is recorded where it belongs: the syntax-layer decisions in
[PMX_CONTRACT.md](../design/PMX_CONTRACT.md) (§1–§13 and §15, now binding) and
[TEXT_ENCODING_POLICY.md §3](../design/TEXT_ENCODING_POLICY.md#3-decoding-pmx-text);
the components and their gates in
[WORKSPACE.md](../architecture/WORKSPACE.md); the codes in
[DIAGNOSTICS.md](../reference/DIAGNOSTICS.md); the parsing claims in
[CAPABILITY_MATRIX.md](../reference/CAPABILITY_MATRIX.md#pmx-parsing-mmdpmx-mmd_inspect);
the commands in [guides/building.md](../guides/building.md) and
[guides/inspecting.md](../guides/inspecting.md); the shipped scope in the
[changelog](../../CHANGELOG.md).

## What remains

- 🚧 The first CI run of this Phase, green on every cell: `ost-source-ci.yml`
  (the graph cell now resolves `mmd_inspect` as a workspace tool; the
  workspace cells run its tests and the robustness suite; the bundle cells
  open all 27 fixtures), `docs-check.yml` (now also checking the diagnostic
  catalog against the declared codes), and the first run of
  `parser-sanitizers.yml`. The libFuzzer build has not run anywhere yet —
  Clang's libFuzzer is not on the Windows host — so a problem it finds is
  fixed as part of this Phase.
- ⬜ Once CI is green: this page replaced by the Phase 2 plan, and the
  roadmap's status table updated. Phase 2 needs STAGE-O1, -O2, -O3, -O5 and
  TEXT-O1, -O2 answered first ([open decisions](README.md#open-decisions)).

## Completion criteria

[DESIGN_POLICY.md §14](../design/DESIGN_POLICY.md#14-phases)'s acceptance for
Phase 1, and where each stands:

- **Deterministic parser fixtures.** The generator writes 27 fixtures —
  every table and record variant of PMX 2.0 and 2.1, every index width, both
  encodings, Japanese names, the recoverable cases and the fatal ones — and
  `workspace_fixtures` fails unless the committed bytes are exactly its
  output. The parser's unit tests build their own bytes with an independently
  written encoder. *Met, on Windows.*
- **Malformed-input tests.** Each fatal and recoverable code at its byte
  (`mmdPmx_unit`), every prefix and every byte overwritten
  (`mmdPmx_robustness`), and the fixtures through the tool and the importer.
  *Met on Windows (MSVC) and under ASan and UBSan on Linux (GCC 15, WSL);
  the fuzzer: CI.*
- **No OpenUSD in the parser's link line.** `mmdPmx_boundaries`, and now
  `mmd_inspect_boundaries` for the tool. *Met.*
- Both build modes, and the installed prefix. `ost plugin test --workspace
  --graph-only` resolves the tool; plain `ctest` passes, installed-consumer
  lane included. *Windows: met. macOS, Linux: CI.*
