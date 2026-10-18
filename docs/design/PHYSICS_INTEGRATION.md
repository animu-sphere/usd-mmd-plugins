# Physics integration contract (MMD-specific boundary)

> Contract: **accepted**. Sections 1–8 define the MMD boundary and conversion
> rules; §12 owns physics fidelity and acceptance policy. The coupling API in
> §5–§6 is an intended runtime contract, not a support claim.
> Current availability is recorded only in the
> [capability matrix](../reference/CAPABILITY_MATRIX.md#physics-preservation-and-execution).
> Static authoring remains binding under [STAGE_CONTRACT §13](STAGE_CONTRACT.md#13-physics).

## 1. Scope

`usd-mmd-plugins` is an MMD format adapter. For physics it owns:

- parsing PMX rigid bodies and joints;
- normalizing their coordinates, units, source identity and MMD semantics;
- authoring the binding representation in
  [STAGE_CONTRACT.md §13](STAGE_CONTRACT.md#13-physics); and
- adapting the MMD-only bone/body coupling rules for a runtime.

It does not own a solver, fixed-timestep loop, collision-query system or
backend integration. Those are shared physics/runtime concerns.

```text
PMX rigid bodies and joints
        │
        ▼
MMD semantic normalization
        │
        ▼
UsdPhysics + mmd:physics:* preservation metadata
        │
        ▼
usd-physics-plugins
        │
        ▼
physics backend
```

Parsing and authoring stay here. Numerical simulation stays outside this
repository, and opening or inspecting a PMX never requires a physics runtime.

## 2. Ownership boundary

| Concern | Owner |
| --- | --- |
| PMX body/joint syntax, source ordering and source names | `mmdPmx` |
| MMD modes, bone association, group/mask rules, basis and unit conversion | `mmdModel` |
| Conventional `UsdPhysics` plus exact `mmd:physics:*` preservation | `usdMmdFileFormat` |
| MMD bone-to-body and body-to-bone coupling | an MMD-specific runtime adapter owned here |
| Physics scene construction, stepping, queries and backend abstraction | `usd-physics-plugins` |
| Backend objects and backend-specific collision layers | the selected physics backend integration |
| Per-frame ordering | `usd-stage-runner` |
| Cross-repository composition and playback state | `usd-avatar-runtime` |
| Generic MotionClip, retargeting and UsdSkelAnimation processing | `usd-motion-plugins`; MMD IK interpretation stays here |
| MMD / MToon / PreviewSurface rendering | `hydra-toon` |

A useful placement test is:

> Would this code still make sense if PMX did not exist?

If yes, it belongs in the shared physics/runtime layer. If no, it remains on
the MMD side. PMX modes, collision masks and bone feedback are MMD concerns;
world stepping, collision queries and backend body creation are not.

## 3. The authored stage is the hand-off

The binding stage representation is already defined by
[STAGE_CONTRACT.md §13](STAGE_CONTRACT.md#13-physics):

```text
/Asset/physics
  rigidBodies/<id>       UsdGeomXform + PhysicsRigidBodyAPI + PhysicsMassAPI
    collider             sphere, cube or capsule + PhysicsCollisionAPI
  joints/<id>            PhysicsJoint where the PMX joint maps faithfully
```

Every PMX value is also recoverable through the established
`mmd:physics:*` attributes. In particular:

- `mmd:physics:bone` preserves the canonical joint association;
- `mmd:physics:mode` is `followBone`, `dynamic` or `dynamicWithBone`;
- `mmd:physics:collisionGroup` and `collisionMask` preserve PMX filtering;
- body coefficients preserve damping, restitution and friction; and
- joint metadata preserves source type, bodies, frames, limits and springs.

These names are part of stage-contract version 1. A future runtime integration
consumes them; it does not introduce aliases such as `mmd:rigidBodyMode`.
Backend namespaces such as `jolt:*`, `physx:*` or `bullet:*` are never authored
by the importer.

## 4. Conventional OpenUSD first

The importer continues to use standard schemas wherever they faithfully state
the physical meaning:

- `UsdPhysicsRigidBodyAPI`, `UsdPhysicsCollisionAPI` and `UsdPhysicsMassAPI`;
- `UsdPhysicsJoint` plus per-axis `UsdPhysicsLimitAPI` for PMX 6-DOF joints;
- `UsdGeomSphere`, `UsdGeomCube` and `UsdGeomCapsule` for colliders.

The importer does not create an MMD rigid-body schema that duplicates those
properties. MMD metadata exists only for meaning that standard OpenUSD cannot
state exactly. The detailed mapping and fidelity gaps remain owned by
[STAGE_CONTRACT.md §13](STAGE_CONTRACT.md#13-physics), not repeated here.

## 5. Rigid-body modes

PMX's modes describe coupling between animation and simulation, not three
different solver body types to bake into the importer.

### 5.1 `followBone`

```text
evaluated bone pose → kinematic rigid body
```

The runtime adapter updates the body from the current pose before the physics
step. The body does not drive the bone.

### 5.2 `dynamic`

```text
physics step → rigid-body transform
```

The body is controlled by physics. There is no bone feedback for this mode.

### 5.3 `dynamicWithBone`

```text
evaluated pose → physics step → MMD body-to-bone feedback → final pose
```

The importer preserves the mode and association. The MMD adapter computes the
feedback; the shared physics layer supplies simulated body transforms and does
not learn MMD's convention.

## 6. Runtime order

`usd-stage-runner` owns execution order. `usd-avatar-runtime` composes the
participants and chooses playback state; neither re-implements MMD semantics.

```text
VMD or shared motion input
        │
        ▼
MMD control evaluation / generic retarget
        │
        ▼
runtime pose
        │
        ▼
MMD bone → body synchronization
        │
        ▼
usd-physics-plugins step
        │
        ▼
MMD body → bone feedback
        │
        ▼
final runtime pose → UsdSkel / renderer consumer
```

The static importer is not in this loop. The MMD coupling adapter may be a
plain library in this repository, but it owns no thread, clock or solver. Its
exact component identity is added to
[WORKSPACE.md](../architecture/WORKSPACE.md) only when the first runtime
consumer makes its API concrete.

## 7. Collision filtering and joints

PMX group/mask values remain source semantics. The runtime adapter translates
them into the shared physics filter contract; the selected backend then maps
that contract to object layers or its equivalent. The USD file never freezes
a Jolt, PhysX or Bullet filtering strategy.

For the existing stage contract's constant integer metadata, the neutral
filter uses `categories = 1 << collisionGroup` and
`collidesWith = collisionMask`, limited to the 16 MMD groups. A set mask bit
admits that group; there is no complement or sign extension. A pair is
admitted only if both bodies accept the other's category. Bodies in the
same group retain their individual masks. An incomplete, out-of-range,
wrongly typed or time-sampled MMD filter declaration must reject instead
of silently using backend defaults. A body with neither MMD attribute may
retain the shared default. This mapping does not reserve categories for
non-MMD participants in a composed runtime; that allocation remains a
runtime contract.

Joint translation follows the same rule:

1. consume the closest conventional `UsdPhysics` representation;
2. read `mmd:physics:*` for source meaning that representation cannot express;
3. report the fidelity gap; and
4. enable enhanced behavior only when the runtime/backend supports it.

Initial runtime support may start with connected bodies, frames and basic
limits. Angular/translational limits, springs and drive-like behavior follow
incrementally. Perfect one-to-one backend behavior is not a prerequisite for
the first integration, but silent loss of source meaning is forbidden.

### 7.1 Contact coefficients

The isolated consumer reads the existing constant `float` attributes
`mmd:physics:friction` and `mmd:physics:restitution` into the shared core's
neutral `ContactMaterial`. Friction must be finite and non-negative;
restitution must be finite and in `[0,1]`. A partial pair, wrong type,
missing/blocked value or time sample rejects. A conventional body with neither
attribute retains the neutral defaults. Values are copied without clamping,
and backend scalar limitations can reject at body creation.

The consumer selects neutral per-world `ContactMixing`: products for both
coefficients, with combined friction capped at 10. This follows the default
non-negative coefficient policy in
[Bullet 3.25](https://github.com/bulletphysics/bullet3/blob/3.25/src/BulletCollision/CollisionDispatch/btManifoldResult.cpp#L48-L63).
Jolt's no-argument world factory keeps its geometric-mean friction and maximum
restitution defaults. Policy selection stays outside the generic USD reader;
no solver-specific properties or USD material bindings are authored.
This is a scalar policy reference, not native MMD trajectory evidence.
Mixed-format policy selection remains a runtime decision.

### 7.2 Body damping

The isolated consumer reads constant `float` attributes
`mmd:physics:linearDamping` and `mmd:physics:angularDamping`. A partial pair,
wrong type, missing/blocked value, time sample or non-finite coefficient
rejects. Conventional bodies with neither attribute keep shared defaults.

The source reference is three.js r168's
[MMD adapter](https://github.com/mrdoob/three.js/blob/r168/examples/jsm/animation/MMDPhysics.js),
which passes both coefficients to Bullet's `setDamping()`.
[Bullet 3.25's default damping](https://github.com/bulletphysics/bullet3/blob/3.25/src/BulletDynamics/Dynamics/btRigidBody.cpp)
multiplies velocity by `(1-d)^dt`. The consumer accepts `0 <= d < 1` and
converts each independently to the neutral inverse-second rate `-log1p(-d)`.
It rejects out-of-domain values without clamping. The valid source endpoint
`d=1` requires instantaneous stopping and rejects explicitly as unsupported
by finite rates. This bounded conversion does not claim native MMD equivalence.

[Jolt 5.5.0](https://github.com/jrouwe/JoltPhysics/blob/v5.5.0/Jolt/Physics/Body/MotionProperties.inl)
approximates exponential decay by `max(0, 1-rate*dt)` per substep, after force
integration. Force ordering and finite steps limit equivalence; response
measurements belong in the [damping report](../reports/2026-10-18-phase8-body-damping.md).
The importer and generic USD reader do not change.

### 7.3 Joint spring metadata audit

The isolated consumer reads the preserved `translationSpring` and
`rotationSpring` pair as constant `vector3f` values. Partial pairs, wrong
types, missing/blocked values, time samples and non-finite components reject.
Finite signed values remain verbatim in the audit; the independent execution
options in §7.4–§7.5 do not alter them. The audit includes format-only joint prims outside the shared
snapshot, without interpreting source-type activation rules.

Nonzero pairs are reported with the input, prim path and both vectors.
Snapshot rows count affected prims and components and mark joint fidelity
`approximate` when nonzero coefficients or format-only joints are present;
otherwise it remains `unverified`. Snapshot acceptance and finite stepping
do not establish spring fidelity. Approximate execution follows §7.4–§7.5;
the [audit report](../reports/2026-10-18-phase8-joint-spring-audit.md) records
the preservation evidence separately.

### 7.4 Experimental translation spring approximation

The isolated execution probe may additionally enable
`MMD_PHYSICS_TRANSLATION_SPRINGS=ON` (default OFF, requires execution mode).
Only a constant `token` type `spring6Dof` activates positive translation
coefficients on free or limited axes of a standard joint. Missing/other types,
negative coefficients, locked axes, rotations and format-only joints do not
gain spring execution. Malformed type metadata rejects. The source audit
continues to report every nonzero source coefficient as a source-fidelity gap.

The adapter copies translation stiffness into the neutral SI spring, with
zero equilibrium and zero viscous damping. With mass/time unchanged and
length scaled by `s`, force scales by `s`, so `s*k*x = k*(s*x)`: translation
stiffness retains its numeric value. This is dimensional inference, supported
by independent source-to-SI oscillator controls, not native MMD evidence.
The three.js r168 adapter passes source coefficients directly to Bullet's
6-DOF spring motor. Bullet's dimensionless default spring damping and iterative
motor algorithm do not define an equivalent viscous coefficient; none is
invented here. Body damping remains independent.

Success output names `translationSpringModel=experimentalHookean` and counts
`approximateTranslationSpringAxes`; per-prim diagnostics state the approximation.
`unsupportedSpringJoints`/`unsupportedSpringAxes` retain their source-fidelity
meaning even for approximated axes, and `jointFidelity` stays `approximate`.
Numerical damping and finite-step behavior limit equivalence; see the
[translation response evidence](../reports/2026-10-18-phase8-translation-springs.md).
These limits follow §12's acceptance policy rather than creating a source-trajectory gate.

### 7.5 Experimental rotation spring approximation

`MMD_PHYSICS_ROTATION_SPRINGS=ON` is a separate default-OFF execution option.
It applies positive rotation coefficients only to free/limited axes of standard
`spring6Dof` joints. Type validation follows section 7.4. Negative, locked and
format-only coefficients remain unexecuted. The source audit retains all
nonzero coefficients as fidelity gaps.

Under the fixed source length conversion `s=0.08`, with mass/time unchanged,
rotational inertia and torque scale by `s^2`, while radians do not change.
Thus the small-angle SI stiffness is inferred as `k_source * 0.0064` N*m/rad.
Zero viscous damping and identity equilibrium are selected. A sign change of
an angular coordinate under reflection changes both torque and angle, so it
does not negate diagonal stiffness. Neither conversion proves native MMD units
or solver equivalence.

The consumer fills neutral `rotationSprings`; shared code knows no MMD names.
The backend's second-frame quaternion position motors are a small-angle
approximation, distinct from Bullet's Euler spring motors and dimensionless
damping. Body damping remains independent. The
[rotation report](../reports/2026-10-18-phase8-rotation-springs.md)
owns source-to-SI oscillator measurements.

Output names `rotationSpringModel=experimentalSmallAngle` and counts
`approximateRotationSpringAxes`, with per-prim approximation diagnostics.
`unsupportedSpringJoints`/`unsupportedSpringAxes` and `jointFidelity` keep their
source-fidelity meanings. Finite-angle/multi-axis and source-trajectory
equivalence are outside this approximation's claim; §12 governs whether
further work is needed for representative avatar behavior.

## 8. Dependency policy

The static path remains lightweight:

```text
usd-mmd-plugins
├─ parser and canonical model
├─ file-format plugin and stage contract
└─ optional MMD runtime adapter
       └─ usd-physics-plugins
```

`mmdPmx`, `mmdModel` and `usdMmdFileFormat` never depend on
`usd-physics-plugins` or on a backend. Any future adapter consumes
`usd-physics-plugins` as an installed package with a declared compatible
version, never as sibling source or vendored code. The exact dependency is
reserved in [DEPENDENCIES.md §7](../architecture/DEPENDENCIES.md#7-usd-physics-plugins).

## 9. Implementation sequence

1. **Confirm the cross-repository stage contract.** Build a consumer against
   the existing `/Asset/physics` layout, units, frames and metadata. Change
   stage-contract version 1 only if the consumer proves a defect.
2. **Consume generic physics.** Translate the authored `UsdPhysics` scene into
   `usd-physics-plugins` without MMD bone coupling.
3. **Synchronize `followBone`.** Drive kinematic bodies from the evaluated
   pose before each step.
4. **Run dynamic bodies.** Simulate `dynamic` bodies and constrained pairs.
5. **Add `dynamicWithBone`.** Apply MMD-specific body feedback to the runtime
   pose outside the importer.
6. **Refine only for acceptance.** Improve limits, springs or damping when a
   representative L3 failure or an explicit external contract requires it;
   generic solver changes belong in `usd-physics-plugins`.

Static preservation was completed in Phase 6. These runtime steps belong to
Phase 8 composition and its external collaborators; they do not reopen Phase
6 unless the authored contract itself is wrong.

The [dated evidence map](../reports/2026-10-18-docs-ownership-audit.md#retained-evidence-and-unverified-signals)
links reader, SDK, contact, damping and spring experiments.
Current work is tracked only in [current.md](../roadmap/current.md).
Further joint refinement is justified by §12's acceptance criteria, not by
an unbounded requirement to reproduce a source solver.

## 10. Testing

Parser/model tests verify source values and conversions without a runtime.
Stage tests verify APIs, relationships, meters, Y-up, deterministic paths
and exact preservation; contract tests reconstruct bodies/joints from the
stage alone. Backend tolerances belong in shared backend tests.

| Lane | Acceptance | PR blocking policy |
| --- | --- | --- |
| fast | Parser, metadata, schema, conversion units and doc links | Required |
| integration | Installed shared reader/execution smoke, finite state and repeatability on representative generated fixtures | Required for the supported configuration once wired into CI |
| acceptance | PMX + VMD → motion → physics → feedback under §12.3 | Small stable automated scenes should block once available |
| research/manual | Bullet comparisons, long/high-resolution runs, large corpus sweeps and visual review | Nonblocking |

This table defines policy, not a claim that all lanes already run.
The current isolated probe wiring is in
[tests/physics_consumer/CMakeLists.txt](../../tests/physics_consumer/CMakeLists.txt);
CI adoption and avatar acceptance are tracked in [current.md](../roadmap/current.md).
With an installed MMD prefix, the probe registers representative PMX assertions;
execution replays a fresh world and compares every exposed transform and linear
velocity at every fixed step. This is same-build numerical repeatability, not
cross-platform or source-solver equivalence. The
[build guide](../guides/building.md#optional-physics-consumer) owns enablement
and the opt-in full-corpus command.

Preserve the generated corpus as regression evidence; do not require every
fixture × platform × timestep combination on every PR. Keep spring-OFF
regressions and spring-ON smoke. Use fixed timesteps, deterministic recorded
input and pinned build configurations; live devices, networking and wall-clock
timing are not prerequisites. Stability regressions block; a tolerance
violation caused only by a known Bullet trajectory difference does not.

## 11. Non-goals

This repository does not become a physics framework, backend wrapper, game
runtime, scheduler, renderer or editor. It does not duplicate shared physics
worlds, stepping, queries or backend object creation.

The intended end state is:

```text
PMX → usd-mmd-plugins → OpenUSD stage
                         ├─ UsdSkel
                         ├─ UsdPhysics
                         └─ minimal MMD metadata
                                │
                  usd-motion-plugins + usd-physics-plugins
                                │
                       usd-stage-runner
                                │
                       usd-avatar-runtime
```

MMD remains specialized but participates in the same reusable motion and
physics architecture as other avatar formats.

## 12. Fidelity and acceptance policy

This section owns the project's physical success criteria. Preserve MMD's
meaning and achieve stable, plausible avatar behavior through shared physics;
do not grow a generic solver or a Bullet reproduction inside this repository.

### 12.1 Fidelity levels

| Level | Success criterion | Requirement |
| --- | --- | --- |
| L1 — Preserve | Retain source semantics and values; expose loss, repairs and unsupported meaning | Required |
| L2 — Stable | Execute the supported bodies/constraints with finite state and repeatable results under a declared deterministic configuration | Required |
| L3 — Plausible | Representative avatars show expected sway, contacts and bone recovery without visible or measured breakdown | Target for avatar composition, verified under §12.3 |
| L4 — Bullet-equivalent | Reproduce Bullet/MMD trajectories or exact per-frame poses | Research only; not a release gate |

Finite stepping or preserved coefficients alone cannot establish L3.
Bullet solver order, floating-point details and exact MMD motion are
non-goals. Investigate an L4 difference only for a concrete user-visible
defect or an explicit external contract. Further spring or contact work must
show a representative L3 improvement; known differences may remain limitations.

### 12.2 Approximation and diagnostic rules

- Start approximations opt-in; preserve defaults unless an explicit contract
  change justifies a change. Source-to-neutral conversions are owned by §7;
  the static coordinate and authoring rules remain in STAGE_CONTRACT §6/§13.
- Describe preservation and execution separately. The capability matrix owns
  their availability summary, including experimental scope.
- Diagnostics must distinguish `exact`, `approximated`, `unsupported`
  and `invalid` at least in logs; prefer machine-readable per-feature data.
  `exact` refers to the stated conversion/feature contract, never whole-trajectory
  equivalence. Well-formed but unexecuted semantics are unsupported;
  malformed declarations are invalid.
- This classification is a requirement for the consumer API, not an assertion
  that those four strings are emitted today. The isolated probe's
  `jointFidelity=approximate/unverified`, source-audit counts and
  `approximateTranslationSpringAxes` / `approximateRotationSpringAxes`
  retain their existing meanings (§7.3–§7.5). Do not lower preservation-gap
  counts merely because an approximation executes.
- Record remaining restrictions in the matrix and evidence in dated reports;
  do not leave equivalence research as an indefinite mandatory gate.

### 12.3 Representative scene acceptance

Use 3–5 fixed scenarios across 1–2 licensed representative models/motions.
Keep local real-model evidence separate from redistributable synthetic CI
fixtures. Record source identifiers, permission boundaries, build SHAs,
feature flags, timestep, duration, random seed where relevant, and repeatable
commands in a new dated report.

The minimum scene set covers:

1. Rest under gravity: constrained secondary bodies settle without explosion,
   NaN/Inf or sustained constraint breakdown.
2. Keyed movement and recovery: animation drives kinematic bodies, secondary
   bodies sway and return, and feedback produces a finite final bone pose.
3. Contact and repeat playback: intended body pairs collide, filtered pairs
   remain excluded, and repeated runs/resets do not accumulate significant drift.
4. Optionally stress a representative spring/limit interaction and a
   retargeted-input case when these affect the selected avatar's behavior.

Before running, declare numeric bounds for contact penetration, joint-frame
separation/limit error, final-pose drift, settling/recovery and repeatability.
No unexplained penetration, explosion, NaN, constraint failure or material
accumulated drift is allowed. Expected bounded contact penetration and
solver tolerance must be stated explicitly.

Automated assertions check finite body/bone transforms every step, declared
constraint/contact bounds, expected qualitative motion and reset/replay
repeatability. Verify the runtime actually consumes the final pose for
rendering. Screenshots or visual review may supplement these assertions;
they cannot alone close L3. Publish failures and known limitations beside the
commands. This contract defines acceptance; only a measured report can claim
that a selected scene set passes.
