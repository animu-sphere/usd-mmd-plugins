# Current roadmap

> Scope: unfinished, owned work only. Updated: 2026-10-31.

## Focus

Continue MMD owner/runtime/renderer integration toward representative avatar
acceptance under the [L1–L4 policy](../design/PHYSICS_INTEGRATION.md#12-fidelity-and-acceptance-policy).
The [capability matrix](../reference/CAPABILITY_MATRIX.md) owns implementation
claims, [reports](../reports/README.md) retain completed checks, and the
[backlog](backlog.md) owns contingent ideas. Phase numbers identify scope;
P1/P2 identify active priority.

## Active gates

States: `planned`, `in_progress`, `blocked`. Keep a blocked gate here only
while it has an owned unblock action; otherwise move it to the backlog.
Remove a gate when its exit evidence is accepted, retaining its contract
and recording results in a dated report and the relevant release record.

| ID | Outcome / exit gate | MMD-local responsibility | External owner / dependency | State | Exit evidence |
| --- | --- | --- | --- | --- | --- |
| P1-C (Phase 8 / PR-C) | Application/stage lifecycle, automatic binding, selective unload, teardown and resident rendering beyond a scoped host | Participant adapter/contract and lifecycle tests; keep the importer physics-free | `usd-avatar-runtime` orchestration, `usd-stage-runner` host loop, `hydra-toon` resident consumption | in_progress | [Runtime order](../design/PHYSICS_INTEGRATION.md#6-runtime-order), [MVP](../design/PHYSICS_INTEGRATION.md#101-coupling-mvp-acceptance) and [interactive acceptance](../design/PHYSICS_INTEGRATION.md#124-interactive-host-acceptance); reproducible lifecycle/host report |
| P1-2 (Phase 8) | Licensed fixed PMX/VMD scene set, automatic L3 assertions and reproducible acceptance report | Fixture admission/licensing, MMD assertions and evidence; distinguish private inputs from redistributable CI assets | Runtime/physics integration and `hydra-toon` rendering | planned | [Representative scene policy](../design/PHYSICS_INTEGRATION.md#123-representative-scene-acceptance); measured bounds and final-render consumption for the selected corpus |
| P1-3 (Phase 8) | Representative material morph and combined renderer parity, advanced UV and vertex-color acceptance | Semantic correctness, immutable snapshots and canonical material/geometry checks | `usd-avatar-runtime` consumers and `hydra-toon` renderer | in_progress | [Material morphs](../design/MATERIAL_POLICY.md#11-material-morphs), [renderer boundary](../design/MATERIAL_POLICY.md#12-rendering-and-integration-belong-elsewhere); representative Hydra/direct evidence with sampled-tint scope stated separately |
| P2-1 (Phase 10) | Packaged USDZ portability and installed-user workflow accepted as a release contract | Verify installed export/reopen, texture resolution, validation and ASCII archive paths; align consumer API and release notes | None unless verification discovers a dependency | planned | [Packaging contract](../design/PACKAGING_POLICY.md), [export guide](../guides/exporting.md) and [release verification](../releases/v0.3.0.md#verification-matrix); remove this gate if release verification closes it |

## Avatar runtime composition

Acceptance definitions remain in
[PHYSICS_INTEGRATION §12.3–12.4](../design/PHYSICS_INTEGRATION.md#123-representative-scene-acceptance)
and [MATERIAL_POLICY §11–12](../design/MATERIAL_POLICY.md#11-material-morphs).
Generated fixtures and scoped private visual reviews are bounded evidence;
they do not close representative L3 or broader lifecycle gates. Use the
[report index](../reports/README.md) for completed integration checkpoints.

Runtime composition follows the
[owner continuation boundary](../design/MOTION_CONTRACT.md#1110-intended-physics-continuation-boundary)
and the [testing policy](../design/PHYSICS_INTEGRATION.md#10-testing).
Generic backend work belongs to `usd-physics-plugins`; package/ABI and
publication requirements belong to
[DEPENDENCIES §7](../architecture/DEPENDENCIES.md#7-usd-physics-plugins).
This roadmap tracks MMD-local deliverables and cross-repository exit evidence,
not the implementation queues of those owners.

### Next integration work

Use the [active gates](#active-gates) for unfinished work and the
[report index](../reports/README.md) for completed checkpoints. These headings
retain existing report links without a second progress narrative.

## Release boundary

The [v0.3.0 candidate](../releases/v0.3.0.md) is an integration checkpoint.
Passing its bounded release checks does not complete P1-C/P1-2/P1-3 or claim
arbitrary-model parity, all UV channels, unrestricted lifecycle, Bullet
trajectory equivalence or production realtime performance. Package versions
and the authored stage-contract version are independent.

Subsequent integration milestones follow the open gates above. A possible
v0.4.0 is not a dated commitment and does not depend on a neighboring
repository's unpublished version. Optional work is promoted from the
[backlog](backlog.md) only when its trigger and owner are concrete.
