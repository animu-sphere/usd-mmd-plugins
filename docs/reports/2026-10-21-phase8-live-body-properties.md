# Phase 8 live MMD body properties (2026-10-21)

## Scope and ownership

Extends the [first live usdview path](2026-10-21-phase8-usdview-live.md).
The installed optional `mmdPhysicsAdapter` now owns `ReadBodyProperties`:
MMD collision admission bits, contact coefficients and source-to-rate damping
conversion. The returned owned optionals introduce no physics/backend/runtime
dependency. The importer and generic physics reader remain unchanged.
[PHYSICS section 7](../design/PHYSICS_INTEGRATION.md#7-collision-filtering-and-joints)
owns semantics; the
[package contract](../architecture/PACKAGE_CONTRACT.md#mmdphysicsadapter)
owns the feature-version declaration.

The runtime host maps those results to installed neutral descriptors and
selects the owner's product mixing policy, with friction capped at 10.
World reconstruction rereads properties. Owner refusal codes and body paths
survive the runtime's generic callback-failure diagnostic. Joint spring
metadata remains explicitly unapplied. This is a bounded generated live-host
extension, not representative hair/skirt, native MMD or avatar L3 acceptance.

MMD base `c11f274`, runtime base `80246f4`, plus local changes in each owner.
Windows x64, MSVC 19.51 Release, Python 3.13.14, Python/Qt-enabled OpenUSD 26.08,
and the preceding live host's installed physics/Jolt/clock/motion packages.
The rebuilt MMD package is installed before building runtime; no sibling source
is compiled into the runtime host. No published dependency or hosted CI claim.

## Results

The standalone owner package passes three CTests: existing coupling, new body
properties and dependency boundaries. Property tests cover all sixteen groups,
zero/high-bit/full masks, friction above one, zero/independent/near-one damping,
absent pairs, partial pairs, missing/blocked values, wrong types, samples,
varying attributes, invalid domains and unit damping. Reads leave stage text
unchanged. Fatal owner codes distinguish invalid metadata from unsupported
instantaneous stopping.

The composed MMD build also passes all six selected coupling/property/boundary,
fixture and clean-prefix installed-consumer tests. Its installed `physics_probe`
now calls the property API against the generated all-mode PMX, confirming header,
symbol and optional-property availability outside the source tree.

The live runtime configuration passes fifteen CTests, including installed
adapter consumers and `hosts.usdview_mmd_properties`. That new test modifies
only an in-memory session layer over the redistributable pendulum, disables
its joint and uses the existing kinematic body as a floor. This isolates contact
from joint collision suppression:

- Admitted drops settle within 3 cm of the expected 0.18 m centre height and
  below 0.05 m/s vertical speed. A zero mask or mask omitting the floor's group
  falls below -10 m after two seconds; the exact admitting bit restores contact.
- Source restitution 0.8 yields upward speed above 2 m/s; zero restitution
  remains below 0.1 m/s.
- A ten-degree incline with friction zero yields horizontal speed more than
  0.5 m/s greater than friction one.
- Source linear damping 0.75 reduces the one-second free-fall speed below
  65 percent of the undamped control.
- Angular damping 0.999 reduces the constrained pendulum's quaternion Z
  magnitude after 0.25 seconds from about 0.194677 to 0.170222. This checks a
  response difference under continuous gravity/constraint impulses, not free-spin
  exponential equivalence.
- Invalid restitution and unit damping refuse both construction and Reset
  with the original owner code. Failure leaves live output unmodified.
- Copied body inspection values survive teardown; close restores the preceding
  session-layer text. The existing three identical 120-tick live pose replays
  still pass, including pause/reset and skinned-vertex movement.

The actual-window usdview test also passes menu Play/Pause/Reset/resume/Stop,
stage replacement and composed UsdSkel rotation. Its warning now identifies
only the two preserved joint spring attributes.

## Reproduction

With the installed MMD and Python-enabled OpenUSD prefixes from the
[live-host guide](https://github.com/animu-sphere/usd-avatar-runtime/blob/main/docs/guides/live-mmd-physics.md),
run from this checkout:

```powershell
cmake -S libs/mmdPhysicsAdapter -B build/live-body-properties -A x64 `
  "-DCMAKE_PREFIX_PATH=$mmdInstall;$usd" "-DCMAKE_INSTALL_PREFIX=$mmdInstall"
cmake --build build/live-body-properties --config Release
ctest --test-dir build/live-body-properties -C Release --output-on-failure
cmake --install build/live-body-properties --config Release
```

Rebuild the runtime's existing `build/usdview-mmd` configuration using that
installed prefix. Under the guide's DLL/plugin environment, run its full CTest
suite and optional actual-window command. `AVATAR_MMD_PREVIEW_PMX` remains the
generated `live-physics-pendulum.pmx`; no fixture bytes or licensed assets change.

## Remaining boundary

Springs, full PMX/VMD intake, runtime non-bone morphs, multi-avatar automatic
binding, seek/source edits, resident Toon integration and Hydra/direct agreement
remain open. Damping endpoint one is unsupported; accepted damping and contact
responses remain backend approximations. Mixed-format category and world contact
policy selection are not established by this single-format host.
P1-C and P1-2 stay open in the [roadmap](../roadmap/current.md).
