# Current roadmap

> Scope: active work only. Historical evidence lives in reports.
> Updated: 2026-10-27

## Focus

MMD bone/body coupling and avatar end-to-end acceptance under the
[L1–L4 policy](../design/PHYSICS_INTEGRATION.md#12-fidelity-and-acceptance-policy).
Active priority is P0 → P1 → P2; Phase 8/9/10 remain scope identifiers.

Support claims belong to the [capability matrix](../reference/CAPABILITY_MATRIX.md).
Experimental probe results do not establish a shipped coupling adapter or
avatar acceptance. The [report index](../reports/README.md) groups dated evidence;
the [ownership audit](../reports/2026-10-18-docs-ownership-audit.md) records the
earlier documentation consolidation.

## Active gates

States: `planned`, `in_progress`, `blocked`. A gate leaves this table when
its outcome is verified; record measurements in a dated report.

| ID | Outcome / exit gate | Owner | State | Evidence / contract |
| --- | --- | --- | --- | --- |
| P1-C (Phase 8 / PR-C) | Complete application/stage lifecycle, automatic participant binding and resident renderer integration beyond the scoped live usdview host, with a physics-free importer | `usd-avatar-runtime` orchestration; MMD adapter here; `usd-stage-runner` host loop | in_progress | [Runtime contract](../design/PHYSICS_INTEGRATION.md#6-runtime-order), [Live-host report](../reports/2026-10-21-phase8-usdview-live.md), [MVP gate](../design/PHYSICS_INTEGRATION.md#101-coupling-mvp-acceptance) |
| P1-2 (Phase 8) | Run the fixed representative scene set with automatic assertions, known limitations and reproducible commands; publish a new acceptance report | `usd-avatar-runtime` + MMD acceptance; `hydra-toon` rendering | planned | [L3 acceptance](../design/PHYSICS_INTEGRATION.md#123-representative-scene-acceptance), [avatar composition](#avatar-runtime-composition) |
| P1-3 (Phase 8) | Validate runtime material morphs and advanced UV/vertex-color behavior through the composed renderer | Runtime consumer + `hydra-toon`; MMD contracts here | in_progress | [Material contract](../design/MATERIAL_POLICY.md#11-material-morphs), [next integration work](#next-integration-work) |
| P2-1 (Phase 10) | Confirm USDZ portability, texture resolution, validation and ASCII archive paths as the release contract; make the installed consumer API and release notes consistent | Export tool and package owners here | planned | [Packaging contract](../design/PACKAGING_POLICY.md), [export guide](../guides/exporting.md), [release procedure](../releases/README.md) |
| P2-2 (Phase 9) | Adopt expression interoperability only after a shared semantic exists; preserve source morph channels meanwhile | `usd-motion-plugins` semantic; MMD mapping here | blocked | [MOTION §10.7](../design/MOTION_CONTRACT.md#107-morphs-as-channels) |

## Avatar runtime composition

The final interactive acceptance step is live physics-host confirmation in
the dedicated `hydra-toon` viewport. The scoped private-avatar Hydra2 visual
review is complete; its [acceptance report](../reports/2026-10-27-phase8-hydra2-visual-acceptance.md)
distinguishes live usdview output from recorded native-viewport playback.
Keep this completed visual review separate from the remaining automated L3
and application-lifecycle gates. Use the
[capability matrix](../reference/CAPABILITY_MATRIX.md#control-evaluation-mmdcontrol)
for the owner/runtime support boundary. The
[report index](../reports/README.md#live-input-morphs-and-pose-feedback)
collects the keyed-input, vertex/group, dynamic-feedback, start-pose and
UV/material evidence; [runtime composition reports](../reports/README.md#runtime-composition-and-lifecycle)
retain the generated consumer and lifecycle history.

### Next integration work

1. **UV rendering (runtime consumer + renderer):** extend generated composed
   deformation/physics/UV checks to private PMX/VMD direct-snapshot bindings
   and automatic assertions, and extend typed admission to additional UV
   channels. Composed-USD Hydra2 visual review has separate
   [private-input acceptance](../reports/2026-10-27-phase8-hydra2-visual-acceptance.md);
   it does not prove private-avatar Hydra/direct numerical or pixel parity. The
   [composed avatar UV report](../reports/2026-10-26-phase8-composed-avatar-uv.md)
   records a generated two-joint MMD/Jolt mesh with vertex morphs and UVs in
   one retained snapshot, automatic resident bindings and USD/Hydra stream/pixel
   agreement across world resets. It does not read PMX/VMD or bind an application.
   The earlier
   [composed UV report](../reports/2026-10-26-phase8-composed-resident-uv.md)
   records automatic binding and USD/Hydra stream/pixel agreement through the
   MMD owner and real runtime snapshots; it uses no bones or simulated bodies.
   Generated renderer tangent regeneration
   and runtime/USD-Hydra parity are recorded in the
   [tangent report](../reports/2026-10-26-phase8-uv-tangents.md).
   Generated point-domain automatic Hydra mapping is recorded in the
   [resident UV report](../reports/2026-10-26-phase8-resident-uv-mapping.md);
   tangent-bearing consumption is covered by the later tangent report.
   Explicit direct-consumer UV bindings and generated textured renderer checks
   are recorded in the [Toon UV report](../reports/2026-10-26-phase8-toon-primary-uv.md);
   they do not establish composed MMD-to-GPU or resident Hydra parity.
   Primary runtime snapshots and live USD
   `st` publication have generated numeric checks in the
   [live primary UV report](../reports/2026-10-25-phase8-live-primary-uv.md).
   Follow [MOTION §11.3](../design/MOTION_CONTRACT.md#113-morphs) for immutable
   base-plus-delta application and admission of other reached leaf types.
2. **Material output (runtime consumer + hydra-toon):** extend the generated
   [typed snapshot publication](../reports/2026-10-27-phase8-material-snapshots.md)
   to automatic material binding, live USD appearance and renderer bindings,
   including sampled texture/sphere/toon contributions. Follow
   [MATERIAL §11](../design/MATERIAL_POLICY.md#11-material-morphs) and the
   [owner material report](../reports/2026-10-25-phase8-material-morph-evaluation.md).
   Validate final values and bindings before publication; include reset to
   immutable bases in composed checks. These two transport slices feed P1-3.
3. **Application lifecycle (avatar-runtime + stage-runner):** complete automatic
   participant configuration, application/stage events and selective unload
   policy for cross-avatar constraints. Continue from the
   [stage/world binding report](../reports/2026-10-21-phase8-stage-world-binding.md)
   under P1-C.
4. **Resident rendering (runtime consumer + hydra-toon):** bind resident avatar
   resources and verify rendered/Hydra agreement under P1-C and P1-3. The
   [Toon consumer report](../reports/2026-10-21-phase8-toon-consumer.md) is the
   snapshot-transport starting point; renderer responsibilities remain in
   [MATERIAL §12](../design/MATERIAL_POLICY.md#12-rendering-and-integration-belong-elsewhere).
5. **Representative intake (MMD acceptance + runtime consumer):** select 1–2
   licensed PMX models and VMD motions, document acquisition/use terms and
   choose owner-admitted damping declarations. Keep local real-model runs
   separate from redistributable generated fixtures. CI receives third-party
   assets only with explicit redistribution permission.
6. **Representative acceptance (P1-2):** run the fixed scene set and publish a
   new report with reproducible commands, automatic assertions and known
   limitations. The scene set and required assertions are defined once in
   [PHYSICS_INTEGRATION §12.3](../design/PHYSICS_INTEGRATION.md#123-representative-scene-acceptance).
   Generated live checks alone do not close this gate.
7. **Final interactive physics-host acceptance (runtime host + hydra-toon):**
   after the preceding integration work, run the private PMX/VMD through a live
   runtime-owned physics session in the dedicated viewport. Verify final joint,
   vertex-morph and primary-UV output together, one world Step per physics tick,
   Play/Pause/Reset/Stop, declared loop/seek behavior and world/resource teardown.
   Present the result for the user's visual decision. Recorded USD playback
   cannot close this gate; host orchestration belongs to `usd-avatar-runtime`
   and `usd-stage-runner`, with resident consumption in `hydra-toon`.

### Integration constraints

P1 verifies the [runtime frame order](../design/PHYSICS_INTEGRATION.md#6-runtime-order):
generic retarget, MMD pose completion, physics and rendering must run together
in the consumer. Callbacks consume the owner-side control continuation under
[MOTION §11.10](../design/MOTION_CONTRACT.md#1110-intended-physics-continuation-boundary),
rather than composing the atomic offline evaluator and retarget completion.
The importer remains physics-free.

Coupling follows the [PR-A → PR-B → PR-C sequence](../design/PHYSICS_INTEGRATION.md#9-implementation-sequence),
then representative L3 acceptance (P1-2). Each change requires focused unit
coverage and a small integration smoke for its scope under
[the testing policy](../design/PHYSICS_INTEGRATION.md#10-testing).
Keep long fidelity comparisons outside normal CI. Prior stage-validation,
installed-package and runtime-consumer results remain in the
[physics](../reports/README.md#physics-preservation-readers-and-execution-probes)
and [runtime](../reports/README.md#runtime-composition-and-lifecycle) evidence indexes.

## External dependencies

| Dependency | Needed outcome | Tracking |
| --- | --- | --- |
| `usd-physics-plugins` | Compatible installed descriptors, deterministic backend configuration and explicit opt-in capabilities; publication is required before claiming a published runtime dependency | [DEPENDENCIES §7](../architecture/DEPENDENCIES.md#7-usd-physics-plugins) |
| `usd-avatar-runtime` / `usd-stage-runner` | Application/stage integration and participant lifecycle; avatar-runtime owns avatar order and shared-world barriers, stage-runner supplies host ticks | [PHYSICS_INTEGRATION §6](../design/PHYSICS_INTEGRATION.md#6-runtime-order) |
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
