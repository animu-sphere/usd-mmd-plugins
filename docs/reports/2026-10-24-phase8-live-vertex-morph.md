# Phase 8 live direct vertex morph output (2026-10-24)

## Scope and environment

This follows the [keyed-input check](2026-10-24-phase8-live-keyed-input.md).
MMD base `f7dbc84` plus generated fixtures; runtime base `038074d` plus live
host/publication changes. Windows x64, MSVC 19.51 Release, Python 3.13.14,
OpenUSD 26.08 with usdview; installed MMD, motion 0.5.4, physics/Jolt and Stage
Runner packages match the preceding check. No owner implementation, dependency
pin or importer contract changes. Both experimental spring options are OFF.

This closes the direct vertex-channel transport gap for one canonical mesh.
It does not close P1-C, representative L3 or the private full-motion intake gate.

## Generated input and owner boundary

`live-physics-morph.pmx` extends the generated contact-free skinned pendulum
with a sparse vertex morph, a group pointing at that vertex morph and a UV
morph. The variant leaves the original pendulum fixture unchanged. Generated
`live-physics-morph.vmd` moves the pivot as before and keys the direct vertex
weight 0 -> 0.5 -> 0 at frames 0, 15 and 30, holding zero through frame 60.
The affected two vertices have a canonical 0.16 m X offset at unit weight.
Separate `live-physics-group.vmd` and `live-physics-uv.vmd` exercise refusals.
All fixture bytes are generated here; no third-party inputs are used.

The live host verifies canonical vertex type, source index, stable identifier
and mesh/target relationships before adding runtime blend-shape slots. Existing
MMD pre/post callbacks consume direct owner channels and write the final weights.
Optional single-mesh `SkeletonPoseOutput` copies those weights by exact
mesh/token identity into the same anonymous default-valued animation as the
physics-driven joints. It performs no morph evaluation and modifies no points.

[MOTION section 11.3](../design/MOTION_CONTRACT.md#113-morphs) preserves raw
non-bone channels, including groups; it does not supply resolved vertex weights
for a group. Runtime must not reinterpret a group scalar as a vertex weight.
Group/flip, UV, material, impulse and hidden outputs remain outside this live
host. Owner-resolved non-bone group execution is a remaining interface need.
The generated group and UV runs refuse with `MMD_ADAPTER_MORPH` and publish no
partial animation. No tracks are stripped to bypass these boundaries.

## Numeric acceptance

Runtime `hosts.usdview_mmd_morph` runs three 240-tick replays at 1/120 s,
using display elapsed 0.0083334 s as in the keyed-input check. Assertions are
declared before execution: finite body state, direct weight error below `1e-6`,
final vertex error below `2e-5` m and exact reset replay. Composed UsdSkel
queries apply blend-shape deformation before skinning; the independent expected
points use the authored sparse delta, keyed weight and concrete solver rotation
about the animated pivot.

| Assertion | Observed |
| --- | --- |
| Direct vertex weight | 0.5 at 0.5 s; zero at 1 s and held through 2 s |
| Maximum composed deformation/skinning error | 3.50935e-7 m |
| Reset replay | Weights, deformed/skinned points and body states exactly equal at every tick in all three runs |
| Pause at nonzero weight | No ticks or body/weight changes |
| Reset / Close | Original session/persistent layer text restored; source points unchanged; no weight time samples |
| Unsupported group / UV | Explicit owner-adapter refusal; no output layer published |

Source and installed USD consumer checks additionally cover reordered runtime
slots, unrelated shapes, unclamped negative/above-one values, missing/duplicate
identity, nonfinite/overflow weights, changed mesh/skeleton/target binding and
refusal preserving the previous live layer. The existing pose-only output
remains covered. All 18 configured runtime CTests pass; MMD fixture/tool/stage
checks and documentation checks also pass.

## Reproduction and remaining work

Follow the runtime [live-host guide](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/guides/live-mmd-physics.md#validation):
generate the current VMD fixture set, configure the existing pendulum/anchor
options plus `AVATAR_MMD_PREVIEW_MORPH_PMX` to the generated morph model, build
and run `hosts.usdview_mmd_morph`. Runtime consumes installed owner packages;
the MMD checkout supplies generated inputs only.

No actual-window morph pixel check or private asset rerun is claimed. Full
non-bone execution/publication, admitted representative damping declarations,
asset provenance/permission records, representative contact/constraint/recovery
bounds, resident Toon resources and Hydra/direct agreement remain required.
