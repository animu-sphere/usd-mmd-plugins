# Phase 8 live primary UV transport (2026-10-25)

## Scope and environment

This follows the [owner UV query](2026-10-25-phase8-uv-morph-evaluation.md).
MMD base `c7fa509` plus generated fixture changes; runtime base `3d4ef74` plus
typed primary UV snapshots, adapter and live USD output; Toon base `deb1f79`
plus a scoped ABI-5 unsupported-output refusal. Windows x64, MSVC 19.51
Release, Python 3.13.14, OpenUSD 26.08 with usdview, and the existing installed
motion 0.5.4, MMD, physics/Jolt and Stage Runner packages. The installed MMD
control library and headers were rebuilt together. Spring opt-ins remain OFF.
No importer, owner evaluation algorithm, dependency pins or authored-stage
contract changes; no third-party assets are used.

## Owner, runtime and consumer boundary

The post-physics adapter separately admits vertex and primary UV leaves from
owner-resolved channels. It calls installed `EvaluateUvMorphs` on raw channels,
adds XY deltas to immutable canonical bases, validates complete mesh/vertex
bindings and finite float-representable final values, and writes absolute
coordinates to retained runtime snapshots. Group resolution and primary-V
conversion occur only in the owner. Primary Z/W do not affect `st`.
Reached unmapped leaves still refuse, including zero-weight material leaves;
additional UV realization is outside this slice. Post-Step failure publishes
no partial snapshot and requires world reset under the existing lifecycle.

Experimental runtime ABI 5 adds `ArVertexUv`, the state array and its
deformation-domain writer. Core copies identities and values, rejects invalid
arrays/duplicate identities/nonfinite bases, starts from baseline each frame,
and retains old outputs across reset and producer destruction. ABI 1–4
negotiation refuses; providers and consumers must rebuild together. The
[runtime state contract](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/contracts/EVALUATED_STATE.md#primary-vertex-uvs)
owns these semantics. Source and installed MMD package lookup probe both UV
headers and linked symbols.

The live host checks primary canonical bases against the directly imported
mesh's unindexed vertex `st`. The optional USD output validates density,
topology, primvar binding and narrowing before staging joints, blend-shape
weights and `st` in one scratch layer. One anonymous session sublayer then
publishes all three; Reset/Close restore previous composition. The Toon direct
adapter can build with ABI 4 or 5 headers and requires the matching binary
revision. Its ABI-5 path explicitly refuses nonempty UV output pending renderer
bindings; it does not silently drop the new state domain.

## Generated checks

The generated pendulum model retains its vertex/group leaves and now adds
primary XY offsets with nonzero Z/W, a mixed UV/vertex group, and an unsupported
material leaf. Direct UV keys follow `0 -> 1.5 -> -0.5 -> 0`. Mixed keys combine
group weight `2w` and direct UV weight `-0.5w`, producing UV weight `0.5w`
and vertex weight `w` without double group expansion.

Each scenario runs 240 ticks at 1/120 s for three reset generations. Every tick
checks live `st`, simultaneous vertex weights and finite solver states. Pause
holds the nonzero UV frame; weights, UVs and body states reproduce exactly.
Returning to zero, Reset and Close restore immutable bases and original
persistent/session layers. Maximum primary coordinate error is `2.78155e-8`.
These are composed USD numeric checks, without a window or pixel evidence.

Source and installed adapter checks additionally cover nested diamond/direct
weights (1.875), reordered vertex slots, primary V conversion, zero return,
retained generations, malformed reached-zero offsets and additional-channel
refusal. Core checks cover deep copying, absent writes returning to baseline,
writer failure latching, invalid values/arrays/identities, write permissions,
reset and retention after runtime destruction. USD output checks cover missing,
duplicate and invalid vertices, nonfinite/overflow values, topology and primvar
edits, stronger session opinions and atomic refusal preserving the last layer.

All 21 CTests in the live-host configuration pass across the source and installed
consumer runs. The Toon `toon-avatar-state` CPU test passes with ABI-5 source
headers, including explicit UV refusal and reference cleanup. Its pre-existing
unrelated working-tree edits were retained; no GPU claim follows from that test.
The MMD Release workspace build and all 52 CTests also pass, including fixture
regeneration, stage/Hydra preservation, documentation and installed consumers.
Documentation selftest, relative links/anchors and the 77-code catalog pass;
changed runtime/Toon Markdown relative links were checked with the same checker.

## Reproduction and remaining work

Use the runtime [live-host guide](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/guides/live-mmd-physics.md#validation)
with the launcher's installed dependency DLL/plugin environment, the generated
pendulum/model paths and current VMD fixture generator. Reinstall current MMD
control and rebuild the runtime first. The full runtime suite is:

```powershell
ctest --test-dir build/usdview-mmd -C Release --output-on-failure
```

For the focused check, use `-R '^hosts.usdview_mmd_(morph|uv)$'`. The guide owns
the configure/generator commands. On this host MSBuild uses one node with node
reuse disabled and a writable temporary directory.

Additional UV/vertex-color output, material realization, actual rendered UV
pixels, resident Toon UV bindings, Hydra/direct agreement, representative
PMX/VMD L3 acceptance and P1-C remain open. This scoped transport does not close
P1-3 or claim complete avatar acceptance.
