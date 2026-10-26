# Evidence reports

Reports preserve the environment, scope and results of a dated check. Read
each report's limitations before applying its findings to another build or
asset. Later reports extend or correct earlier evidence; they do not rewrite it.

This index groups evidence by subject. The [capability matrix](../reference/CAPABILITY_MATRIX.md)
owns current support claims, [current.md](../roadmap/current.md) owns active
work, and [release records](../releases/README.md) own shipped scope.

## Live input, morphs and pose feedback

- 2026-10-27: [Private-avatar Hydra2 visual acceptance](2026-10-27-phase8-hydra2-visual-acceptance.md).
- 2026-10-26: [Renderer UV tangent regeneration](2026-10-26-phase8-uv-tangents.md).
- 2026-10-26: [Resident primary UV mapping](2026-10-26-phase8-resident-uv-mapping.md).
- 2026-10-26: [Toon primary UV consumer](2026-10-26-phase8-toon-primary-uv.md).
- 2026-10-26: [Generated composed resident UV](2026-10-26-phase8-composed-resident-uv.md).
- 2026-10-26: [Generated composed avatar deformation and UV](2026-10-26-phase8-composed-avatar-uv.md).
- 2026-10-25: [Live primary UV transport](2026-10-25-phase8-live-primary-uv.md).
- 2026-10-25: [Owner material morph evaluation](2026-10-25-phase8-material-morph-evaluation.md).
- 2026-10-25: [Owner UV morph evaluation](2026-10-25-phase8-uv-morph-evaluation.md).
- 2026-10-25: [Start-pose initialization](2026-10-25-phase8-start-pose-initialization.md).
- 2026-10-25: [Dynamic-bone feedback correction](2026-10-25-phase8-dynamic-bone-feedback.md).
- 2026-10-24: [Live owner-resolved vertex groups](2026-10-24-phase8-live-resolved-morph.md).
- 2026-10-24: [Owner non-bone group resolution](2026-10-24-phase8-resolved-morph-channels.md).
- 2026-10-24: [Live direct vertex morphs](2026-10-24-phase8-live-vertex-morph.md).
- 2026-10-24: [Live keyed input and private intake](2026-10-24-phase8-live-keyed-input.md).

## Runtime composition and lifecycle

These reports cover generated consumers, explicit resources and scoped live
hosts. Their individual checks do not establish representative avatar L3
acceptance or resident-renderer agreement.

- 2026-10-21: [Live usdview host](2026-10-21-phase8-usdview-live.md).
- 2026-10-21: [Live body properties](2026-10-21-phase8-live-body-properties.md).
- 2026-10-21: [Live experimental springs](2026-10-21-phase8-live-springs.md).
- 2026-10-21: [Stage/world binding](2026-10-21-phase8-stage-world-binding.md).
- 2026-10-21: [Scene body/joint resources](2026-10-21-phase8-scene-resources.md).
- 2026-10-21: [Selective participant unload](2026-10-21-phase8-selective-unload.md).
- 2026-10-21: [World lifecycle](2026-10-21-phase8-world-lifecycle.md).
- 2026-10-21: [Host playback session](2026-10-21-phase8-host-session.md).
- 2026-10-21: [Stage Runner clock integration](2026-10-21-phase8-stage-runner-clock.md).
- 2026-10-21: [Toon snapshot consumer](2026-10-21-phase8-toon-consumer.md).
- 2026-10-21: [First runtime consumer](2026-10-21-phase8-runtime-consumer.md).
- 2026-10-21: [Coupling adapter](2026-10-21-phase8-coupling-adapter.md).
- 2026-10-21: [Owner control continuation](2026-10-21-phase8-control-continuation.md).
- 2026-10-21: [Control boundary](2026-10-21-phase8-control-boundary.md).

## Physics preservation, readers and execution probes

The [physics contract](../design/PHYSICS_INTEGRATION.md) defines conversion
and acceptance policy. These measurements retain their original package and
backend scope; consult the matrix for execution limits.

- 2026-10-20: [Stage validation on Windows](2026-10-20-phase8-stage-validation-windows.md).
- 2026-10-20: [Stage validation](2026-10-20-phase8-stage-validation.md).
- 2026-10-20: [Hosted physics acceptance](2026-10-20-phase8-physics-hosted.md).
- 2026-10-20: [Physics CI Python environment](2026-10-20-phase8-physics-ci-python.md).
- 2026-10-20: [Physics CI integration](2026-10-20-phase8-physics-ci.md).
- 2026-10-18: [Package compatibility](2026-10-18-phase8-package-compatibility.md).
- 2026-10-18: [Generated fixture and replay regression](2026-10-18-phase8-fixture-replay.md).
- 2026-10-18: [Rotation-spring approximation](2026-10-18-phase8-rotation-springs.md).
- 2026-10-18: [Translation-spring approximation](2026-10-18-phase8-translation-springs.md).
- 2026-10-18: [Joint-spring metadata audit](2026-10-18-phase8-joint-spring-audit.md).
- 2026-10-18: [Contact pair mixing](2026-10-18-phase8-contact-mixing.md).
- 2026-10-18: [Body damping](2026-10-18-phase8-body-damping.md).
- 2026-10-18: [Contact material coefficients](2026-10-18-phase8-contact-material.md).
- 2026-10-18: [Collision filtering](2026-10-18-phase8-collision-filter.md).
- 2026-10-18: [Six-DOF execution](2026-10-18-phase8-six-dof-execution.md).
- 2026-10-18: [Mass reader](2026-10-18-phase8-mass-reader.md).
- 2026-10-18: [Joint reader](2026-10-18-phase8-joint-reader.md).
- 2026-10-18: [Kinematic reader](2026-10-18-phase8-kinematic-reader.md).
- 2026-10-18: [Primitive reader](2026-10-18-phase8-primitive-reader.md).
- 2026-10-18: [Collider local pose](2026-10-18-phase8-collider-local-pose.md).
- 2026-10-18: [Child box reader](2026-10-18-phase8-child-box-reader.md).
- 2026-10-17: [Initial physics consumer](2026-10-17-phase8-physics-consumer.md).

## Motion, reference rests and retargeting

- 2026-10-05: [Shipped PMX pose finish](2026-10-05-phase8-pmx-pose-finish-shipped.md).
- 2026-10-05: [PMX pose finish](2026-10-05-phase8-pmx-pose-finish.md).
- 2026-10-05: [Shipped shoulder reference rest](2026-10-05-phase9-shoulder-rest-shipped.md).
- 2026-10-04: [Shoulder rest measurement](2026-10-04-phase9-shoulder-rest.md).
- 2026-10-04: [Arm-chain reference rest](2026-10-04-phase9-arm-chain-reference-rest.md).
- 2026-10-04: [Connector captures onto PMX](2026-10-04-phase8-connector-captures-onto-pmx.md).
- 2026-10-04: [Generic clips onto PMX](2026-10-04-phase8-generic-clips-onto-pmx.md).
- 2026-10-04: [Upper-chest fold](2026-10-04-phase8-upper-chest-fold.md).
- 2026-09-25: [Upper-chest mapping](2026-09-25-phase9-upper-chest.md).
- 2026-09-25: [Rest-pose comparison](2026-09-25-phase9-rest-pose-comparison.md).
- 2026-09-22: [Skeletal motion acceptance](2026-09-22-phase9-motion-acceptance.md).
- 2026-09-19: [Roles and root motion](2026-09-19-phase9-roles-and-root.md).
- 2026-09-19: [IK reference comparison](2026-09-19-phase9-ik-reference.md).
- 2026-09-19: [Local control evaluation](2026-09-19-phase9-local-control.md).
- 2026-09-17: [Phase 7 local motions](2026-09-17-phase7-local-motions.md).

## Importer, materials and packaging

- 2026-09-26: [USDZ export over local models](2026-09-26-phase10-mmd-export-local-models.md).
- 2026-09-25: [USDZ packaging probe](2026-09-25-usdz-packaging-probe.md).
- 2026-09-25: [Importer contract v2](2026-09-25-phase8-importer-contract-v2.md).
- 2026-09-25: [Material input measurement](2026-09-25-phase8-material-inputs.md).
- 2026-09-17: [Phase 6 local models](2026-09-17-phase6-local-models.md).
- 2026-09-16: [Phase 5 local models](2026-09-16-phase5-local-models.md).
- 2026-09-16: [Phase 4 local models](2026-09-16-phase4-local-models.md).
- 2026-09-15: [Phase 2 local models](2026-09-15-phase2-local-models.md).

## Documentation and build tooling

- 2026-10-18: [Documentation ownership audit](2026-10-18-docs-ownership-audit.md).
- [OpenStrata reports](ost/README.md): source-tree bundle staging and moved library pins.
