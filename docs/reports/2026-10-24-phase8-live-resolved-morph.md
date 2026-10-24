# Phase 8 live owner-resolved vertex groups (2026-10-24)

## Scope and environment

This follows the [owner scalar API](2026-10-24-phase8-resolved-morph-channels.md)
and [direct vertex publication](2026-10-24-phase8-live-vertex-morph.md).
MMD base `4403f1f` plus a generated mixed VMD fixture; runtime base `4d0d108`
plus adapter, package checks and live-test integration. Windows x64, MSVC 19.51
Release, Python 3.13.14 and OpenUSD 26.08 with usdview. The existing installed
motion 0.5.4, MMD, physics/Jolt and Stage Runner packages supply the host;
MMD control was rebuilt and reinstalled with `ResolveMorphChannels`.
Both experimental spring options remain OFF. No dependency pins, importer,
authored-stage contract, owner algorithms or third-party assets change.

## Owner/runtime boundary

The runtime MMD post callback resolves the final pose's raw channels through
installed `Evaluator::ResolveMorphChannels`, then uses only those leaf weights
for output validation and publication. It does not expand groups itself or
add raw direct weights again. Bone effects remain solely in the evaluated pose.
The existing live host validates vertex source identity/type and the authored
mesh/shape binding; it needs no new group-to-shape mapping.

Negative/above-one weights remain unclamped. Bound leaves absent from the
resolved result write zero. Non-finite results and unmapped reached leaves
refuse before output writes, including unsupported leaves reached at zero.
The runtime publishes no partial snapshot; post-Step refusal requires world
reset under the existing lifecycle. Source and installed MMD component lookup
probe both owner headers and linked symbols. An older installed control
package was explicitly checked and refuses configuration with a rebuild
instruction. Core-only installed lookup remains independent of MMD.

## Generated acceptance

The unchanged `live-physics-morph.pmx` supplies one vertex shape, a group with
coefficient 0.5 and an unsupported UV morph. The live test now checks three
generated VMDs, each for 240 ticks at 1/120 s and three exact reset replays:

| Input | Expected vertex output |
| --- | --- |
| Existing direct keys | 0 -> 0.5 -> 0, holding zero after 1 s |
| Existing group-only key | Constant 0.5 from group weight 1 |
| New mixed keys | Group peak 2 and direct peak -0.5 resolve to 0.5; return to zero |

The direct and mixed inputs retain the animated pivot. Every replay checks
finite body state, weight error below `1e-6`, composed UsdSkel blend-shape
deformation followed by skinning against the concrete body rotation below
`2e-5` m, paused output, unchanged source points, default-only animation,
and session/persistent layer restoration on Reset/Close.
The maximum observed deformation/skinning error across all three scenarios
is `4.16318e-7` m. Weights, points and body states repeat exactly at every tick.
UV still refuses with `MMD_ADAPTER_MORPH` and no output layer.

Source and installed adapter checks additionally exercise a nested diamond
with simultaneous direct input (known result 1.875), group bone translation
applied once, negative/zero/absent output, retained snapshots and pause/reset.
Unmapped UV/material/flip/impulse leaves refuse even through zero group/member
weights, and non-finite group results refuse explicitly.
All 18 configured runtime CTests pass. After adding installed-package API
admission, core installed, MMD installed and live morph checks pass again.
MMD control unit, VMD tool fixtures and generated PMX fixtures pass.

## Reproduction and remaining gates

Use the runtime [live-host guide](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/guides/live-mmd-physics.md#validation)
for installed dependencies and plugin/DLL environment. Rebuild/install MMD,
regenerate the VMD fixture directory, keep the existing pendulum/anchor/morph
options, then build the runtime host and run:

```powershell
ctest --test-dir build/usdview-mmd -C Release --output-on-failure
```

This closes the generated vertex/group transport slice. It does not close
P1-C, private full-motion intake or representative L3. UV/material/flip/impulse
realization, admitted representative damping, local asset provenance/permission,
contact/constraint/recovery bounds, actual-window morph pixels, resident Toon
resources and Hydra/direct agreement remain open.
