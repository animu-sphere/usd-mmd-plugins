# CI execution policy

This file owns CI execution policy. [openstrata.ci.yaml](../../openstrata.ci.yaml)
owns the support cells, tool/runtime pins and verification depths. The
[workspace contract](WORKSPACE.md) owns the dependency and test responsibilities.

## Execution layers

[ci-gate.yml](../../.github/workflows/ci-gate.yml) runs on every pull request,
push to `main`, and manual dispatch, without workflow path filters. It calls
the existing workflows as reusable jobs. The release workflow remains separate.

| Change or event | Cheap checks | Source cells | Parser sanitizers/fuzz | Installed physics |
| --- | --- | --- | --- | --- |
| Added/modified documentation only | Always | Skip | Skip | Skip |
| Documentation checker only, optionally with docs | Always | Skip | Skip | Skip |
| Other PR changes | Always | All 13 | Related changes, infrastructure or unknown paths | Run |
| Other main pushes | Always | No source lane, as before | Same classification as PRs | Run |
| Manual `ci-gate` dispatch | Always | All 13 | Run | Run |
| Incomplete diff, unknown path, deletion or rename | Always | All 13 on PRs | Run | Run |

The first change implements the docs-only split. Code PRs retain three OS
workspace builds, the graph gate and all nine standalone bundle builds.
Physics keeps its three installed-consumer configurations. Sanitizers retain
all five libraries and both fuzz targets; their previous path coverage is
preserved and expanded to common infrastructure and all fixture-generator
changes. There is no test deletion or reduction of standalone/release contracts.

Cheap checks run the documentation selftest, links/anchors, mirrored versions
and diagnostics, CI policy regressions and source-workflow drift verification.
The drift check downloads only the checksum-verified, manifest-pinned OST CLI
and renders YAML. It never pulls an OpenUSD runtime or configures/builds CMake.

## Change discovery and conservative selection

[classify_ci_changes.py](../../scripts/classify_ci_changes.py) owns the path
rules. Pure documentation is an allowlist of root contributor/product documents,
`LICENSE`, and Markdown under `docs/`. A `.md` extension elsewhere, a fixture,
a script under `docs/`, a version or a manifest is not sufficient. The current
`docs/reference` files are Markdown checked by `check_docs.py`; executable and
generated test inputs are outside the allowlist. Any future generated reference
file that requires a build must be removed from this allowlist when introduced.

All changed files must qualify before heavy jobs are skipped. Adding docs to a
code change cannot make it docs-only. `scripts/check_docs.py` is separately
allowed to use just the cheap checks, without reporting `docs_only=true`.

PR discovery uses the read-only GitHub PR files API in pages of 100, checks
the complete file count and uniqueness, and checks the PR head/base revisions
before and after pagination. API errors, revision races, truncation and lists
over the API's 3,000-file limit select full CI. Fork PRs receive no secrets.
Only the discovery job has `pull-requests: read`; other jobs use `contents: read`.
The [GitHub PR API contract](https://docs.github.com/en/rest/pulls/pulls#list-pull-requests-files)
defines these pagination limits.

Main pushes use the complete local `before`/`after` git diff, with full checkout
history and NUL-delimited paths. Missing revisions and failed comparisons select
all applicable main-push checks. Deletions and renames deliberately select full
CI even when both paths are Markdown. Manual dispatch always selects full CI.

## Stable result and branch protection

`required-ci` always evaluates the classification job, cheap checks and all
selected heavy jobs. A selected job must finish with `success`; `failure`,
`cancelled`, `skipped`, missing flags or invalid flags fail the gate. An
unselected job must be `skipped`. The source reusable workflow also checks that
every generated matrix job group succeeds, so an accidentally skipped group
cannot be hidden by another successful group.

Use the observed `required-ci` check as the stable required status when enabling
branch protection. Individual optional matrix/physics/sanitizer checks must not
remain required: a docs-only PR intentionally skips them. On 2026-10-31 the API
reported no classic protection on `main` and a disabled `Protect main` ruleset.
No repository protection setting was changed as part of this implementation.
Confirm the actual check names on a hosted run before enabling the rule.

PR supersession cancels the previous `ci-gate` run, including called jobs.
Main pushes/manual runs are not automatically cancelled. Release concurrency
and quality checks remain independent. Called workflows share their caller's
event context, as defined by
[GitHub reusable workflows](https://docs.github.com/en/actions/reference/workflows-and-actions/reusing-workflow-configurations).

## Generated source CI

OST 0.23.13 cannot express conditional/reusable source execution. The repository
uses [generate_source_ci.py](../../scripts/generate_source_ci.py) as a thin
renderer adapter: it invokes the exact manifest-pinned OST renderer, replaces
the PR trigger with `workflow_call`, removes the renderer's PR event guards,
and appends the source result check. Source build, test, package and artifact
steps remain renderer output. The adapter refuses unexpected triggers/guards
and requires explicit review when new source lanes are introduced.

Regenerate through the adapter after changing the matrix; never edit generated
jobs directly. Every cheap gate re-renders and compares the entire workflow.
This is a repository-local bridge until upstream OST supports orchestration.
The generated workspace's inactive pyramid step references `matrix.up_to`
although `verify: test` cells omit that key; this existing upstream output
requires a narrow actionlint exclusion for that expression when linting it.

## Validation and follow-up

The implementation is locally checked for docs-only/mixed changes, fixtures,
manifests, workflows, unknown paths, deletion/rename, pagination/truncation,
revision races, discovery failures, all selection combinations and unsuccessful
job outcomes. A hosted docs-only run and a hosted selected-job failure run still
need to confirm the GitHub scheduling/results behavior after these changes land.
Local simulations are not evidence of hosted execution or measured speedup.

Before reducing the 13-cell code matrix, inventory CTest and OST test IDs,
contracts, fixtures, owners, dependencies and measured durations. Preserve the
standalone build/package/discovery contract and the installed-consumer contract
even where semantic tests overlap. Define merge-before three-OS guarantees
before moving cells to main, a schedule or a merge queue. Narrow the physics
dependency closure and sanitizer chains only with that inventory. Release
distribution, reproducibility and installed smoke checks remain separate.
