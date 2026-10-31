# Contingent backlog

These ideas are not active work or release promises. They have no assigned
date, priority or version. Promote an item to [current.md](current.md) only
when its trigger is met, an owner accepts an actionable deliverable and its
exit evidence is defined. Remove or mark the backlog entry as promoted in
the same change; do not maintain a second active roadmap here.

Contracts retain semantic questions and architectural reservations. This page
owns only the trigger for taking up the work; the
[capability matrix](../reference/CAPABILITY_MATRIX.md) owns present support.

| ID | Idea / desired outcome | Trigger for promotion | Owner / dependency | Disposition |
| --- | --- | --- | --- | --- |
| P2-2 | Shared expression semantics and MMD morph mapping | A stable shared expression contract is published and a concrete consumer needs it | `usd-motion-plugins` semantics; MMD mapping here under [MOTION §10.7](../design/MOTION_CONTRACT.md#107-morphs-as-channels) | Blocked; moved from current, with no active unblock action |
| MOT-O2 | Open a model-free VMD as a USD stage | A concrete reader requires standalone source tracks and the shared motion-stage contract admits them | `usd-motion-plugins` plus the first reader; semantic question stays in [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) | Contingent; keep the existing motion library/inspection path meanwhile |
| Shared packaging extraction | Reuse proven packaging behavior across formats | A second working packager establishes common behavior and tests | MMD export and `usd-vrm-plugins`; [PACKAGING §16](../design/PACKAGING_POLICY.md#16-later) | Deferred; no empty shared package |
| MOT-O11 | Compare keyed-knee initialization with MMD output | A reproducible visible defect or trustworthy reference output justifies comparison | MMD control; bounded question and current zero-angle rule stay in [MOTION §9](../design/MOTION_CONTRACT.md#9-open-questions) and [§11.7](../design/MOTION_CONTRACT.md#117-ik) | Research; no current rule change |
| Physics fidelity | Additional spring/solver/contact fidelity | An observed representative, user-visible regression or explicit external contract requires it | MMD conversion here; neutral solver work in `usd-physics-plugins`; [acceptance policy](../design/PHYSICS_INTEGRATION.md#12-fidelity-and-acceptance-policy) | Deferred; no blanket Bullet equivalence target |
| Format breadth | PMD/PMM or other new-format interoperability | Concrete use case, maintenance estimate and admissible fixtures | Format owner to be established; [reserved identities](../architecture/WORKSPACE.md#12-later-only-when-their-responsibility-is-real) | Not planned for the immediate cycle |
| Broader performance sweeps | Wider platform/timestep/corpus measurements | A supported deployment or measurable regression needs the sweep | Runtime/renderer and relevant backend owners; [testing policy](../design/PHYSICS_INTEGRATION.md#10-testing) | Contingent; routine release lanes remain required |

Renderer-private common code and a possible common material API remain subject
to the two-working-adapter admission rule in
[MATERIAL_POLICY §12](../design/MATERIAL_POLICY.md#12-rendering-and-integration-belong-elsewhere).
Packaging options retain their individual use-case triggers in
[PACKAGING §16](../design/PACKAGING_POLICY.md#16-later).
