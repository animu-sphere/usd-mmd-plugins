# Phase 5 — control semantics

Status: ⬜ not started.

`/Asset/rig` is the reserved scope for MMD's **control** semantics, the ones
the deformation skeleton deliberately does not carry: IK chains, append
rotation and translation, fixed and local axes, external parents, the
transform layer and the after-physics flag, and bone tails. Phase 5 authors
them declaratively, so a consumer can reconstruct every relation from the
stage alone — and solves none of them, as
[DESIGN_POLICY.md §2.2](../design/DESIGN_POLICY.md#22-the-static-importer-boundary)
requires.

## Outcome

```text
Usd.Stage.Open("model.pmx")
    → …                       the Phase 0–4 stage, unchanged
    → /Asset/rig              IK chains, append relations, axes, external parents
```

## What remains

- ⬜ Decide the prim shape: plain `mmd:rig:*` attributes on typeless prims, or
  an admitted API schema under
  [DESIGN_POLICY.md §6](../design/DESIGN_POLICY.md#6-the-schema-admission-test)
  — that is STAGE-O6, in
  [STAGE_CONTRACT.md §16](../design/STAGE_CONTRACT.md#16-open-questions).
- ⬜ Carry the canonical control semantics in `mmdModel`: IK targets, links
  and their rotation limits through the basis conversion
  ([STAGE_CONTRACT.md §6.3](../design/STAGE_CONTRACT.md#63-conversion-functions)),
  append sources and ratios, fixed and local axes, external-parent keys.
- ⬜ Author `/Asset/rig` and make
  [STAGE_CONTRACT.md §12](../design/STAGE_CONTRACT.md#12-control-rig--reserved)
  binding, with fixtures for every relation and for the repairs a broken
  chain needs.
- ⬜ Prove the acceptance: a consumer can reconstruct every IK chain and
  append relation from the stage alone.

## Completion criteria

[DESIGN_POLICY.md §14](../design/DESIGN_POLICY.md#14-phases)'s acceptance for
Phase 5: **a consumer can reconstruct every IK chain and append relation from
the stage alone** — with nothing solved while the file opens.

Phase 6 (physics preservation) and the preservation parts of Phase 5 together
complete what
[DESIGN_POLICY.md §14.1](../design/DESIGN_POLICY.md#141-first-substantial-release--definition-of-done)
still wants for the first substantial release; Phases 0–4 are done.
