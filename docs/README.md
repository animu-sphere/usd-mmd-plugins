# usd-mmd-plugins documentation

Each fact has one owner. Use the capability matrix for support claims and
the current roadmap for active work; contracts define behavior, reports
preserve evidence, and release records describe shipped versions.

| Category | Responsibility | Entry |
| --- | --- | --- |
| [architecture/](architecture/) | Stable structure, dependency directions, package interfaces and links to pin sources | [WORKSPACE.md](architecture/WORKSPACE.md) · [DEPENDENCIES.md](architecture/DEPENDENCIES.md) · [PACKAGE_CONTRACT.md](architecture/PACKAGE_CONTRACT.md) |
| [design/](design/) | Semantics, public contracts and acceptance policy | [DESIGN_POLICY.md](design/DESIGN_POLICY.md) |
| [guides/](guides/) | Reproducible user tasks | [building.md](guides/building.md) · [opening.md](guides/opening.md) · [inspecting.md](guides/inspecting.md) · [exporting.md](guides/exporting.md) |
| [reference/](reference/) | Current feature support, diagnostics and source mapping | [CAPABILITY_MATRIX.md](reference/CAPABILITY_MATRIX.md) · [DIAGNOSTICS.md](reference/DIAGNOSTICS.md) · [SOURCE_MAPPING.md](reference/SOURCE_MAPPING.md) |
| [roadmap/](roadmap/) | Active gates and separately contingent ideas with promotion triggers | [current.md](roadmap/current.md) · [backlog.md](roadmap/backlog.md) |
| [reports/](reports/) | Dated experiments and decision evidence | [Report index](reports/README.md) · [Ownership audit](reports/2026-10-18-docs-ownership-audit.md) |
| [releases/](releases/) | Per-version shipped scope and release procedure | [README.md](releases/README.md) |
| [contributing/](contributing/) | Maintenance rules | [documentation.md](contributing/documentation.md) · [release note template](contributing/RELEASE_NOTES_TEMPLATE.md) |

## Canonical contracts

Focused contracts win over the general design policy in their own area.
Keep the existing owner instead of creating a synonymous contract.

| Area | Owner |
| --- | --- |
| Authored stage, coordinates, skeleton and metadata | [STAGE_CONTRACT.md](design/STAGE_CONTRACT.md) |
| PMX bytes and canonical semantics | [PMX_CONTRACT.md](design/PMX_CONTRACT.md) |
| Material schema, fallbacks and rendering interface | [MATERIAL_POLICY.md](design/MATERIAL_POLICY.md) |
| Text, names, identifiers and texture paths | [TEXT_ENCODING_POLICY.md](design/TEXT_ENCODING_POLICY.md) |
| VMD, IK, motion conversion and adapters | [MOTION_CONTRACT.md](design/MOTION_CONTRACT.md) |
| MMD physics conversion, runtime boundary and L1–L4 acceptance | [PHYSICS_INTEGRATION.md](design/PHYSICS_INTEGRATION.md) |
| USDZ export and portability | [PACKAGING_POLICY.md](design/PACKAGING_POLICY.md) |

## Source-of-truth rules

- Code and fixtures establish implemented behavior; the
  [capability matrix](reference/CAPABILITY_MATRIX.md) summarizes it once.
- [current.md](roadmap/current.md) alone owns current work status.
- [backlog.md](roadmap/backlog.md) owns optional ideas and promotion triggers,
  without dates or release promises.
- Contracts hold semantics and stable decisions, without a running progress log.
- Dated reports hold measurements and environment details; later findings
  get new reports. The [ownership audit](reports/2026-10-18-docs-ownership-audit.md)
  is a historical snapshot of the earlier consolidation.
- [Release records](releases/README.md) own shipped scope.
- Build manifests and pin modules own exact dependency versions and digests.
- [Documentation guidelines](contributing/documentation.md) define the change
  matrix and lightweight checks.
