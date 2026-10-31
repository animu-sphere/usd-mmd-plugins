# CI cost baseline (2026-10-31)

This is the pre-change measurement for the [CI execution policy](../architecture/CI_POLICY.md).
It measures the latest 30 distinct PR head revisions from the first 100
`pull_request` workflow runs returned by the GitHub API: 99 completed runs,
from 2026-10-21 22:26:11 JST through 2026-10-31 16:07:38 JST.
Multiple revisions of a PR can be in this sample; these are not 30 unique PRs.

| Workflow | Completed runs | Failed runs | Wall p50 (s) | Wall p95 (s) | Job execution p50 (runner minutes) | Jobs per run |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| docs-check | 30 | 0 | 11 | 18 | 0.10 | 1 |
| ost source ci | 30 | 1 | 338.5 | 1007 | 24.03 | 13 |
| installed physics | 30 | 0 | 442.5 | 480 | 7.28 | 1 |
| parser-sanitizers | 9 | 0 | 380 | 401 | 6.28 | 1 |

Wall time is workflow `updated_at - created_at`, including initial queue and
finalization. Runner minutes sum each job's `completed_at - started_at` and
divide by 60; these are raw execution minutes, without billing multipliers or
per-job billing rounding. p95 uses the nearest-rank method. All completed
conclusions remain in the sample, including the source failure.

The API queries were `actions/runs?event=pull_request&per_page=100`, followed
by `actions/runs/{id}/jobs?per_page=100` for each selected run, in
`animu-sphere/usd-mmd-plugins`. The raw local audit is retained as
`build/ci-audit/baseline.json`. Representative latest runs are
[source](https://github.com/animu-sphere/usd-mmd-plugins/actions/runs/41179352310),
[physics](https://github.com/animu-sphere/usd-mmd-plugins/actions/runs/41179352303)
and [docs](https://github.com/animu-sphere/usd-mmd-plugins/actions/runs/41179352286).
The oldest sampled run is
[39607972781](https://github.com/animu-sphere/usd-mmd-plugins/actions/runs/39607972781).

This does not measure cache-hit rates, individual test durations, duplicate test
IDs, isolated queue times or post-change performance. Compare representative
two-week windows after rollout before claiming an improvement rate or reducing
code coverage. The implementation has not yet produced a hosted docs-only run.
