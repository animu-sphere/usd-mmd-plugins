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

Joint translation follows the same rule:

1. consume the closest conventional `UsdPhysics` representation;
2. read `mmd:physics:*` for source meaning that representation cannot express;
3. report the fidelity gap; and
4. enable enhanced behavior only when the runtime/backend supports it.

Initial runtime support may start with connected bodies, frames and basic
limits. Angular/translational limits, springs and drive-like behavior follow
incrementally. Perfect one-to-one backend behavior is not a prerequisite for
the first integration, but silent loss of source meaning is forbidden.

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
