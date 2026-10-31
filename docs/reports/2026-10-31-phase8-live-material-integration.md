# Generated live material integration (2026-10-31)

## Scope and evidence owners

Reconciles the MMD integration status with the runtime and renderer's
2026-10-29 implementation and reruns the focused generated checks against the
source-damping/inert-joint package composition. Continues the
[typed snapshot report](2026-10-27-phase8-material-snapshots.md) without
rewriting its historical scope. No production code, schema, dependency pin or
third-party asset changes are part of this checkpoint.

The runtime's
[session contract](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/architecture/MMD_SESSION.md)
owns direct-PMX material admission and delivery. Its
[material evidence](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/reference/CAPABILITY_MATRIX.md#live-mmd-material-evidence-2026-10-29)
owns the detailed numeric and refusal coverage. The renderer's
[material report](https://github.com/animu-sphere/hydra-toon/blob/main/docs/reports/renderer/111-2026-10-29-live-mmd-materials.md)
owns target matching, draw membership and sampled-tint GPU realization.

Runtime automatically binds stage-contract v2 materials from the directly
opened PMX, requiring canonical paths, applied schema, source indices and
agreement with immutable source baselines. Each material supplies six
canonical inputs and six vec4 multiply/add inputs for independently sampled
texture, sphere and toon contributions: 40 numeric components in 12 inputs.
The runtime calls the installed MMD owner on final channels; the renderer
consumes those values without evaluating morphs or physics.

USD publication covers the six canonical inputs only. The sampled coefficients
are runtime identities, not new USD attributes. The renderer's existing
`--mmd-morph-tints on` selects their full.fx realization; the default retains
its existing untinted policy. This distinction prevents canonical USD parity
from being mistaken for sampled-tint Hydra/direct parity.

## Verification

Windows x64, MSVC 19.51, CMake 4.4, Ninja/Release, OpenUSD 26.08, deterministic
Jolt with six-DOF and Vulkan. Source checkouts were clean before this docs
change: MMD `c59e6d81d668fb0fd8c73d1d3d0b9b09a7076133`, runtime
`6e921e891f73a655b857a2323057a42a5b568e65` and renderer
`6d660d0b985f71d200653dc5d1da275fa1ba9dd9`. The renderer uses the locally
installed runtime/owner closure described in its
[admission report](https://github.com/animu-sphere/hydra-toon/blob/main/docs/reports/renderer/112-2026-10-31-mmd-admission.md#ownership-and-package-composition);
source checkout identities do not imply a newly published package.

Inputs are the runtime-generated two-material PMX/VMD pair and synthetic
texture, produced by `tools/generate-mmd-material-fixture.py` using the MMD
owner's fixture encoders. No private asset is needed.

The runtime material test was enabled in the existing source-damping build
with `AVATAR_MMD_SESSION_TEST_MATERIAL_PMX` and
`AVATAR_MMD_SESSION_TEST_MATERIAL_VMD`. The renderer build already configured
the same generated pair through `TOON_MMD_MATERIAL_TEST_PMX` and
`TOON_MMD_MATERIAL_TEST_VMD`. Use the runtime's
[live-host setup](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/guides/live-mmd-physics.md)
for installed DLL and MMD plugin discovery. From each respective checkout,
with the configured dependencies and Developer PowerShell environment:

```powershell
# usd-avatar-runtime
cmake --build build/damping-session --config Release --target mmdSessionMaterials
ctest --test-dir build/damping-session -C Release --output-on-failure -R '^(hosts.mmd_session_materials|adapters.usd_pose_output)$'

# hydra-toon
cmake --build build/mmd-admission-viewport --config Release --target toon-avatar-state-test toon-mmd-runtime-test toon-viewport-mmd
ctest --test-dir build/mmd-admission-viewport -C Release --output-on-failure -R '^(toon-avatar-state(-gpu)?|toon-mmd-material-resident|toon-viewport-mmd-material|toon-mmd-runtime-cache|toon-viewport-mmd-runtime)$'
```

Both runtime tests and all six renderer tests passed; no test was skipped.
The runtime test compares two materials against installed owner evaluation
over 1440 fixed ticks across three reset generations in each publication mode.
It checks exact replay, zero restoration, Pause, retained snapshots after
destruction, canonical USD float values within `1e-6`, source/session-layer
restoration and refusal of incoherent PMX/stage declarations.

Renderer checks cover automatic resident matching, cached/uncached material
values, reset/replay and generated viewport delivery. The GPU test compares
mapped values against a manually populated renderer reference; the material
viewport test presents 120 generated frames with sampled tints enabled. These
are the bounded checks defined by the renderer's material report, not an
independent Hydra reference or a full-motion private-avatar review.

The first build attempt failed because the local Visual Studio launcher could
not decode discovery JSON and left compiler include paths unset. Direct
`Microsoft.VisualStudio.DevShell` module import and
`Enter-VsDevShell -VsInstallPath ...` resolved that environment failure; the
passing builds used `_CL_=/Z7` as in the admission checkpoint. No owner API
change was needed.

## Remaining gates

Generated automatic material binding, canonical USD publication and sampled
renderer delivery are established within this scope. Broader material
discovery, representative private-asset appearance, additional runtime UV
channels and composed Hydra/direct numeric or pixel agreement remain open.
The scoped native viewport's user decision is recorded separately in the
[native review](2026-10-31-phase8-native-viewport-admission.md).

This checkpoint closes no P1 gate. Continue P1-3 with representative material
assertions and composed-path comparison, and P1-2 with the licensed fixed scene
set and predeclared automated bounds in
[PHYSICS_INTEGRATION §12.3](../design/PHYSICS_INTEGRATION.md#123-representative-scene-acceptance).
