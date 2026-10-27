# Source damping response and unit endpoint (2026-10-27)

## Scope

Private-avatar intake encountered finite source damping coefficients 1 and 2.
The existing strict stage reader refuses both an out-of-domain declaration and
the unit endpoint that cannot be represented by its finite inverse-second rate.
A local preview uses explicit temporary opinions of 0.99; that is an approximation,
not formal endpoint support. Original assets and identifying paths remain local.

## Source-policy resolution

The new owner-side `ResolveSourceDamping(float)` returns an owned typed response:
finite coefficients are clamped to [0,1], with an explicit `clamped` indication;
values below one resolve to `-log1p(-d)`; one resolves to
`SourceDampingMode::ClearPriorVelocity`, with no fabricated large rate. Nonfinite
input refuses. The package exposes `mmdPhysicsAdapter_SOURCE_DAMPING_VERSION=1`.

This follows the pinned
[Bullet 3.25 coefficient clamp and exponential damping](https://github.com/bulletphysics/bullet3/blob/3.25/src/BulletDynamics/Dynamics/btRigidBody.cpp#L123-L143)
used by the referenced
[three.js r168 MMD adapter](https://github.com/mrdoob/three.js/blob/r168/examples/jsm/animation/MMDPhysics.js).
The endpoint removes prior velocity; it does not freeze a body against new
forces or constraint impulses. Finite negative source coefficients normalize
to zero in this resolver. Malformed USD declarations still refuse in the reader.

`ReadBodyProperties` retains its strict version-1 behavior. The new resolver is
not yet selected by the live host, and it does not change imported metadata,
generic physics descriptors or backend stepping. Its unit test passes alongside
the existing reader refusals: negative/zero/interior/nearest-below-one/unit/above-one
values, independent analytical decay at 60/120/240 Hz, and nonfinite refusal.

## Remaining integration

The backend needs an explicit neutral representation for clearing prior linear
and/or angular velocity at a step boundary, with independent component control.
The runtime must opt into the source policy and report normalization, preserve
body paths and original declarations, and repeat admission on Reset.

Do not substitute a huge finite rate in Jolt:
[its force/drag implementation](https://github.com/jrouwe/JoltPhysics/blob/v5.5.0/Jolt/Physics/Body/MotionProperties.inl#L114-L133)
adds force/gravity before applying drag, while the selected source policy removes
prior velocity before the subsequent force/constraint response. Test both
zero-force decay and nonzero force/gravity/constraint response, reset replay,
linear/angular independence, and static/kinematic behavior before enabling it.

Backend/runtime integration and a private-input rerun without the damping
preview layer remain open. No full MMD trajectory or representative L3
equivalence is established by this source-policy slice.
