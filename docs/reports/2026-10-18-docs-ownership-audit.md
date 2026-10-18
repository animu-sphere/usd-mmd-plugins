# Documentation ownership audit (2026-10-18)

## Baseline and decision

This audit applies the scope/physics/docs policy supplied on 2026-10-18 to
repository baseline `1db1ef3f90a5cc8b38b3fef140453658deb3b5ea`. It is a dated reorganization record, not a
current-status page or a new physics experiment. No runtime acceptance was
rerun for this documentation change.

The existing PHYSICS_INTEGRATION contract remains the owner for MMD physics
conversion and acceptance. The supplied policy's L1–L3 scope and L4 non-goal
are adopted there. Rotation approximation was already present at the baseline,
so it is retained as experimental instead of described as absent.

## Duplicate claims found at the baseline

| Claim / evidence | Previous locations | Owner after consolidation |
| --- | --- | --- |
| Phase progress and long completion history | Root README, docs index, roadmap index/current, contract introductions | current.md for active work; reports/releases for history |
| Physics reader, SDK, damping/contact and spring results | docs index, current.md, PHYSICS_INTEGRATION §9, DEPENDENCIES §7, matrix | Dated reports for measurements; matrix for bounded support |
| Full source-trajectory fidelity as a prerequisite to coupling | current.md, physics implementation sequence and copied follow-ups | PHYSICS_INTEGRATION §12 acceptance policy; differences retained as limits |
| Motion revisions and measured pose comparisons | MOTION_CONTRACT introduction/decision rows and current.md | Reports below; normative decisions keep their existing section/ID |
| Dependency ranges, artifact versions and package history | DEPENDENCIES prose/tables and build manifests | Manifests/pin modules; architecture keeps required interfaces and boundaries |
| Shipped Phase/version mapping | Roadmap status table and release records | Release records |

## Retained evidence and unverified signals

The baseline files remain recoverable at the SHA above. The following evidence
owns the facts removed from progress summaries; historical results are not
rewritten to imply later support.

- Initial static/parser/model/rig/physics/VMD acceptance: dated Phase 2–7
  reports in the inventory below; shipped scope in
  [v0.1.0](../releases/v0.1.0.md).
- Motion evaluation and IK comparison:
  [local control](2026-09-19-phase9-local-control.md),
  [IK reference](2026-09-19-phase9-ik-reference.md),
  [roles/root](2026-09-19-phase9-roles-and-root.md), and
  [skeletal acceptance](2026-09-22-phase9-motion-acceptance.md).
- Reference-rest and retarget decisions:
  [rest comparison](2026-09-25-phase9-rest-pose-comparison.md),
  [upper chest](2026-09-25-phase9-upper-chest.md),
  [arm reference rest](2026-10-04-phase9-arm-chain-reference-rest.md),
  [shoulder measurement](2026-10-04-phase9-shoulder-rest.md), and
  [shipped shoulder](2026-10-05-phase9-shoulder-rest-shipped.md).
- Generic/connector input and PMX pose completion:
  [generic clips](2026-10-04-phase8-generic-clips-onto-pmx.md),
  [upper-chest fold](2026-10-04-phase8-upper-chest-fold.md),
  [connector captures](2026-10-04-phase8-connector-captures-onto-pmx.md),
  [pose finish](2026-10-05-phase8-pmx-pose-finish.md), and
  [shipped pose finish](2026-10-05-phase8-pmx-pose-finish-shipped.md).
- Materials/schema/importer:
  [input measurement](2026-09-25-phase8-material-inputs.md),
  [contract-v2 importer](2026-09-25-phase8-importer-contract-v2.md), and
  shipped package scope in [v0.2.0](../releases/v0.2.0.md).
- Physics reader progression:
  [initial rejection](2026-10-17-phase8-physics-consumer.md),
  [child box](2026-10-18-phase8-child-box-reader.md),
  [collider pose](2026-10-18-phase8-collider-local-pose.md),
  [primitives](2026-10-18-phase8-primitive-reader.md),
  [kinematics](2026-10-18-phase8-kinematic-reader.md),
  [joint snapshots](2026-10-18-phase8-joint-reader.md), and
  [mass inference](2026-10-18-phase8-mass-reader.md).
- Execution/SDK:
  [six-DOF experiment](2026-10-18-phase8-six-dof-execution.md).
  The baseline also records the MSVC /fp:fast diagnosis and deterministic-SDK
  selection in [shared PR #19](https://github.com/animu-sphere/usd-physics-plugins/pull/19),
  and hosted Windows/Linux six-axis acceptance in
  [shared PR #20](https://github.com/animu-sphere/usd-physics-plugins/pull/20).
  These historical observations do not imply published spring packages.
- Source metadata/execution increments:
  [collision filters](2026-10-18-phase8-collision-filter.md),
  [contact values](2026-10-18-phase8-contact-material.md),
  [body damping](2026-10-18-phase8-body-damping.md),
  [pair mixing](2026-10-18-phase8-contact-mixing.md),
  [spring audit](2026-10-18-phase8-joint-spring-audit.md),
  [translation approximation](2026-10-18-phase8-translation-springs.md), and
  [rotation approximation](2026-10-18-phase8-rotation-springs.md).
- USDZ:
  [initial probe](2026-09-25-usdz-packaging-probe.md),
  [local models](2026-09-26-phase10-mmd-export-local-models.md), and
  [v0.2.0](../releases/v0.2.0.md). Packaging fixture tests own deterministic
  bytes and portable names; no new portability test was performed by this audit.
- The former roadmap records an operator report that hdToon look approximation
  was complete on 2026-10-17. This is an operator signal, not an automated
  assertion or runtime material-morph/UV/vertex-color acceptance. Renderer
  evidence belongs to hydra-toon; composed acceptance remains separate.
- The former roadmap's schema/imaging/installed-package completion notes
  remain in the v0.2.0 release record and their tests. Their implementation
  detail is not an avatar-physics acceptance result.

## Inventory

Classification covers every existing docs Markdown page, including category
indexes. Reference pages are classified as contract (diagnostics/source mapping)
or guide (the capability lookup); the matrix remains the single support summary.
The root README is a guide/entry point, not a second status source.

| File | Class |
| --- | --- |
| [architecture/DEPENDENCIES.md](../../docs/architecture/DEPENDENCIES.md) | architecture |
| [architecture/PACKAGE_CONTRACT.md](../../docs/architecture/PACKAGE_CONTRACT.md) | architecture |
| [architecture/WORKSPACE.md](../../docs/architecture/WORKSPACE.md) | architecture |
| [contributing/documentation.md](../../docs/contributing/documentation.md) | guide |
| [contributing/RELEASE_NOTES_TEMPLATE.md](../../docs/contributing/RELEASE_NOTES_TEMPLATE.md) | guide |
| [design/DESIGN_POLICY.md](../../docs/design/DESIGN_POLICY.md) | contract |
| [design/MATERIAL_POLICY.md](../../docs/design/MATERIAL_POLICY.md) | contract |
| [design/MOTION_CONTRACT.md](../../docs/design/MOTION_CONTRACT.md) | contract |
| [design/PACKAGING_POLICY.md](../../docs/design/PACKAGING_POLICY.md) | contract |
| [design/PHYSICS_INTEGRATION.md](../../docs/design/PHYSICS_INTEGRATION.md) | contract |
| [design/PMX_CONTRACT.md](../../docs/design/PMX_CONTRACT.md) | contract |
| [design/STAGE_CONTRACT.md](../../docs/design/STAGE_CONTRACT.md) | contract |
| [design/TEXT_ENCODING_POLICY.md](../../docs/design/TEXT_ENCODING_POLICY.md) | contract |
| [guides/building.md](../../docs/guides/building.md) | guide |
| [guides/exporting.md](../../docs/guides/exporting.md) | guide |
| [guides/inspecting.md](../../docs/guides/inspecting.md) | guide |
| [guides/opening.md](../../docs/guides/opening.md) | guide |
| [README.md](../../docs/README.md) | guide |
| [reference/CAPABILITY_MATRIX.md](../../docs/reference/CAPABILITY_MATRIX.md) | guide (capability reference) |
| [reference/DIAGNOSTICS.md](../../docs/reference/DIAGNOSTICS.md) | contract |
| [reference/SOURCE_MAPPING.md](../../docs/reference/SOURCE_MAPPING.md) | contract |
| [releases/README.md](../../docs/releases/README.md) | release |
| [releases/v0.1.0.md](../../docs/releases/v0.1.0.md) | release |
| [releases/v0.2.0.md](../../docs/releases/v0.2.0.md) | release |
| [reports/2026-09-15-phase2-local-models.md](../../docs/reports/2026-09-15-phase2-local-models.md) | report |
| [reports/2026-09-16-phase4-local-models.md](../../docs/reports/2026-09-16-phase4-local-models.md) | report |
| [reports/2026-09-16-phase5-local-models.md](../../docs/reports/2026-09-16-phase5-local-models.md) | report |
| [reports/2026-09-17-phase6-local-models.md](../../docs/reports/2026-09-17-phase6-local-models.md) | report |
| [reports/2026-09-17-phase7-local-motions.md](../../docs/reports/2026-09-17-phase7-local-motions.md) | report |
| [reports/2026-09-19-phase9-ik-reference.md](../../docs/reports/2026-09-19-phase9-ik-reference.md) | report |
| [reports/2026-09-19-phase9-local-control.md](../../docs/reports/2026-09-19-phase9-local-control.md) | report |
| [reports/2026-09-19-phase9-roles-and-root.md](../../docs/reports/2026-09-19-phase9-roles-and-root.md) | report |
| [reports/2026-09-22-phase9-motion-acceptance.md](../../docs/reports/2026-09-22-phase9-motion-acceptance.md) | report |
| [reports/2026-09-25-phase8-importer-contract-v2.md](../../docs/reports/2026-09-25-phase8-importer-contract-v2.md) | report |
| [reports/2026-09-25-phase8-material-inputs.md](../../docs/reports/2026-09-25-phase8-material-inputs.md) | report |
| [reports/2026-09-25-phase9-rest-pose-comparison.md](../../docs/reports/2026-09-25-phase9-rest-pose-comparison.md) | report |
| [reports/2026-09-25-phase9-upper-chest.md](../../docs/reports/2026-09-25-phase9-upper-chest.md) | report |
| [reports/2026-09-25-usdz-packaging-probe.md](../../docs/reports/2026-09-25-usdz-packaging-probe.md) | report |
| [reports/2026-09-26-phase10-mmd-export-local-models.md](../../docs/reports/2026-09-26-phase10-mmd-export-local-models.md) | report |
| [reports/2026-10-04-phase8-connector-captures-onto-pmx.md](../../docs/reports/2026-10-04-phase8-connector-captures-onto-pmx.md) | report |
| [reports/2026-10-04-phase8-generic-clips-onto-pmx.md](../../docs/reports/2026-10-04-phase8-generic-clips-onto-pmx.md) | report |
| [reports/2026-10-04-phase8-upper-chest-fold.md](../../docs/reports/2026-10-04-phase8-upper-chest-fold.md) | report |
| [reports/2026-10-04-phase9-arm-chain-reference-rest.md](../../docs/reports/2026-10-04-phase9-arm-chain-reference-rest.md) | report |
| [reports/2026-10-04-phase9-shoulder-rest.md](../../docs/reports/2026-10-04-phase9-shoulder-rest.md) | report |
| [reports/2026-10-05-phase8-pmx-pose-finish-shipped.md](../../docs/reports/2026-10-05-phase8-pmx-pose-finish-shipped.md) | report |
| [reports/2026-10-05-phase8-pmx-pose-finish.md](../../docs/reports/2026-10-05-phase8-pmx-pose-finish.md) | report |
| [reports/2026-10-05-phase9-shoulder-rest-shipped.md](../../docs/reports/2026-10-05-phase9-shoulder-rest-shipped.md) | report |
| [reports/2026-10-17-phase8-physics-consumer.md](../../docs/reports/2026-10-17-phase8-physics-consumer.md) | report |
| [reports/2026-10-18-phase8-body-damping.md](../../docs/reports/2026-10-18-phase8-body-damping.md) | report |
| [reports/2026-10-18-phase8-child-box-reader.md](../../docs/reports/2026-10-18-phase8-child-box-reader.md) | report |
| [reports/2026-10-18-phase8-collider-local-pose.md](../../docs/reports/2026-10-18-phase8-collider-local-pose.md) | report |
| [reports/2026-10-18-phase8-collision-filter.md](../../docs/reports/2026-10-18-phase8-collision-filter.md) | report |
| [reports/2026-10-18-phase8-contact-material.md](../../docs/reports/2026-10-18-phase8-contact-material.md) | report |
| [reports/2026-10-18-phase8-contact-mixing.md](../../docs/reports/2026-10-18-phase8-contact-mixing.md) | report |
| [reports/2026-10-18-phase8-joint-reader.md](../../docs/reports/2026-10-18-phase8-joint-reader.md) | report |
| [reports/2026-10-18-phase8-joint-spring-audit.md](../../docs/reports/2026-10-18-phase8-joint-spring-audit.md) | report |
| [reports/2026-10-18-phase8-kinematic-reader.md](../../docs/reports/2026-10-18-phase8-kinematic-reader.md) | report |
| [reports/2026-10-18-phase8-mass-reader.md](../../docs/reports/2026-10-18-phase8-mass-reader.md) | report |
| [reports/2026-10-18-phase8-primitive-reader.md](../../docs/reports/2026-10-18-phase8-primitive-reader.md) | report |
| [reports/2026-10-18-phase8-rotation-springs.md](../../docs/reports/2026-10-18-phase8-rotation-springs.md) | report |
| [reports/2026-10-18-phase8-six-dof-execution.md](../../docs/reports/2026-10-18-phase8-six-dof-execution.md) | report |
| [reports/2026-10-18-phase8-translation-springs.md](../../docs/reports/2026-10-18-phase8-translation-springs.md) | report |
| [reports/ost/01-2026-09-24-v0.23.3-a-bundle-is-staged-in-its-source-tree.md](../../docs/reports/ost/01-2026-09-24-v0.23.3-a-bundle-is-staged-in-its-source-tree.md) | report |
| [reports/ost/02-2026-10-04-v0.23.14-a-moved-library-pin-keeps-its-old-package.md](../../docs/reports/ost/02-2026-10-04-v0.23.14-a-moved-library-pin-keeps-its-old-package.md) | report |
| [reports/ost/README.md](../../docs/reports/ost/README.md) | report |
| [roadmap/current.md](../../docs/roadmap/current.md) | roadmap |
| [roadmap/README.md](../../docs/roadmap/README.md) | roadmap |
