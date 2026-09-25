# Phase 8 canonical material values as UsdShade inputs (2026-09-25)

Dated evidence from real runs; append-only
([contributing/documentation.md](../contributing/documentation.md)).

## Question

On 2026-09-22 [MATERIAL_POLICY.md](../design/MATERIAL_POLICY.md) kept the
canonical material values as plain `mmd:material:*` attributes, with
`MmdMaterialAPI` declaring them. The `/preview` and `/mtlx` graphs copy the
values they need. On 2026-09-25 `usd-vrm-plugins` measured that UsdShade
connects only `inputs:` and `outputs:` attributes, and moved its canonical
values to `inputs:vrm:*` (its MATERIAL_ARCHITECTURE_POLICY §6.4.1). Material
morphs change MMD material values at runtime
([MATERIAL_POLICY.md §11](../design/MATERIAL_POLICY.md#11-material-morphs)).
Can a changed canonical value reach an MMD realization, and in what form?

## What was run

A scratch Python script built one stage per case. Each stage had a quad and a
`UsdShadeMaterial` whose canonical diffuse value was time-sampled red at frame
1 and blue at frame 2. A `/preview` NodeGraph had an interface input connected
to that value, and its `UsdPreviewSurface` read it through connections. The
surface had `diffuseColor` black and `roughness` 1, as the importer writes it.
`usdrecord` rendered frames 1 and 2 in Storm with OpenUSD 26.08, and the
centre pixel was read.

| Case | Canonical value | How the surface reads it | Frame 1 | Frame 2 |
| --- | --- | --- | --- | --- |
| A | `inputs:mmd:material:diffuseColor`, `color4f` | white 4×4 `UsdUVTexture`, `scale` (`float4`) ← the value, `rgb` → `emissiveColor` | (255, 25, 25) | (25, 25, 255) |
| B | `mmd:material:diffuseColor`, `color4f`, a plain custom attribute as stage-contract v1 authors it | as A | (255, 255, 255) | (255, 255, 255) |
| C | `inputs:mmd:material:diffuseColor`, `color3f` | `emissiveColor` ← the value | (255, 25, 25) | (25, 25, 255) |
| D | `inputs:mmd:material:diffuseColor`, `color4f` | `emissiveColor` (`color3f`) ← the value | (255, 25, 25) | (25, 25, 255) |
| E | `inputs:mmd:material:diffuseColor`, `color4f`, alpha 0.25 at frame 2 | fileless `UsdUVTexture`, `fallback` (`float4`) ← the value, `rgb` → `emissiveColor`, `a` → `opacity` | (255, 25, 25, 255) | (25, 25, 255, 0) |

## Findings

- **A plain attribute cannot be a source.** In B, `scale` is connected to
  today's attribute, and Storm ignores the connection without reporting an
  error. The texture then draws with the default `scale` of 1, which is white.
  The same connection to an `inputs:` attribute (A) follows the value on every
  frame. Stage-contract v1's names cannot drive a realization.
- **RGBA reaches a textured preview.** A is the common MMD case: the base
  texture's colour multiplied by diffuse. `color4f` and `float4` are the same
  value type, `GfVec4f`, so the connection is exact, and the whole RGBA factor
  folds into `scale` as the importer folds it today.
- **An untextured preview has no clean form.** `UsdPreviewSurface` has no node
  that splits RGBA into RGB and alpha. D connects `color4f` to a `color3f`
  input, which Storm accepts but UsdShade's types do not promise, and it gives
  no alpha for `opacity`. E follows both RGB and alpha. However, Storm warns
  on every frame that the fileless texture node has no texture (`Could not
  find texture rgb for node preview/diffuse`). Neither is adopted. This is
  MAT-O6
  ([MATERIAL_POLICY.md §13](../design/MATERIAL_POLICY.md#13-open-questions)).
- `/mtlx` is not a problem: `ND_separate4_color4` and `ND_multiply_color4`
  already split and multiply `color4` inside the graph.

## Decision

[MATERIAL_POLICY.md §4](../design/MATERIAL_POLICY.md#4-canonical-material-semantics)
now names the canonical values `inputs:mmd:material:*`, as `usd-vrm-plugins`
does. RGBA colours stay `color4f`, because the common textured case connects
exactly. Values a material morph modulates are varying, and so are the
texture slots. Mode and flag values stay uniform. Moving to new names changes
downstream interpretation, so the stage contract becomes version 2
([STAGE_CONTRACT.md §2](../design/STAGE_CONTRACT.md#2-contract-version)).
