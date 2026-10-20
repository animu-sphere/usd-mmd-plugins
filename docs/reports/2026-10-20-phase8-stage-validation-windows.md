# Phase 8 Windows USDC lifetime correction (2026-10-20)

## Hosted failure

The [Windows workspace job](https://github.com/animu-sphere/usd-mmd-plugins/actions/runs/39363741354/job/118435650737)
for PR #76 at `36b2620` passed 45 of 46 tests. The new
`usdMmdFileFormat_physics_roundtrip` test reached temporary-directory cleanup
and failed with `PermissionError: [WinError 5]` deleting the final `21.usdc`.
Linux and macOS workspace jobs passed. This failure is separate from the
local installed motion-package link limitation recorded in the
[initial validation report](2026-10-20-phase8-stage-validation.md).

The loop retained the final reopened stage through both its direct variable
and the `Checker` object, keeping the USDC backing file alive during cleanup.
Windows can reject deletion while that file remains mapped. Preservation
assertions had completed; the failure concerned test resource lifetime.

## Correction and validation

Reopened-stage checks now run in a per-file helper. Its stage and checker
references leave scope before the caller deletes the exported file. Every
USDA and USDC is explicitly unlinked immediately after validation, exercising
the release requirement for all 44 exports, including the final USDC.
Cleanup errors remain test failures; no retry, cleanup suppression or test
exclusion is introduced.

On Windows x86_64 / Python 3.13.14 / OpenUSD 26.08, the focused command passes
2/2 tests and all 22 generated physics fixtures retain their previous assertions:

```powershell
ctest --test-dir build/windows-msvc -C Release `
  -R 'usdMmdFileFormat_(stage_open|physics_roundtrip)$' --output-on-failure
```

Documentation checks and their selftest also pass. Follow-up hosted results
are recorded by [PR #76 checks](https://github.com/animu-sphere/usd-mmd-plugins/pull/76/checks).
