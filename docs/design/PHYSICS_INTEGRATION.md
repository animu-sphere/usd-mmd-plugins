# Physics integration contract (MMD-specific boundary)

> Status: **proposed** (2026-09-21). The static stage contract is already
> binding from Phase 6; this document defines the future runtime boundary
> around it. Nothing here claims that simulation is implemented.

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
integration. Linear/angular response controls at 60/240 Hz measure that
approximation; force ordering, large rates/steps and full PMX trajectories
remain fidelity gaps. The importer and generic USD reader do not change.

### 7.3 Joint spring metadata audit

The isolated consumer reads the preserved `translationSpring` and
`rotationSpring` pair as constant `vector3f` values. Partial pairs, wrong
types, missing/blocked values, time samples and non-finite components reject.
Finite signed values remain verbatim: no stiffness conversion or force is
implemented. The audit includes format-only joint prims outside the shared
snapshot, without interpreting source-type activation rules.

Nonzero pairs are reported with the input, prim path and both vectors.
Snapshot rows count affected prims and components and mark joint fidelity
`approximate` when nonzero coefficients or format-only joints are present;
otherwise it remains `unverified`. Snapshot acceptance and finite stepping
do not establish spring fidelity. Unit conversion and a neutral execution
contract still require independent response evidence
([audit report](../reports/2026-10-18-phase8-joint-spring-audit.md)).

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
The neutral backend's numerical damping is measured at 60/240 Hz. Source
response, angular springs and runtime coupling remain open
([evidence and sources](../reports/2026-10-18-phase8-translation-springs.md)).

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
6. **Improve joint fidelity.** Add limits, springs, damping and validation in
   backend-independent increments.

Static preservation was completed in Phase 6. These runtime steps belong to
Phase 8 composition and its external collaborators; they do not reopen Phase
6 unless the authored contract itself is wrong.

**First consumer evidence (2026-10-17).** An isolated probe linked only to
installed `physicsUsd` 0.1 accepts its positive Box control but rejects all 22
physics-bearing importer fixtures: its current reader requires collision on
the body Cube, while §3's stage has a body Xform and a child collider. The
[report](../reports/2026-10-17-phase8-physics-consumer.md) separates that
observed failure from further gaps identified in the reader's source.
Steps 1–2 are not complete. Expand the generic reader using these conventional
USD declarations before fixing the MMD adapter's API; do not flatten the
importer's hierarchy or add backend metadata to accommodate the Box subset.

**Bounded reader follow-up (2026-10-18).** A local installed shared-reader
increment accepts a centered, axis-aligned child Cube below a rigid body
Xform, with composed body orientation/translation and child scale. The
probe now checks this beside its original direct Cube control. All 22 full
physics-bearing PMX fixtures still reject, at the joint declaration during
the reader's initial validation pass. The changed rejection path does not
establish body support. Collider-local offsets/rotations, sphere/capsule
shapes, kinematic bodies and joints remain required; steps 1–2 remain open
([follow-up report](../reports/2026-10-18-phase8-child-box-reader.md)).

**Collider-local pose follow-up (2026-10-18).** A further local installed
increment carries the child Cube's composed translation/rotation in a neutral
shape-local transform, preserving the separate body frame. The isolated
consumer checks this with a third positive control; all 22 full physics-bearing
PMX fixtures still reject at joint validation. Sphere/capsule geometry,
kinematic operations and joints remain required, and steps 1–2 remain open
([report](../reports/2026-10-18-phase8-collider-local-pose.md)).

**Primitive reader follow-up (2026-10-18).** A local installed increment adds
Sphere/Capsule geometry and a `readPrimitiveScene()` snapshot API while
retaining the Cube-only API. The probe accepts its three Box controls plus
child Sphere/Capsule controls; all 22 full physics-bearing PMX fixtures still
reject at joint declaration validation. Kinematic operations and joints
remain required, and steps 1–2 remain open
([report](../reports/2026-10-18-phase8-primitive-reader.md)).

**Kinematic follow-up (2026-10-18).** A local installed shared increment adds
neutral kinematic body descriptors and an optional pose-target movement
capability implemented by Jolt. The reader admits standard time-independent
kinematic flags without requiring dynamic mass. The probe accepts three
mass-free child Cube/Sphere/Capsule controls alongside the previous five
controls. All 22 full physics-bearing fixtures still reject at joint
declaration validation; steps 1–2 remain open. Joint frames/limits are next,
and MMD bone synchronization remains deferred
([report](../reports/2026-10-18-phase8-kinematic-reader.md)).

**Joint snapshot follow-up (2026-10-18).** A local installed shared increment
adds `readPhysicsScene()`, preserving generic/fixed joint endpoints, local
frames and six-axis limits in meters/radians. The body-only readers retain
their joint rejection; the new API creates no runtime constraints. The probe
accepts its previous eight controls and a two-body joint control. Of 22
physics-bearing PMX fixtures, one yields three bodies/two standard joints
with coincident reconstructed world joint frames, but also has a format-only
joint outside the snapshot; 21 reject missing explicit dynamic mass. Six
physics-free fixtures remain empty snapshots. Steps 1–2 remain open for
complete consumption and fidelity. Mass inference, generic six-DOF execution,
coefficients/filtering and source-only semantics precede MMD bone coupling
([report](../reports/2026-10-18-phase8-joint-reader.md)).

**Primitive mass inference follow-up (2026-10-18).** A local installed shared
increment resolves zero/absent dynamic mass from single-primitive collision
volume and body-local density, with USD's 1000 kg/m³ density fallback. All
22 physics-bearing PMX fixtures now yield three bodies/two standard joints
each; six physics-free fixtures yield empty snapshots. The isolated consumer
also passes three new inference controls alongside its previous nine. One
format-only joint remains outside the generic snapshot. Steps 1–2 remain
open for full fidelity and simulation: six-DOF execution, physical
coefficients/filtering, springs, PMX zero-mass equivalence and runtime
synchronization still precede MMD bone coupling
([report](../reports/2026-10-18-phase8-mass-reader.md)).

**Experimental six-DOF execution follow-up (2026-10-18).** An explicitly
enabled installed backend creates the two standard constraints and steps all
22 physics-bearing fixtures 120 times with finite state. The neutral contract
states body-origin frames, free/locked/limited axes and pair collision flags;
angular execution uses twist/pyramid swing. The baseline scalar SDK fails a
new Z-axis load test, while a separate candidate SDK passes. Execution remains
opt-in pending SDK/hosted acceptance. Steps 1–2 remain open for full fidelity:
source-only joints, coefficients/filtering, springs, PMX angular/zero-mass
equivalence and runtime synchronization still precede MMD bone coupling
([report](../reports/2026-10-18-phase8-six-dof-execution.md)).

**SDK cause follow-up (2026-10-18).** The shared repository traced the
baseline SDK's Z-axis failure to MSVC `/fp:fast`, which Jolt applies to
non-deterministic Release builds; x86 SIMD options do not matter, and GCC
passes either way. It selected `CROSS_PLATFORM_DETERMINISTIC=ON`, the
configuration this repository's execution probe already used, and its source
CI now builds Jolt that way ([usd-physics-plugins #19](https://github.com/animu-sphere/usd-physics-plugins/pull/19)). The probe needs
no rerun. Hosted six-DOF acceptance, coefficients/filtering, springs and
runtime synchronization still precede MMD bone coupling.

**Hosted six-DOF acceptance (2026-10-18).** The shared repository's
`usd-jolt-six-dof` intent enables the capability over its deterministic CI
Jolt, and its Windows (MSVC) and Linux (GCC) pull-request cells pass all 18
tests, including the per-axis gate and the installed consumer's six-DOF
section; those cells never package ([usd-physics-plugins #20](https://github.com/animu-sphere/usd-physics-plugins/pull/20)). The SDK gate is closed. The option
stays OFF by default and no six-DOF package is published, so this
repository's probe still builds the shared packages locally. Coefficients/
filtering, springs, source-only semantics and runtime synchronization still
precede MMD bone coupling.

**MMD collision-filter follow-up (2026-10-18).** The isolated consumer now
validates the preserved group/mask and passes the neutral filter at body
creation, outside `physicsUsd`. Reader-only controls cover all 16 groups,
zero/all/high-bit masks, asymmetric rejection and malformed metadata. Six
execution controls check actual contacts, including same-group bodies with
different masks in one world. All 22 physics-bearing fixtures still create
three filtered bodies/two constraints and step 120 times with finite state;
six physics-free fixtures remain empty. This closes the probe's source-filter
gap locally, without establishing a runtime adapter or mixed-format category
allocation. Coefficients, springs, source-only joints, PMX angular/zero-mass
equivalence, hosted MMD-filter acceptance and synchronization remain open
([report](../reports/2026-10-18-phase8-collision-filter.md)).

**Contact-coefficient follow-up (2026-10-18).** Locally rebuilt installed
core/backend packages now accept neutral friction/restitution on body creation.
The isolated consumer validates the two MMD attributes in reader-only mode
and passes them into execution. Four response controls distinguish sliding
and bounce; the existing filter controls and all 22 physics-bearing fixtures
still pass, with three materials/bodies, two constraints and 120 finite steps
each. Damping, pair-mixing fidelity, springs, source-only semantics and runtime
synchronization remain open. No runtime adapter or new normal dependency is
introduced ([report](../reports/2026-10-18-phase8-contact-material.md)).

**Body-damping follow-up (2026-10-18).** Locally rebuilt installed packages
accept neutral linear/angular decay rates. The consumer validates and converts
the two MMD coefficients in reader-only mode, then supplies them at body
creation. Independent translation and free-spin controls pass at 60/240 Hz;
all 22 physics-bearing fixtures still create three bodies/two constraints and
step 120 times with finite state. Instantaneous damping (`d=1`), finite-step
fidelity, springs, source-only joints and runtime synchronization remain open
([report](../reports/2026-10-18-phase8-body-damping.md)).

**Contact-mixing follow-up (2026-10-18).** Installed shared packages now accept
neutral per-world coefficient rules and an optional friction ceiling. The
consumer chooses products with a ceiling of 10; independent source metadata,
asymmetric/equal/zero pairs and the ceiling pass at 60/240 Hz. All 22
physics-bearing fixtures still create three bodies/two constraints and step
120 times with finite state. Shared tests preserve default Jolt behavior and
isolate simultaneously alive worlds. Source trajectories, mixed-format
policies, springs and runtime synchronization remain open
([report](../reports/2026-10-18-phase8-contact-mixing.md)).

## 10. Testing

- Parser and canonical-model tests continue to verify every PMX value and
  conversion without any physics runtime.
- Stage tests verify APIs, relationships, meters, Y-up, deterministic paths
  and exact `mmd:physics:*` preservation without simulation.
- Contract tests reconstruct every body and joint from a stage alone.
- Runtime integration tests cover bone → kinematic body, dynamic body motion,
  a constrained pair and body → bone feedback.
- Backend-specific tolerances stay in the backend integration tests, never in
  parser or canonical-model tests.
- Deterministic recorded motion drives CI; live devices and wall-clock timing
  are not prerequisites.

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
