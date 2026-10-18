# Current roadmap

> Scope: active work only. Historical evidence lives in reports.
> Updated: 2026-10-18

## Focus

MMD physics convergence and avatar end-to-end acceptance under the
[L1–L4 policy](../design/PHYSICS_INTEGRATION.md#12-fidelity-and-acceptance-policy).
Active priority is P0 → P1 → P2; Phase 8/9/10 remain scope identifiers.

Support claims belong to the [capability matrix](../reference/CAPABILITY_MATRIX.md).
Experimental probe results do not establish a shipped coupling adapter or
avatar acceptance. The [ownership audit](../reports/2026-10-18-docs-ownership-audit.md)
maps the earlier completion history to its evidence.

## Active gates

States: `planned`, `in_progress`, `blocked`. A gate leaves this table when
its outcome is verified; record measurements in a dated report.

| ID | Outcome / exit gate | Owner | State | Evidence / contract |
| --- | --- | --- | --- | --- |
| P0-2 (Phase 8) | Wire the installed-package representative lane into CI and verify the hosted configuration; maintain source/reader/stability regressions and explicit execution limits | MMD acceptance here; generic backend in `usd-physics-plugins` | in_progress | [Testing policy](../design/PHYSICS_INTEGRATION.md#10-testing), [fixture/replay automation](../reports/2026-10-18-phase8-fixture-replay.md), [installed-package baseline](../reports/2026-10-18-phase8-package-compatibility.md), [capabilities](../reference/CAPABILITY_MATRIX.md#physics-preservation-and-execution) |
| P1-1 (Phase 8) | Establish the optional coupling component/API from the first runtime consumer; verify pose → kinematic sync → shared step → body feedback → final pose | MMD adapter here; `usd-stage-runner` ordering; `usd-avatar-runtime` composition | planned | [Runtime contract](../design/PHYSICS_INTEGRATION.md#6-runtime-order), [package contract](../architecture/PACKAGE_CONTRACT.md) |
| P1-2 (Phase 8) | Run the fixed representative scene set with automatic assertions, known limitations and reproducible commands; publish a new acceptance report | `usd-avatar-runtime` + MMD acceptance; `hydra-toon` rendering | planned | [L3 acceptance](../design/PHYSICS_INTEGRATION.md#123-representative-scene-acceptance), [avatar composition](#avatar-runtime-composition) |
| P1-3 (Phase 8) | Validate runtime material morphs and advanced UV/vertex-color behavior through the composed renderer | Runtime consumer + `hydra-toon`; MMD contracts here | planned | [Material contract](../design/MATERIAL_POLICY.md), [operator evidence boundary](../reports/2026-10-18-docs-ownership-audit.md#retained-evidence-and-unverified-signals) |
| P2-1 (Phase 10) | Confirm USDZ portability, texture resolution, validation and ASCII archive paths as the release contract; make the installed consumer API and release notes consistent | Export tool and package owners here | planned | [Packaging contract](../design/PACKAGING_POLICY.md), [export guide](../guides/exporting.md), [release procedure](../releases/README.md) |
| P2-2 (Phase 9) | Adopt expression interoperability only after a shared semantic exists; preserve source morph channels meanwhile | `usd-motion-plugins` semantic; MMD mapping here | blocked | [MOTION §10.7](../design/MOTION_CONTRACT.md#107-morphs-as-channels) |

## Avatar runtime composition

This heading is retained for links from earlier evidence.
P1 verifies the frame order in the owning physics contract; generic retarget,
MMD pose completion, physics and rendering must run together in the consumer.

Select 1–2 licensed representative PMX models and VMD motions and document
their acquisition/use terms. Keep local real-model runs separate from
redistributable generated fixtures. CI receives third-party assets only with
explicit redistribution permission. The scene set and required assertions are
defined once in [PHYSICS_INTEGRATION §12.3](../design/PHYSICS_INTEGRATION.md#123-representative-scene-acceptance).

## External dependencies

| Dependency | Needed outcome | Tracking |
| --- | --- | --- |
| `usd-physics-plugins` | Compatible installed descriptors, deterministic backend configuration and explicit opt-in capabilities; publication is required before claiming a published runtime dependency | [DEPENDENCIES §7](../architecture/DEPENDENCIES.md#7-usd-physics-plugins) |
| `usd-stage-runner` / `usd-avatar-runtime` | First composed consumer to fix scheduling and the MMD coupling API | [PHYSICS_INTEGRATION §6](../design/PHYSICS_INTEGRATION.md#6-runtime-order) |
| `hydra-toon` | Composed material/geometry acceptance; renderer-private common code only after both MMD and MToon paths provide evidence | [MATERIAL_POLICY](../design/MATERIAL_POLICY.md) |
| `usd-motion-plugins` | Common expression semantic for P2-2 | [MOTION §10.7](../design/MOTION_CONTRACT.md#107-morphs-as-channels) |
| `usd-motion-plugins` / first model-free VMD consumer | Standalone VMD stage decision (MOT-O2), before `usdVmdFileFormat` | [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) |
| `usd-vrm-plugins` | Second working packager before extracting shared packaging code | [PACKAGING §16](../design/PACKAGING_POLICY.md#16-later) |

## Deferred / non-goals

- Bullet/MMD trajectory or frame-by-frame equivalence is not a release gate.
  Research reopens only for a concrete user-visible defect or external contract.
- Further spring/solver refinement, finite-angle equivalence, additional contact
  models and solver-order matching are deferred; the
  [capability matrix](../reference/CAPABILITY_MATRIX.md#physics-preservation-and-execution)
  records execution limits. Existing opt-in rotation approximation remains experimental.
- MOT-O11's keyed-knee comparison awaits MMD output, without changing the
  current rule; it is a bounded research question, not a physics release gate.
- Model-free VMD stage opening and shared packaging extraction remain contingent
  work as listed above; neither blocks the present importer/exporter scope.
- Long corpus runs, exhaustive timestep/platform combinations and manual
  Bullet comparisons are research/manual work, outside PR blocking acceptance.

## Completion criteria

- P0: compatible installed packages and representative source/reader/stability
  regressions pass; execution limits are explicit and discoverable.
- P1: representative PMX + VMD motion → physics → bone feedback → render
  behavior satisfies the owning L3 assertions, with a reproducible report.
- P2: users can build, inspect and export through the documented entry points,
  judge support and portability, and assess a release without further fidelity
  exploration. External blocked features stay explicitly outside that release's claim.

No avatar L3 completion is claimed by this roadmap update.
