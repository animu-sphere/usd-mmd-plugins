# Phase 5 against distributed models (2026-09-16)

Dated evidence from real runs; append-only
([contributing/documentation.md](../contributing/documentation.md)).

## What was run

The importer at Phase 5 (Windows 11, MSVC, OpenUSD 26.08) opened ten PMX
files held locally — distributed MMD character models, none of them a
fixture, none committed or redistributed
([DESIGN_POLICY.md §13](../design/DESIGN_POLICY.md#13-testing-policy)). For
each one, a script read **only the stage** — `/Asset/rig` and the Skeleton's
`mmd:bone:sourceIndex` — to reconstruct every IK chain as (IK bone, effector,
links) and every append as (bone, source) in bone-table terms, and compared
them with what `mmd_inspect --json` reports from the parser. Per joint it
also compared the transform layer and the after-physics flag, and it ran
every validator OpenUSD registers over the stage.

## Control semantics

| Model | Joints | IK chains | Links (limited) | Appends | Fixed axes | Local axes | Layers | Opened in | Reconstructed |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | ---: | --- |
| A | 280 | 4 | 6 (2) | 24 | 4 | 0 | 0–2 | 0.31 s | yes |
| B | 516 | 4 | 6 (2) | 28 | 4 | 38 | 0–2 | 0.35 s | yes |
| C | 490 | 4 | 6 (2) | 24 | 4 | 59 | 0–2 | 0.33 s | yes |
| D | 566 | 4 | 6 (2) | 24 | 4 | 38 | 0–1 | 0.37 s | yes |
| E | 444 | 4 | 6 (2) | 22 | 4 | 32 | 0–1 | 0.26 s | yes |
| F | 633 | 4 | 6 (2) | 26 | 4 | 38 | 0–1 | 0.44 s | yes |
| G | 366 | 4 | 6 (2) | 22 | 4 | 32 | 0–1 | 0.18 s | yes |
| H | 461 | 16 | 18 (12) | 66 | 4 | 76 | 0–2 | 1.23 s | yes |
| I | 692 | 5 | 7 (2) | 25 | 5 | 41 | 0–1 | 0.53 s | yes |
| J | 319 | 11 | 13 (2) | 12 | 4 | 2 | 0–3 | 0.35 s | yes |

- **Every chain and append is reconstructed.** On all ten models the IK
  chains and append relations read back from the stage alone are exactly the
  parser's, through the canonical joint order — Phase 5's acceptance
  ([DESIGN_POLICY.md §14](../design/DESIGN_POLICY.md#14-phases)).
- **Every append the source flags survives.** Each model's append count
  equals the number of bones with an append flag: none named a missing bone,
  so no relation was dropped.
- **Evaluation order survives.** Transform layers up to 3 and the
  after-physics flag match the source joint for joint. None of these models
  sets after-physics or an external parent; the fixtures cover both.
- **Validation is unchanged.** Adding `/Asset/rig` leaves every OpenUSD
  validator passing on all ten models.
- **No new diagnostics.** The rig raises none of its own; a dropped relation
  is reported by the parser, and none of these models has one.
