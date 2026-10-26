# Phase 8 private-avatar Hydra2 visual acceptance (2026-10-27)

## Scope and environment

Continues the [generated composed-avatar checks](2026-10-26-phase8-composed-avatar-uv.md)
with one private PMX avatar and its supplied VMD. MMD checkout `07af30b`,
runtime checkout `a76aaf4` and renderer checkout `455ebc4`; Windows x64,
MSVC Release, Python 3.13, OpenUSD 26.08, compatible installed MMD/runtime/Jolt
packages and Vulkan. No production source, public API or dependency pin changed.

The user supplied the inputs for local testing and prohibited committing both
the assets and their names. Source files, identifying names/paths, screenshots,
raw diagnostics, temporary launchers and derived USD remain local and ignored.
No redistribution or acquisition-provenance claim follows from this review.

## Live Hydra2 review

The runtime-owned usdview host opens the original PMX and reads the original
VMD, without a derived motion omitting morph tracks. Installed owner control,
runtime fixed ticks and Jolt produce final pose, vertex-morph and primary-UV
output in a temporary composed USD layer. The current `hdToon` delegate renders
that stage through Hydra2, with MMD schema/imaging plugins registered.

A headless intake first completed 120 ticks with the full input. The actual
window then showed textured character motion with live physics; local status
confirmed advancing ticks and changing final joints. Play, Pause, Restart and
front/side/back controls were made available for local visual review. The
renderer module was rebuilt before review because an older staged module
initially drew only a silhouette.

The user accepted the scoped Hydra2 visual confirmation as complete. This is
a manual decision on the displayed private-avatar path, not an automated
comparison with native MMD or a private-avatar Hydra/direct parity measurement.
The generated stream/pixel comparisons retain their earlier report's scope.

## Dedicated viewport preview

The dedicated `toon-viewport` was rebuilt and opened on a local recording of
the same owner/runtime/Jolt evaluation. All original VMD tracks were passed
to binding. The recording covers 40 seconds, with 4,800 fixed physics ticks
and 2,400 output samples at 60 samples/s; primary UV values changed at 828
sample boundaries. Assertions checked one completed Step per requested tick,
unchanged imported root-layer text, and the reopened animation sample count.
Resolved texture references continue to use the original local files.

The native viewport presented 641 frames with Vulkan and 4x MSAA, reporting
20 MMD materials, 20 skinned MMD draws and 10 textures. This was interactive
USD animation playback with looping, not a live physics session inside the
dedicated viewport. It establishes that the recorded final output is viewable
there; it does not establish native-viewport physics-host integration.

## Limits and follow-up

The original input retains ordinary owner binding diagnostics: unmatched bone
and morph tracks are reported and dropped by the existing matching rules.
Passing the full source does not mean every source track has a target. Both
experimental spring opt-ins remained OFF; unapplied spring metadata warnings
remain. SDEF uses the importer's documented linear-skinning approximation.
This run does not validate material morphs, additional runtime UV channels,
representative contact/constraint bounds or every asset in the private dataset.

The user's sequencing decision places live physics-host confirmation in the
dedicated viewport last, after prerequisite runtime/resident integration.
That final review must use runtime-owned physics ticks and live final output,
with playback/reset/teardown checks, rather than treating this recording as
physics-host evidence. The [current roadmap](../roadmap/current.md#next-integration-work)
owns that ordering. P1-C, P1-2, P1-3 and representative L3 acceptance remain open.

## Reproduction and documentation checks

The runtime's [live-host guide](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/guides/live-mmd-physics.md)
owns installed-package setup and the usdview host. The renderer's
[viewport guide](https://github.com/animu-sphere/hydra-toon/blob/main/docs/guides/BUILDING.md#a-usd-stage-in-the-viewport)
owns native USD playback. Use private inputs locally; the recording/launcher
is not a committed fixture or a reusable public conversion API.

Documentation validation for this report and the owning matrix/roadmap:

```powershell
python scripts/check_docs.py --selftest
python scripts/check_docs.py
```
