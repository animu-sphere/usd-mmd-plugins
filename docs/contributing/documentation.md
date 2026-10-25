# Documentation guidelines

Each fact has one owner. Change that owner and link to it from other pages;
do not copy status, measurements or dependency pin values.

## Category ownership

| Category | Owns | Excludes |
| --- | --- | --- |
| Root README and `docs/README.md` | Entry points, quick start and links | Detailed support tables, progress logs and benchmark results |
| `architecture/` | Stable component identities, dependency directions, build/package interfaces; links to authoritative manifests | Release progress and copies of pin values |
| `design/` | Semantics, public contracts, conversion rules and acceptance policy | Current task status, running test results and completion diaries |
| `reference/` | One capability matrix, diagnostics and source mapping | Parallel current-status summaries or task plans |
| `roadmap/current.md` | Active priorities, owners, external dependencies and exit gates | Long completion history, design text and benchmark tables |
| `guides/` | Reproducible tasks with commands that have been run | Untested command recipes |
| `releases/` | Shipped support, breaking changes and user impact per version | Future plans as a substitute for the roadmap |
| `reports/` | Dated evidence, environment, SHA, measurements, comparisons and decisions | A rolling latest-status summary |
| `contributing/` | Maintenance rules and templates | End-user tasks |

The [ownership audit](../reports/2026-10-18-docs-ownership-audit.md) classifies
the existing tree and records the consolidation's evidence links.

## One source of truth per fact

- Phase meanings: [DESIGN_POLICY §14](../design/DESIGN_POLICY.md#14-phases).
- Authored stage: [STAGE_CONTRACT](../design/STAGE_CONTRACT.md).
- MMD physics conversions and L1–L4 policy:
  [PHYSICS_INTEGRATION](../design/PHYSICS_INTEGRATION.md).
  Do not add a second `PHYSICS_CONTRACT.md`.
- Structure and dependency edges: [WORKSPACE](../architecture/WORKSPACE.md);
  required external interfaces: [DEPENDENCIES](../architecture/DEPENDENCIES.md).
  Version ranges/digests live in manifests and pin modules linked there.
- Implemented support and limitations:
  [CAPABILITY_MATRIX](../reference/CAPABILITY_MATRIX.md).
- Current work status: [current.md](../roadmap/current.md) only.
- Shipped Phase/release mapping: [release records](../releases/README.md).

Code and fixtures establish implemented behavior. Contracts may define
accepted future behavior, but must not imply it is available; link to the
matrix for the current support claim.

## Status and evidence

Contract labels such as `proposed`, `accepted`, `binding`, `superseded`
or `rejected` describe normative force, not task progress. Binding semantics
remain binding after their completion history leaves the page.

Roadmap gates use `planned`, `in_progress` or `blocked`. Remove completed
gates; retain their contract in design, measurements in a dated report and
shipped scope in release notes. Aim for roughly 100–180 lines in current.md
without making line count or file size a CI failure.

The matrix distinguishes preservation from execution and labels approximations,
unsupported features, experimental scope and unverified claims. Never say
supported merely because a source value was preserved or a finite probe ran.
Diagnostic classification follows the owning contract; proposed diagnostic
vocabulary must not be described as an already emitted enum.

Release records and dated reports are not rewritten. A later finding gets a
new report; a one-line forward-note may link to it from an older report.
Preserve synthetic fixtures and dated evidence during reorganization.

## Change destination matrix

| Change | Required destination |
| --- | --- |
| Semantics / public API | Owning design contract plus code/tests |
| Current priority / gate / blocker | `roadmap/current.md` |
| Experiment / new measurements | New dated report, linked from current only as needed |
| Support / limitation | Capability matrix plus tests and any necessary contract change |
| Shipped behavior | Per-version release record; README links when useful |
| Dependency boundary | Owning architecture document plus build manifest |

A PR may touch several rows; each fact still appears once. For physics,
identify whether the change is MMD conversion/coupling, a generic solver API,
or runtime orchestration before adding implementation.

## Lightweight checks and review

Run `python scripts/check_docs.py --selftest` and
`python scripts/check_docs.py`. The existing docs CI lane checks relative
links/anchors, required owning entries, version mirrors and diagnostics.
Ownership lint warns about additional capability/current-status tables and
progress markers in README/design; warnings are advisory during cleanup.
Binding contract labels are allowed. No new heavyweight docs lane is needed.

Reviewers check semantic drift, unsupported support claims, copied numbers,
and large historical additions to current.md. Meaning is reviewed by people;
file size and line count are not blocking checks. Physics lane policy is
[PHYSICS_INTEGRATION §10](../design/PHYSICS_INTEGRATION.md#10-testing).

## Language and form

- Write repository documents in English, with relative repository links.
- Number design/architecture sections stably; append sections or subsections
  instead of changing existing numbers. Keep decision IDs such as MOT-O11.
- Keep indexes and owning links in sync.
- Add new evidence reports to the [report index](../reports/README.md) under
  their subject; link from the roadmap only where needed for an active gate.
- Never commit machine-local paths or third-party MMD assets/screenshots
  without explicit redistribution permission.
